#include "Textures.h"

#include "TextureData.h"
#include "ptexture/DynamicTexture.h"
#include "../Options.h"
#include "../../platform/time.h"
#include "../../AppPlatform.h"

#include <vector>
#include <cstring>

/*static*/ int  Textures::textureChanges = 0;
/*static*/ bool Textures::MIPMAP = false;
#ifndef _WIN32
const TextureId Textures::InvalidId;
#endif

#define glCheck(f) do { int err = glGetError(); if(err != 0) { printf("GL Error: " #f ": %d\n", err); fflush(0); exit(1); } } while(0)


namespace {
static bool isTerrainAtlasName(const std::string& resourceName) {
	const char* terrain = "terrain.png";
	const size_t terrainLen = 11;
	if (resourceName == terrain)
		return true;
	if (resourceName.length() < terrainLen)
		return false;
	return resourceName.compare(resourceName.length() - terrainLen, terrainLen, terrain) == 0;
}

static bool shouldUseMipMapsForTexture(const std::string& resourceName, const TextureData& img) {
	// Minecraft's terrain atlas is the one that benefits most from mipmaps at distance.
	// Keep this narrow so GUI/font textures stay sharp and old 3DS VRAM use stays low.
	return Textures::MIPMAP &&
		isTerrainAtlasName(resourceName) &&
		img.data != NULL &&
		img.format == TEXF_UNCOMPRESSED_8888 &&
		img.w > 1 && img.h > 1 &&
		(img.w % 16) == 0 && (img.h % 16) == 0;
}

static void average4RGBA(const unsigned char* c0, const unsigned char* c1,
						 const unsigned char* c2, const unsigned char* c3,
						 unsigned char* out) {
	const unsigned char* c[4] = { c0, c1, c2, c3 };
	int aSum = 0;
	int rWeighted = 0;
	int gWeighted = 0;
	int bWeighted = 0;
	int rPlain = 0;
	int gPlain = 0;
	int bPlain = 0;
	int aPlain = 0;

	for (int i = 0; i < 4; ++i) {
		int r = c[i][0];
		int g = c[i][1];
		int b = c[i][2];
		int a = c[i][3];
		rPlain += r;
		gPlain += g;
		bPlain += b;
		aPlain += a;
		aSum += a;
		rWeighted += r * a;
		gWeighted += g * a;
		bWeighted += b * a;
	}

	if (aSum > 0) {
		out[0] = (unsigned char)(rWeighted / aSum);
		out[1] = (unsigned char)(gWeighted / aSum);
		out[2] = (unsigned char)(bWeighted / aSum);
	} else {
		out[0] = (unsigned char)(rPlain >> 2);
		out[1] = (unsigned char)(gPlain >> 2);
		out[2] = (unsigned char)(bPlain >> 2);
	}
	out[3] = (unsigned char)(aPlain >> 2);
}

static const unsigned char* pixelAtRGBA(const unsigned char* src, int w, int h, int x, int y) {
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x >= w) x = w - 1;
	if (y >= h) y = h - 1;
	return src + ((y * w + x) << 2);
}

static void downsampleRGBA(const unsigned char* src, int srcW, int srcH,
						  unsigned char* dst, int dstW, int dstH) {
	for (int y = 0; y < dstH; ++y) {
		for (int x = 0; x < dstW; ++x) {
			int sx = x << 1;
			int sy = y << 1;
			average4RGBA(pixelAtRGBA(src, srcW, srcH, sx,     sy),
						 pixelAtRGBA(src, srcW, srcH, sx + 1, sy),
						 pixelAtRGBA(src, srcW, srcH, sx + 1, sy + 1),
						 pixelAtRGBA(src, srcW, srcH, sx,     sy + 1),
						 dst + ((y * dstW + x) << 2));
		}
	}
}

