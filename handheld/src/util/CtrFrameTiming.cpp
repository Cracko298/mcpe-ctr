#ifdef __3DS__
#include "CtrFrameTiming.h"

const char* CtrFrameTiming::names[CtrFrameTiming::STAGE_COUNT] = {
	"input",
	"gpuwait",
	"net",
	"lrTk",
	"lvlTk",
	"tick",
	"lights",
	"sound",
	"pick",
	"uDC",
	"setup",
	"terrain0",
	"terrain1",
	"entities",
	"particles",
	"water",
	"hand",
	"gui",
	"present"
};

float CtrFrameTiming::curMs[CtrFrameTiming::STAGE_COUNT] = {};
float CtrFrameTiming::lastMs[CtrFrameTiming::STAGE_COUNT] = {};
u64   CtrFrameTiming::stageStart[CtrFrameTiming::STAGE_COUNT] = {};
float CtrFrameTiming::totalMs = 0;
int   CtrFrameTiming::fps = 0;
float CtrFrameTiming::_fpsAcc = 0;
int   CtrFrameTiming::_fpsFrames = 0;
u64   CtrFrameTiming::_lastFrameTick = 0;

#endif // __3DS__
