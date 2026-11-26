# Surreal Engine for MiSTer FPGA (work in progress)

This is a fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), the reimplementation of Unreal Engine 1 by Magnus Norddahl, Lupert Everett and contributors. The fork's goal is to run Unreal Tournament (UT99) on a [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer/wiki) as a **hybrid core**: the engine runs on the MiSTer's ARM CPU, and a small FPGA core gives it native 15kHz video, sound and the MiSTer's controllers, through danik's [Hybrid Core Framework](https://github.com/ItsDanik/Hybrid_MiSTer).

Surreal Engine draws through Vulkan, OpenGL or Direct3D 11. The MiSTer's ARM (a Cortex-A9) has no GPU, so this fork adds a renderer that needs none.

## Status

**Experimental. It has not yet run with game data, and not on a MiSTer.**

What has been checked:

- The engine builds for x86-64 and for 32-bit ARM (Cortex-A9) with `-DMISTER_HYBRID=ON`.
- The software render device passes its own test (`SoftwareRenderTest`, synthetic polygons) on both, the ARM build under emulation, with identical frames.
- Without game data, the engine shows its message on the framework's stand-in for the FPGA core and exits.

What is not known: whether a map is drawn correctly, whether sound plays, and how fast any of it is on the MiSTer's 800MHz CPU. The open items are listed in [BACKLOG.md](https://github.com/WindyCityJess/SurrealEngine_MiSTer/blob/main/BACKLOG.md).

## The two branches

| Branch | |
|---|---|
| [`mister`](https://github.com/WindyCityJess/SurrealEngine_MiSTer/tree/mister) (this one) | The engine with the changes for the MiSTer |
| [`main`](https://github.com/WindyCityJess/SurrealEngine_MiSTer/tree/main) | The MiSTer core: the FPGA core, build and package scripts, its [README](https://github.com/WindyCityJess/SurrealEngine_MiSTer/blob/main/README.md), notes and backlog. This branch is its `surrealengine/` submodule |

To build the port, clone `main` with its submodules:

```sh
git clone --recursive -b main https://github.com/WindyCityJess/SurrealEngine_MiSTer
```

## What this fork adds

All of it is behind the CMake option `MISTER_HYBRID`. Without the option the engine builds as upstream's does.

- **Software render device** (`SurrealEngine/RenderDevice/Software/`): every draw call of the engine's `RenderDevice` interface rasterized on the CPU. Perspective correct texturing, lightmaps and fog maps, masked, translucent and modulated polygons, vertex light and fog, tiles and lines. No stencil yet (portals and mirrors), no macro or detail textures.
- **A build without GPU or desktop libraries**: no Vulkan, OpenGL, X11, Wayland, D-Bus, GTK, fontconfig, OpenAL or ALSA, and no editor or debugger. SDL2 alone provides the window, sound and input.
- **Sound through SDL** (`SurrealEngine/Audio/AudioPlayer/SDLAudioPlayer.cpp`), from the engine's own mixer.
- **Gamepad** (`SurrealEngine/MiSTer/MisterGamepad.cpp`): the MiSTer's joystick 1 on the engine's joystick keys, with bindings of its own. On an 8BitDo Pro style pad: left stick walks and strafes, right stick turns and looks, R or R2 fires, L or L2 is alt fire, B jumps, A ducks, X and Y change weapon, Select shows the scores, Start opens the menu.
- **MiSTer support code** (`SurrealEngine/MiSTer/mister_port.c`): one frame for every field of the core's picture, the message when no game is found, the exit code.
- **No launcher window**: the game is the folder named on the command line. The window is 320x240.
- **`Tools/SoftwareRenderTest`**: the render device without a window or game data. It draws a small scene, checks pixels whose values are known and writes the frame as a PPM.
- **32-bit ARM**: the NEON light code builds for ARMv7, which lacks some of the instructions it used.

## Building the engine alone

For the MiSTer, use the `main` branch's `build.sh`, which builds SDL2 with the framework's drivers first. To try the software render device on a PC, with SDL2 installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMISTER_HYBRID=ON -DENABLE_SDL3=OFF
cmake --build build --target SurrealEngine SoftwareRenderTest
build/SoftwareRenderTest out.ppm
```

Command line: `SurrealEngine [--url=<mapname>] [--engineversion=X] <path to game folder>`. The game folder is the one with the game's `System` folder in it. Unreal Tournament v436 is the version the engine runs best.

Upstream's own documents are unchanged: [Docs/Status.md](Docs/Status.md) (what works in which game), [Docs/Building.md](Docs/Building.md) and [Docs/PackageFormat.md](Docs/PackageFormat.md).

## Relation to upstream

These changes were written mainly with AI assistance (Claude). Surreal Engine asks that such changes are not sent to it as pull requests ([NO-AI Code Rule.md](NO-AI%20Code%20Rule.md)), so they stay in this fork.

Please do not report this fork's problems to Surreal Engine or ask its authors for help with it: bring them to this repository instead. This fork is not associated with or endorsed by Surreal Engine's authors, danik, or Epic Games, and it includes no game data.

## License

Surreal Engine's license applies to the engine and to this fork's changes to it: see [LICENSE.md](LICENSE.md), which also lists the third-party parts.