static void downsampleAtlasRGBA(const unsigned char* src, int srcW, int srcH,
							   unsigned char* dst, int dstW, int dstH,
							   int tilesX, int tilesY) {
	const int srcTileW = srcW / tilesX;
	const int srcTileH = srcH / tilesY;
	const int dstTileW = dstW / tilesX;
	const int dstTileH = dstH / tilesY;

	for (int ty = 0; ty < tilesY; ++ty) {
		for (int tx = 0; tx < tilesX; ++tx) {
			const int srcBaseX = tx * srcTileW;
			const int srcBaseY = ty * srcTileH;
			const int dstBaseX = tx * dstTileW;
			const int dstBaseY = ty * dstTileH;

			for (int y = 0; y < dstTileH; ++y) {
				for (int x = 0; x < dstTileW; ++x) {
					int sx0 = srcBaseX + (x << 1);
					int sy0 = srcBaseY + (y << 1);
					int sx1 = sx0 + 1;
					int sy1 = sy0 + 1;
					if (sx0 >= srcBaseX + srcTileW) sx0 = srcBaseX + srcTileW - 1;
					if (sy0 >= srcBaseY + srcTileH) sy0 = srcBaseY + srcTileH - 1;
					if (sx1 >= srcBaseX + srcTileW) sx1 = srcBaseX + srcTileW - 1;
					if (sy1 >= srcBaseY + srcTileH) sy1 = srcBaseY + srcTileH - 1;

					average4RGBA(pixelAtRGBA(src, srcW, srcH, sx0, sy0),
							 pixelAtRGBA(src, srcW, srcH, sx1, sy0),
							 pixelAtRGBA(src, srcW, srcH, sx1, sy1),
							 pixelAtRGBA(src, srcW, srcH, sx0, sy1),
							 dst + (((dstBaseY + y) * dstW + dstBaseX + x) << 2));
				}
			}
		}
	}
}

static void uploadGeneratedMipMaps(const std::string& resourceName, const TextureData& img, GLint mode) {
	if (!shouldUseMipMapsForTexture(resourceName, img))
		return;

	std::vector<unsigned char> prev(img.data, img.data + img.w * img.h * 4);
	int srcW = img.w;
	int srcH = img.h;
	int level = 1;
	bool atlasSafe = isTerrainAtlasName(resourceName) && (srcW % 16) == 0 && (srcH % 16) == 0;

	while (srcW > 1 || srcH > 1) {
		int dstW;
		int dstH;

		if (atlasSafe && srcW >= 16 && srcH >= 16 && (srcW % 16) == 0 && (srcH % 16) == 0 && (srcW / 16 > 1 || srcH / 16 > 1)) {
			int dstTileW = (srcW / 16) > 1 ? (srcW / 16) >> 1 : 1;
			int dstTileH = (srcH / 16) > 1 ? (srcH / 16) >> 1 : 1;
			dstW = dstTileW * 16;
			dstH = dstTileH * 16;
		} else {
			atlasSafe = false;
			dstW = srcW > 1 ? srcW >> 1 : 1;
			dstH = srcH > 1 ? srcH >> 1 : 1;
		}

		std::vector<unsigned char> next(dstW * dstH * 4);
		if (atlasSafe)
			downsampleAtlasRGBA(&prev[0], srcW, srcH, &next[0], dstW, dstH, 16, 16);
		else
			downsampleRGBA(&prev[0], srcW, srcH, &next[0], dstW, dstH);

		glTexImage2D2(GL_TEXTURE_2D, level, mode, dstW, dstH, 0, mode, GL_UNSIGNED_BYTE, &next[0]);
		glCheck(glTexImage2D2);

		prev.swap(next);
		srcW = dstW;
		srcH = dstH;
		++level;
	}
}

static void uploadDynamicTextureMipMaps(DynamicTexture* tex, int tileRepeatX, int tileRepeatY) {
	std::vector<unsigned char> prev(tex->pixels, tex->pixels + 16 * 16 * 4);
	int srcSize = 16;
	int level = 1;
	while (srcSize > 1) {
		int dstSize = srcSize >> 1;
		std::vector<unsigned char> next(dstSize * dstSize * 4);
		downsampleRGBA(&prev[0], srcSize, srcSize, &next[0], dstSize, dstSize);
		glTexSubImage2D2(GL_TEXTURE_2D, level,
			((tex->tex % 16) + tileRepeatX) * dstSize,
			((tex->tex / 16) + tileRepeatY) * dstSize,
			dstSize, dstSize, GL_RGBA, GL_UNSIGNED_BYTE, &next[0]);
		glCheck(glTexSubImage2D2);
		prev.swap(next);
		srcSize = dstSize;
		++level;
	}
}
}

Textures::Textures( Options* options_, AppPlatform* platform_ )
:	clamp(false),
	blur(false),
	options(options_),
	platform(platform_),
	lastBoundTexture(Textures::InvalidId)
{
}

