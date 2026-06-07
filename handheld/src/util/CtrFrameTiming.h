#ifndef CTR_FRAME_TIMING_H
#define CTR_FRAME_TIMING_H

#ifdef __3DS__
#include <3ds.h>

// Lightweight per-frame timing for the 3DS performance overlay.
// Call markStart(STAGE) before and markEnd(STAGE) after each pipeline step.
// The overlay reads lastMs[] which holds the previous frame's timings.
struct CtrFrameTiming {
	enum Stage {
		INPUT = 0,
		GPU_WAIT,   // present / swap
		NET,
		LR_TICK,    // levelRenderer tick
		LVL_TICK,   // level tick
		TICK,
		LIGHTS,
		SOUND,
		PICK,
		UDC,        // updateDirtyChunks
		SETUP,
		TERRAIN0,
		TERRAIN1,
		ENTITIES,
		PARTICLES,
		WATER,
		HAND,
		GUI,
		PRESENT,
		STAGE_COUNT
	};

	static const char* names[STAGE_COUNT];
	static float curMs[STAGE_COUNT];   // accumulating current frame
	static float lastMs[STAGE_COUNT];  // snapshotted previous frame
	static u64   stageStart[STAGE_COUNT];
	static float totalMs;
	static int   fps;

	// Internal FPS counter
	static float _fpsAcc;
	static int   _fpsFrames;
	static u64   _lastFrameTick;

	static inline float ticksToMs(u64 ticks) {
		return (float)ticks * 1000.0f / (float)SYSCLOCK_ARM11;
	}

	static inline void markStart(Stage s) {
		stageStart[s] = svcGetSystemTick();
	}

	static inline void markEnd(Stage s) {
		u64 elapsed = svcGetSystemTick() - stageStart[s];
		curMs[s] += ticksToMs(elapsed);
	}

	// Call once per frame at the very end
	static void endFrame() {
		u64 now = svcGetSystemTick();
		totalMs = ticksToMs(now - _lastFrameTick);
		_lastFrameTick = now;

		// Copy current -> last, reset current
		for (int i = 0; i < STAGE_COUNT; i++) {
			lastMs[i] = curMs[i];
			curMs[i] = 0;
		}

		// FPS counter
		_fpsAcc += totalMs * 0.001f;
		_fpsFrames++;
		if (_fpsAcc >= 1.0f) {
			fps = _fpsFrames;
			_fpsFrames = 0;
			_fpsAcc -= 1.0f;
		}
	}

	static void init() {
		for (int i = 0; i < STAGE_COUNT; i++) {
			curMs[i] = 0;
			lastMs[i] = 0;
			stageStart[i] = 0;
		}
		totalMs = 0;
		fps = 0;
		_fpsAcc = 0;
		_fpsFrames = 0;
		_lastFrameTick = svcGetSystemTick();
	}
};

#else // !__3DS__

// No-op stub for non-3DS platforms
struct CtrFrameTiming {
	enum Stage {
		INPUT = 0, GPU_WAIT, NET, LR_TICK, LVL_TICK, TICK, LIGHTS, SOUND,
		PICK, UDC, SETUP, TERRAIN0, TERRAIN1, ENTITIES, PARTICLES, WATER,
		HAND, GUI, PRESENT, STAGE_COUNT
	};
	static inline void markStart(Stage) {}
	static inline void markEnd(Stage) {}
	static inline void endFrame() {}
	static inline void init() {}
};

#endif // __3DS__
#endif // CTR_FRAME_TIMING_H
