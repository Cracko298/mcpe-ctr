#ifndef SoundSystemCtr_H__
#define SoundSystemCtr_H__

#include "SoundSystem.h"
#include <3ds.h>

class SoundSystemCtr : public SoundSystem
{
public:
    SoundSystemCtr();
    virtual ~SoundSystemCtr();

    virtual bool isAvailable();
    virtual void enable(bool status);
    virtual void setListenerPos(float x, float y, float z);
    virtual void setListenerAngle(float deg);
    virtual void playAt(const SoundDesc& sound, float x, float y, float z, float volume, float pitch);

private:
    static const int NUM_VOICES = 16;

    struct Voice
    {
        ndspWaveBuf waveBuf;
        void* data;
        int dataSize;
    };

    void initVoice(int channel);
    void releaseVoice(int channel);
    int formatFor(const SoundDesc& sound) const;

    Voice voices[NUM_VOICES];
    int voiceIndex;
    bool available;
    bool enabled;
    float listenerX;
    float listenerY;
    float listenerZ;
    float listenerAngle;
};

#endif