Textures::~Textures()
{
	clear();

	for (unsigned int i = 0; i < dynamicTextures.size(); ++i)
		delete dynamicTextures[i];
}

void Textures::clear()
{
	for (TextureMap::iterator it = idMap.begin(); it != idMap.end(); ++it) {
		if (it->second != Textures::InvalidId)
			glDeleteTextures(1, &it->second);
	}
	for (TextureImageMap::iterator it = loadedImages.begin(); it != loadedImages.end(); ++it) {
		if (!(it->second).memoryHandledExternally)
			delete[] (it->second).data;
	}
	idMap.clear();
	loadedImages.clear();

	lastBoundTexture = Textures::InvalidId;
}

TextureId Textures::loadAndBindTexture( const std::string& resourceName )
{
	//static Stopwatch t;

	//t.start();
	TextureId id = loadTexture(resourceName);
	//t.stop();
	if (id != Textures::InvalidId)
		bind(id);

	//t.printEvery(1000);

	return id;
}

TextureId Textures::loadTexture( const std::string& resourceName, bool inTextureFolder /* = true */ )
{
	TextureMap::iterator it = idMap.find(resourceName);
	if (it != idMap.end())
		return it->second;

	TextureData texdata = platform->loadTexture(resourceName, inTextureFolder);
	if (texdata.data)
		return assignTexture(resourceName, texdata);
    else if (texdata.identifier != InvalidId) {
        //LOGI("Adding id: %d for %s\n", texdata.identifier, resourceName.c_str());
		idMap.insert(std::make_pair(resourceName, texdata.identifier));
    }
	else {
		idMap.insert(std::make_pair(resourceName, Textures::InvalidId));
		//loadedImages.insert(std::make_pair(InvalidId, texdata));
	}
	return Textures::InvalidId;
}

