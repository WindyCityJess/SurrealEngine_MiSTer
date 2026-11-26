# SurrealEngine for MiSTer

Unreal Tournament on [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer/wiki) as a hybrid port: work in progress.

The game runs on [Surreal Engine](https://github.com/dpjudas/SurrealEngine) by Magnus Norddahl, Lupert Everett and contributors, a reimplementation of Unreal Engine 1. The engine runs on the MiSTer's ARM CPU, and the SurrealEngine FPGA core, built on danik's [Hybrid Core Framework](https://github.com/ItsDanik/Hybrid_MiSTer), provides native 15kHz video (CRT, VGA and HDMI), 44.1kHz audio and keyboard, mouse and gamepad input.

Surreal Engine draws through Vulkan, OpenGL or Direct3D, and the MiSTer's ARM has no GPU. This port adds a **software render device** to the engine (`surrealengine/SurrealEngine/RenderDevice/Software/`) and builds it without any of the GPU code.

> **Status: experimental and untested.** The engine compiles for the MiSTer's ARM and the software render device passes its own test with synthetic polygons. It has not yet run with game data, and not on a MiSTer. Nothing is known about its speed there. See [BACKLOG.md](BACKLOG.md).
>
> **Disclaimer:** this port was written mainly with AI assistance (Claude). Surreal Engine asks that such changes are not sent to it as pull requests, so they live in this fork only. Please do not report this port's problems to Surreal Engine.

## Requirements

- **danik_hybrid_ports**, the launcher of the Hybrid Core Framework: run it once from the MiSTer's `Scripts` menu. Without it the core says "Game not running".
- **Game data**, which is not included: Unreal Tournament, version 436 (the version Surreal Engine runs best; 469c launches, later versions do not).
- **Recommended:** a gamepad with two analog sticks, like an 8BitDo Pro, or a mouse and a keyboard. The game's menus need the mouse: the gamepad does not move their pointer yet.

## Installation

There is no release yet. To try a build:

1. Build the game, the core and the package ([NOTES.md](NOTES.md)).
2. Copy the game's folders, whole and as they are, to `/media/fat/games/dnk/SurrealEngine/`: `System`, `Maps`, `Textures`, `Sounds` and `Music` (so that `System/UnrealTournament.exe` is there).
3. Load **dnk-Unreal Tournament** from the `Other` menu.

The game's log is in `/media/fat/logs/dnk/SurrealEngine/surrealengine.log`.

## OSD options

| Option | |
|---|---|
| Aspect ratio, Scale, Scandoubler Fx, Stereo Mix | as in other cores |
| FPS Counter | shows the pictures per second in the top left corner |
| HDMI Only | **Yes** if no CRT is connected |
| CRT Options | **Horizontal Size**, **Horizontal Pos**, **Vertical Pos**: fit the picture to a 15kHz screen. The HUD lines are greyed out: the port does not move the HUD yet |
| Reset | starts the game again from its beginning |
| Save Diagnostic Report | saves a report of the setup and the last 60 seconds in `games/dnk/diagnostics/` |

## Controls

- **Keyboard and mouse:** as the game's own settings have them (`System/User.ini`).
- **Gamepad** (default mapping, laid out for an 8BitDo Pro; not yet tried with a pad or the game):

| Button | Action |
|---|---|
| Left stick | Walk and strafe |
| Right stick | Turn and look up or down |
| D-pad | The cursor keys: the game's menus, and whatever the game binds them to |
| R and R2 | Fire |
| L and L2 | Alt Fire |
| B (Xbox A / PlayStation Cross) | Jump |
| A (Xbox B / PlayStation Circle) | Duck |
| X (Xbox Y / PlayStation Triangle) | Next Weapon |
| Y (Xbox X / PlayStation Square) | Prev Weapon |
| Select | Scores |
| Start | Menu (Escape) |

Change the buttons in the OSD under *Define SurrealEngine buttons*. The MiSTer menu's *Menu OK* / *Menu Back* buttons are Enter and Escape in the game's menus. The gamepad's actions are bound by the port itself, not by `User.ini`.

## Building

How the port works and how to build it: [NOTES.md](NOTES.md). What is open: [BACKLOG.md](BACKLOG.md).

## Credits

- **[Surreal Engine](https://github.com/dpjudas/SurrealEngine)** by Magnus Norddahl, Lupert Everett and contributors.
- **[Hybrid Core Framework](https://github.com/ItsDanik/Hybrid_MiSTer)** by danik: the FPGA host module, the ARM library, the SDL2 drivers, the launcher and the template this repository was started from.
- **[SDL](https://libsdl.org)** by Sam Lantinga and contributors.
- **[MiSTer](https://github.com/MiSTer-devel)** by Sorgelig and the MiSTer-devel contributors: the framework and Template_MiSTer.
- **[MiSTer Frontier](https://github.com/MiSTerOrganize/MiSTer_Frontier)** by MiSTer Organize, which inspired hybrid cores on the MiSTer and the way their game is launched.

This project is not associated with or endorsed by Epic Games, the authors of Surreal Engine, or danik. It does not include any game data.

## License

The top-level scripts, tools and documentation are licensed under the [GPL-3.0](LICENSE). The components keep their own licenses: Surreal Engine and the changes to it under its zlib-style license (`surrealengine/LICENSE.md`, which also lists its third-party parts); the part of the framework the game is linked with (`hybrid/hps/`) is GPL-2.0-or-later, the rest of `hybrid/` GPL-3.0 except where its files say otherwise; SDL and the SDL drivers in `hybrid/sdl2` are zlib; the FPGA core and the MiSTer framework are GPL-2.0 (`core/LICENSE`).
