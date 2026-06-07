# Minecraft PE for the Nintendo 3DS!

A source port of Minecraft Pocket Edition Alpha 0.6.1 targeting the Nintendo 3DS, using [NovaGL](https://github.com/efimandreev0/NovaGL) (OpenGL ES 1.1 translator) and a custom [SDL fork](https://github.com/efimandreev0/SDL-N3DS_NovaGL) optimized for 3DS via NovaGL.

[![Build 3DS MCPE](../../actions/workflows/build.yml/badge.svg)](../../actions/workflows/build.yml)

## Building

Make sure you have the latest [devkitPro](https://devkitpro.org/) toolchain installed (devkitARM).

### Dependencies

The following libraries are cloned and built automatically by the CI workflow, but for local builds you need to set them up manually:

| Dependency | Source | Description |
|---|---|---|
| **NovaGL** | [efimandreev0/NovaGL](https://github.com/efimandreev0/NovaGL) | OpenGL ES 1.1 translator for Nintendo 3DS |
| **SDL-N3DS_NovaGL** | [efimandreev0/SDL-N3DS_NovaGL](https://github.com/efimandreev0/SDL-N3DS_NovaGL) | Custom SDL2 fork for 3DS with NovaGL rendering backend |

### Local Build

```bash
# 1. Clone and build SDL-N3DS_NovaGL
git clone https://github.com/efimandreev0/SDL-N3DS_NovaGL.git handheld/lib/SDL-N3DS_NovaGL
mkdir -p handheld/lib/SDL-N3DS_NovaGL/build && cd handheld/lib/SDL-N3DS_NovaGL/build
cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
find . -name "*.a" -exec cp {} ../../ \;
cd ../../../..

# 2. Clone and build NovaGL
git clone --recursive https://github.com/efimandreev0/NovaGL.git handheld/lib/NovaGL
mkdir -p handheld/lib/NovaGL/build && cd handheld/lib/NovaGL/build
cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake -DCMAKE_BUILD_TYPE=Release ..
make NovaGL -j$(nproc)
find . -name "*.a" -exec cp {} ../../ \;
cd ../../../..

# 3. Build MCPE
cmake -DPUBLISH=on -B build -S handheld/project/ctr
cd build
make -j$(nproc)
```

If you want to build the demo, add the `-DDEMO=on` flag to the cmake line.

### CI / GitHub Actions

Pushing to the `3DS_NovaGL` branch (or triggering manually) will run the full build via GitHub Actions. Compiled `.3dsx`, `.cia`, and `.3ds` artifacts are uploaded automatically.

## Credits
- **Olebeck** — graphics, sound, networking
- **Li** — controls, refining options, menuing
- **Koutsie** — original options menu concept
- **PVR_PSP2 developers** (GrapheneCt) — Vita graphics backend
- **Silica** — PSV port
- **efimandreev0** — NX / 3DS port, [NovaGL](https://github.com/efimandreev0/NovaGL), [SDL-N3DS_NovaGL](https://github.com/efimandreev0/SDL-N3DS_NovaGL)

## Other Information
- `.3dsx` / `.cia` is install-and-play — no additional setup required.
- Local multiplayer over local Wi-Fi is supported, including cross-play with Android/iOS running the official APK.
- Due to the 3DS having limited IO speeds, world generation can take a while the first time.
- Save data (worlds, options, etc.) is stored in `sdmc:/3ds/minecraftpe/`.
- This is a source port and cannot be easily adapted to any other MCPE version.