TextureId Textures::assignTexture( const std::string& resourceName, const TextureData& img )
{
	TextureId id;
	glGenTextures(1, &id);

	bind(id);

	const bool useMipMaps = shouldUseMipMapsForTexture(resourceName, img);
	if (useMipMaps) {
		// Keep block textures pixel-crisp while still selecting smaller mip levels at distance.
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	if (blur) {
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (clamp) {
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	} else {
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri2(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

    switch (img.format)
    {
        case TEXF_COMPRESSED_PVRTC_4444:
        case TEXF_COMPRESSED_PVRTC_565:
        case TEXF_COMPRESSED_PVRTC_5551:
        {
#if defined(__APPLE__) || defined(__VITA__) || defined(__SWITCH__) || defined(__3DS__)
            int fmt = img.transparent? GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG : GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, fmt, img.w, img.h, 0, img.numBytes, img.data);
#endif
            break;
        }

        default:
            const GLint mode = img.transparent? GL_RGBA : GL_RGB;

            if (img.format == TEXF_UNCOMPRESSED_565) {
                glTexImage2D2(GL_TEXTURE_2D, 0, mode, img.w, img.h, 0, mode, GL_UNSIGNED_SHORT_5_6_5, img.data);
            }
            else if (img.format == TEXF_UNCOMPRESSED_4444) {
//#ifdef __3DS__
//                glTexImage2D2(GL_TEXTURE_2D, 0, mode, img.w, img.h, 0, mode, GL_UNSIGNED_SHORT_4_4_4_4_REV, img.data);
//#else
                glTexImage2D2(GL_TEXTURE_2D, 0, mode, img.w, img.h, 0, mode, GL_UNSIGNED_SHORT_4_4_4_4, img.data);
//#endif
            }
            else if (img.format == TEXF_UNCOMPRESSED_5551) {
                glTexImage2D2(GL_TEXTURE_2D, 0, mode, img.w, img.h, 0, mode, GL_UNSIGNED_SHORT_5_5_5_1, img.data);
            }
            else {
                glTexImage2D2(GL_TEXTURE_2D, 0, mode, img.w, img.h, 0, mode, GL_UNSIGNED_BYTE, img.data);
				if (useMipMaps)
					uploadGeneratedMipMaps(resourceName, img, mode);
            }
			glCheck(glTexImage2D2);
            break;
    }

    LOGI("Adding id: %d to map\n", id);
	idMap.insert(std::make_pair(resourceName, id));
	loadedImages.insert(std::make_pair(id, img));

	return id;
}

const TextureData* Textures::getTemporaryTextureData( TextureId id )
{
	TextureImageMap::iterator it = loadedImages.find(id);
	if (it == loadedImages.end())
		return NULL;

	return &it->second;
}

void Textures::tick(bool uploadToGraphicsCard)
{
	for (unsigned int i = 0; i < dynamicTextures.size(); ++i ) {
		DynamicTexture* tex = dynamicTextures[i];
		tex->tick();

        if (uploadToGraphicsCard) {
            tex->bindTexture(this);
		    for (int xx = 0; xx < tex->replicate; xx++) {
				for (int yy = 0; yy < tex->replicate; yy++) {
					glTexSubImage2D2(GL_TEXTURE_2D, 0, tex->tex % 16 * 16 + xx * 16,
						tex->tex / 16 * 16 + yy * 16, 16, 16,
						GL_RGBA, GL_UNSIGNED_BYTE, tex->pixels);
					glCheck(glTexSubImage2D2);
					if (MIPMAP)
						uploadDynamicTextureMipMaps(tex, xx, yy);
				}
			}
        }
	}
}

void Textures::addDynamicTexture( DynamicTexture* dynamicTexture )
{
	dynamicTextures.push_back(dynamicTexture);
	dynamicTexture->tick();
}

void Textures::reloadAll()
{
	//TexturePack skin = skins.selected;

	//for (int id : loadedImages.keySet()) {
	//    BufferedImage image = loadedImages.get(id);
	//    loadTexture(image, id);
	//}

	////for (HttpTexture httpTexture : httpTextures.values()) {
	////    httpTexture.isLoaded = false;
	////}

	//for (std::string name : idMap.keySet()) {
	//    try {
	//        BufferedImage image;
	//        if (name.startsWith("##")) {
	//            image = makeStrip(readImage(skin.getResource(name.substring(2))));
	//        } else if (name.startsWith("%clamp%")) {
	//            clamp = true;
	//            image = readImage(skin.getResource(name.substring(7)));
	//        } else if (name.startsWith("%blur%")) {
	//            blur = true;
	//            image = readImage(skin.getResource(name.substring(6)));
	//        } else {
	//            image = readImage(skin.getResource(name));
	//        }
	//        int id = idMap.get(name);
	//        loadTexture(image, id);
	//        blur = false;
	//        clamp = false;
	//    } catch (IOException e) {
	//        e.printStackTrace();
	//    }
	//}
}

int Textures::smoothBlend( int c0, int c1 )
{
	int a0 = (int) (((c0 & 0xff000000) >> 24)) & 0xff;
	int a1 = (int) (((c1 & 0xff000000) >> 24)) & 0xff;
	return ((a0 + a1) >> 1 << 24) + (((c0 & 0x00fefefe) + (c1 & 0x00fefefe)) >> 1);
}

int Textures::crispBlend( int c0, int c1 )
{
	int a0 = (int) (((c0 & 0xff000000) >> 24)) & 0xff;
	int a1 = (int) (((c1 & 0xff000000) >> 24)) & 0xff;

	int a = 255;
	if (a0 + a1 == 0) {
		a0 = 1;
		a1 = 1;
		a = 0;
	}

	int r0 = ((c0 >> 16) & 0xff) * a0;
	int g0 = ((c0 >> 8) & 0xff) * a0;
	int b0 = ((c0) & 0xff) * a0;

	int r1 = ((c1 >> 16) & 0xff) * a1;
	int g1 = ((c1 >> 8) & 0xff) * a1;
	int b1 = ((c1) & 0xff) * a1;

	int r = (r0 + r1) / (a0 + a1);
	int g = (g0 + g1) / (a0 + a1);
	int b = (b0 + b1) / (a0 + a1);

	return (a << 24) | (r << 16) | (g << 8) | b;
}

///*public*/ int loadHttpTexture(std::string url, std::string backup) {
//    HttpTexture texture = httpTextures.get(url);
//    if (texture != NULL) {
//        if (texture.loadedImage != NULL && !texture.isLoaded) {
//            if (texture.id < 0) {
//                texture.id = getTexture(texture.loadedImage);
//            } else {
//                loadTexture(texture.loadedImage, texture.id);
//            }
//            texture.isLoaded = true;
//        }
//    }
//    if (texture == NULL || texture.id < 0) {
//        if (backup == NULL) return -1;
//        return loadTexture(backup);
//    }
//    return texture.id;
//}

//HttpTexture addHttpTexture(std::string url, HttpTextureProcessor processor) {
//    HttpTexture texture = httpTextures.get(url);
//    if (texture == NULL) {
//        httpTextures.put(url, /*new*/ HttpTexture(url, processor));
//    } else {
//        texture.count++;
//    }
//    return texture;
//}

//void removeHttpTexture(std::string url) {
//    HttpTexture texture = httpTextures.get(url);
//    if (texture != NULL) {
//        texture.count--;
//        if (texture.count == 0) {
//            if (texture.id >= 0) releaseTexture(texture.id);
//            httpTextures.remove(url);
//        }
//    }
//}

//void tick() {
//	for (int i = 0; i < dynamicTextures.size(); i++) {
//		DynamicTexture dynamicTexture = dynamicTextures.get(i);
//		dynamicTexture.anaglyph3d = options.anaglyph3d;
//		dynamicTexture.tick();
//
//		pixels.clear();
//		pixels.put(dynamicTexture.pixels);
//		pixels.position(0).limit(dynamicTexture.pixels.length);
//
//		dynamicTexture.bindTexture(this);
//
//		for (int xx = 0; xx < dynamicTexture.replicate; xx++)
//			for (int yy = 0; yy < dynamicTexture.replicate; yy++) {
//
//				glTexSubImage2D2(GL_TEXTURE_2D, 0, dynamicTexture.tex % 16 * 16 + xx * 16, dynamicTexture.tex / 16 * 16 + yy * 16, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//				if (MIPMAP) {
//					for (int level = 1; level <= 4; level++) {
//						int os = 16 >> (level - 1);
//						int s = 16 >> level;
//
//						for (int x = 0; x < s; x++)
//							for (int y = 0; y < s; y++) {
//								int c0 = pixels.getInt(((x * 2 + 0) + (y * 2 + 0) * os) * 4);
//								int c1 = pixels.getInt(((x * 2 + 1) + (y * 2 + 0) * os) * 4);
//								int c2 = pixels.getInt(((x * 2 + 1) + (y * 2 + 1) * os) * 4);
//								int c3 = pixels.getInt(((x * 2 + 0) + (y * 2 + 1) * os) * 4);
//								int col = smoothBlend(smoothBlend(c0, c1), smoothBlend(c2, c3));
//								pixels.putInt((x + y * s) * 4, col);
//							}
//							glTexSubImage2D2(GL_TEXTURE_2D, level, dynamicTexture.tex % 16 * s, dynamicTexture.tex / 16 * s, s, s, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//					}
//				}
//			}
//	}
//
//	for (int i = 0; i < dynamicTextures.size(); i++) {
//		DynamicTexture dynamicTexture = dynamicTextures.get(i);
//
//		if (dynamicTexture.copyTo > 0) {
//			pixels.clear();
//			pixels.put(dynamicTexture.pixels);
//			pixels.position(0).limit(dynamicTexture.pixels.length);
//			glBindTexture2(GL_TEXTURE_2D, dynamicTexture.copyTo);
//			glTexSubImage2D2(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//			if (MIPMAP) {
//				for (int level = 1; level <= 4; level++) {
//					int os = 16 >> (level - 1);
//					int s = 16 >> level;
//
//					for (int x = 0; x < s; x++)
//						for (int y = 0; y < s; y++) {
//							int c0 = pixels.getInt(((x * 2 + 0) + (y * 2 + 0) * os) * 4);
//							int c1 = pixels.getInt(((x * 2 + 1) + (y * 2 + 0) * os) * 4);
//							int c2 = pixels.getInt(((x * 2 + 1) + (y * 2 + 1) * os) * 4);
//							int c3 = pixels.getInt(((x * 2 + 0) + (y * 2 + 1) * os) * 4);
//							int col = smoothBlend(smoothBlend(c0, c1), smoothBlend(c2, c3));
//							pixels.putInt((x + y * s) * 4, col);
//						}
//						glTexSubImage2D2(GL_TEXTURE_2D, level, 0, 0, s, s, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//				}
//			}
//		}
//	}
//}
//  void releaseTexture(int id) {
//      loadedImages.erase(id);
//      glDeleteTextures(1, (const GLuint*)&id);
//  }

