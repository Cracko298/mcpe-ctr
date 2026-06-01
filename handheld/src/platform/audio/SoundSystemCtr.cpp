#include "SoundSystemCtr.h"
#include "../../client/sound/Sound.h"
#include "../log.h"
#include <cstring>

static int alignUp(int value, int alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

SoundSystemCtr::SoundSystemCtr()
:   voiceIndex(0),
    available(false),
    enabled(true),
    listenerX(0.0f),
    listenerY(0.0f),
    listenerZ(0.0f),
    listenerAngle(0.0f)
{
    std::memset(voices, 0, sizeof(voices));

    Result result = ndspInit();
    if (R_FAILED(result)) {
        LOGI("NDSP init failed: 0x%08lX\n", (unsigned long)result);
        return;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetClippingMode(NDSP_CLIP_SOFT);
    ndspSetMasterVol(1.0f);

    for (int i = 0; i < NUM_VOICES; i++) {
        initVoice(i);
    }

    available = true;
    LOGI("SoundSystemCtr NDSP initialized successfully.\n");
}

SoundSystemCtr::~SoundSystemCtr()
{
    if (!available) return;

    for (int i = 0; i < NUM_VOICES; i++) {
        releaseVoice(i);
    }

    ndspExit();
    available = false;
}

bool SoundSystemCtr::isAvailable()
{
    return available;
}

void SoundSystemCtr::enable(bool status)
{
    enabled = status;
}

void SoundSystemCtr::setListenerPos(float x, float y, float z)
{
    listenerX = x;
    listenerY = y;
    listenerZ = z;
}

void SoundSystemCtr::setListenerAngle(float deg)
{
    listenerAngle = deg;
}

void SoundSystemCtr::initVoice(int channel)
{
    ndspChnReset(channel);
    ndspChnInitParams(channel);
    ndspChnSetInterp(channel, NDSP_INTERP_LINEAR);

    float mix[12] = {};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(channel, mix);
}

void SoundSystemCtr::releaseVoice(int channel)
{
    ndspChnReset(channel);

    if (voices[channel].data != NULL) {
        linearFree(voices[channel].data);
        voices[channel].data = NULL;
        voices[channel].dataSize = 0;
    }

    std::memset(&voices[channel].waveBuf, 0, sizeof(voices[channel].waveBuf));
}

int SoundSystemCtr::formatFor(const SoundDesc& sound) const
{
    if (sound.byteWidth == 1) {
        return sound.channels == 2 ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_MONO_PCM8;
    }

    return sound.channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16;
}

void SoundSystemCtr::playAt(const SoundDesc& sound, float x, float y, float z, float volume, float pitch)
{
    if (!available || !enabled || !sound.isValid() || sound.size <= 0) return;
    if (sound.channels <= 0 || sound.byteWidth <= 0 || sound.numFrames <= 0) return;
    if (volume <= 0.0f) return;
    if (pitch < 0.01f) pitch = 1.0f;
    if (volume > 1.0f) volume = 1.0f;

    int channel = voiceIndex;
    voiceIndex++;
    if (voiceIndex >= NUM_VOICES) {
        voiceIndex = 0;
    }

    releaseVoice(channel);

    const int playbackBytes = sound.numFrames * sound.channels * sound.byteWidth;
    const int dataSize = alignUp(playbackBytes + 128, 0x80);
    void* data = linearAlloc(dataSize);
    if (data == NULL) {
        LOGI("NDSP sound allocation failed: %d bytes\n", dataSize);
        initVoice(channel);
        return;
    }

    std::memset(data, 0, dataSize);
    std::memcpy(data, sound.frames, playbackBytes);
    DSP_FlushDataCache(data, dataSize);

    initVoice(channel);
    ndspChnSetRate(channel, sound.frameRate * pitch);
    ndspChnSetFormat(channel, formatFor(sound));

    float mix[12] = {};
    mix[0] = volume;
    mix[1] = volume;
    ndspChnSetMix(channel, mix);

    Voice& voice = voices[channel];
    voice.data = data;
    voice.dataSize = dataSize;
    std::memset(&voice.waveBuf, 0, sizeof(voice.waveBuf));
    voice.waveBuf.data_vaddr = data;
    voice.waveBuf.nsamples = sound.numFrames;
    voice.waveBuf.looping = false;

    ndspChnWaveBufAdd(channel, &voice.waveBuf);
}
