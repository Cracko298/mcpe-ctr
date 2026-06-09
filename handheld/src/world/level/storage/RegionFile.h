#ifndef NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__
#define NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__

#include <map>
#include <string>
#include <stdio.h>
#include "../../../raknet/BitStream.h"

typedef std::map<int, bool> FreeSectorMap;

class RegionFile
{
public:
	RegionFile(const std::string& basePath);
	virtual ~RegionFile();

	bool open();
	bool readChunk(int x, int z, RakNet::BitStream** destChunkData);
	bool writeChunk(int x, int z, RakNet::BitStream& chunkData);
private:
	bool write(int sector, RakNet::BitStream& chunkData);
	void close();
	void setBuffer();

	FILE* file;
	std::string	filename;
	int* offsets;
	int* emptyChunk;
	char* ioBuffer;
	FreeSectorMap sectorFree;
};


#endif /*NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__*/
