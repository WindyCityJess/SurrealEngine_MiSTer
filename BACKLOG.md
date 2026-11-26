# SurrealEngine for MiSTer: backlog

What is open, in three parts: the port itself, the engine's own known bugs (from upstream's `Docs/Status.md`), and other projects worth reading for fixes and speed.

Upstream asks that changes made mainly with LLM tools are not sent to it as pull requests (`NO-AI Code Rule.md` in dpjudas/SurrealEngine). Everything here stays in this fork.

## 1. The port

### Not tested yet

Nothing below has run with game data or on a MiSTer. Checked so far (2026-11-25): the engine builds for x86-64 and for ARMv7 (Cortex-A9, NEON); `SoftwareRenderTest` (synthetic polygons) passes on both, the ARM one under qemu's Cortex-A9, with the same frame bit for bit; without game data the engine shows its message on the framework's fake core and exits.

- [ ] First run with Unreal Tournament v436 data on a PC (`./build.sh host`, against `hybrid/tools/fakecore`)
- [ ] First run on a MiSTer; measure the frame time (`FPS Counter` in the OSD)
- [ ] FPGA core built with Quartus (`core/build_core.sh`); the template's core is unchanged apart from `CONF_STR`, `MIDI_SYNTH` 0 and `HUD_EDGES` 0
- [ ] Binary built in the framework's Docker toolchain (glibc 2.31). The ARM binary made so far came from Ubuntu's cross compiler, as a compile check only

### Video

- [ ] Draw RGB565 straight into the window surface (`SDL_MISTER_DIRECT=1`) instead of a 32-bit frame that SDL converts: saves a conversion of every frame
- [ ] Menus: UT's UWindow menus are laid out for 640x480. At 320x240 they will not fit. Options: 640x480 for menus only (interlaced at 15kHz), or the world drawn at half size into a 640x480 frame
- [ ] HUD margins (`MiSTer_HudRows()`), then `HUD_EDGES` back to 3 in `core/SurrealEngine.sv`
- [ ] 320x200 and the other modes: only 320x240 is asked for now (`SURREAL_MISTER_MODE=<w>x<h>` overrides it)
- [ ] Gamma and brightness (`RenderDevice::Brightness`, the gamma settings) are ignored

### Software render device: missing

- [ ] Stencil: `DrawStencilPolygon()` and `SetStencilRef()` do nothing, so portals and mirrors are drawn without their mask
- [ ] `SceneNode::NearClip` (the mirror's clip plane) and `Zoom` are ignored
- [ ] Macro textures and detail textures are not drawn (fog maps are)
- [ ] Compressed textures (S3TC / BC1 and up) are not decoded: the device reports only P8, BGRA8, RGB8, R5G6B5 and lightmaps as supported
- [ ] Texture filtering: nearest only; one mipmap per 16-pixel span, picked along the row
- [ ] `PF_Environment` and the wavy flags have no special handling in the device
- [ ] `UpdateTextureRect()` converts the whole texture again
- [ ] Editor hit testing (`PushHit()` / `PopHit()`): not needed for the game

### Software render device: speed

Ideas, most promising first. None is measured yet.

- [ ] Both CPUs: split the screen into two halves (rows), one thread each. Draw calls would be queued per frame and replayed by both
- [ ] Less overdraw: the engine draws through a z-buffer back to front in places. A span buffer or front-to-back order for the world (it comes out of a BSP) would skip hidden pixels before they are textured
- [ ] 16-bit frame and 16-bit depth (half the memory traffic; the DE10-nano's DDR3 is slow)
- [ ] Specialised inner loops per state (opaque + lightmap is nearly all of the world) instead of the one loop with branches
- [ ] Surface cache as in Quake: texture times lightmap built once per surface and mipmap, so the inner loop is a plain copy
- [ ] NEON: the engine's SSE2 paths (`USE_SSE2`: lightmap building, light effects, the BSP clipper, matrices) fall back to plain C on ARM. The sse2neon header could map them to NEON
- [ ] The UnrealScript VM and the lightmap builder are engine costs outside the device: profile before guessing

### Sound and input

- [ ] Sound goes through the engine's own mixer (`AudioPlayerDevice`) to SDL: not heard yet. Upstream's default device is OpenAL, which this build leaves out
- [ ] Music: tracker modules through libopenmpt on the ARM; may be a large share of the CPU
- [ ] Gamepad: buttons and sticks are not mapped to the engine's `IK_Joy*` keys yet. Keyboard and mouse arrive as SDL events
- [ ] `MiSTer_PickGame()`: one game only (the folder the launch script names). Unreal Gold would be a second entry of `package/games.txt`

### Repository

- [ ] `README.md`, `NOTES.md`, `package/games/SurrealEngine/README.txt`: the template's `TODO`s
- [ ] The game's logo (`package/logos/UnrealTournament.png`, 440x200) and the banner and screenshots in `docs/`: `package.sh` refuses without the logo

## 2. Known engine bugs (upstream)

From upstream's status page, November 2026. They are the engine's, not the port's.

### All games

- [ ] Movers: projectiles pass through some movers
- [ ] Movers: the player can get stuck on some movers when approaching from certain angles
- [ ] Movers: encroachment is not implemented, so movers do not crush anything or return to their previous position
- [ ] Mirrors and reflections are drawn a bit wrong, especially around the edges of world geometry
- [ ] Portals somewhat work, but can push players and projectiles in unexpected directions
- [ ] Semisolid brushes: usually the player falls through them as if they were not there
- [ ] Bot and scripted pawn AI is better than before but not fully working
- [ ] Bots rotate their whole body (feet off the ground) to look up and down
- [ ] Waving water textures at the end of waterfalls are drawn broken (NyLeve's Falls in Unreal, DM-ArcaneTemple in UT)
- [ ] Underwater collisions are somewhat buggy, especially in Klingon Honor Guard
- [ ] A power-up's screen tint (Invisibility, Energy Amplifier) stays, and adds up with each pick-up, until the next map
- [ ] The `viewclass` command crashes with a null dereference
- [ ] Opening a map sometimes stops with "Failed to spawn the player actor"
- [ ] Sounds can become too loud (Pulse Rifle secondary fire, the minigun)
- [ ] Inventory from loaded saves does not carry over to the next map
- [ ] Saving packages (`.u*` files, saved games) is not fully implemented
- [ ] No networking at all
- [ ] Native mods will never work, by design

### Unreal Tournament

- v436 launches; menus work and botmatches can be played. Bots behave more or less as in Unreal, and some maps miss some functions
- v469c launches; touch events do not work. Anything beyond 469c does not boot
- [ ] [469] Many new native functions are not implemented
- [ ] Shock Rifle beams are drawn glitchy
- [ ] The player can slide through some of the catwalks in the lava room of DM-Conveyor

### Unreal (Gold)

- 226 launches; menus work most of the time; single player maps and botmatches can be played; bots sometimes move and attack
- 227i launches; some menu items do not work; emitters and static meshes are not implemented. 227j is untested, 227k_14 crashes at once
- [ ] [227] Many new native functions are not implemented
- [ ] [227i] The Translator Scale option does nothing
- [ ] Some UPak native functions are not implemented
- [ ] Nali Fruit Seeds and ASMDs in a map are not drawn, but can be picked up
- [ ] ASMD tertiary fire rings are drawn wrong

### Other games

- Deus Ex 1112fm: boots to the title screen; the start of the training map can be played. Missing natives, an unfinished conversation system, a Load Game menu that locks up, missing HUD text, DataCubes and books that cannot be read
- Tactical-Ops: crashes at once (cannot find `TO_Replacer.ini`); menus and maps crash
- Klingon Honor Guard 1.1: launches. Keys cannot be bound in the game (the ini file works); starting a botmatch crashes
- Nerf Arena Blast: boots, little tested
- Every other UE1 game crashes at startup

## 3. Other projects to read

Found by search in November 2026. Only the pages named were read; none of their code has been examined or used.

| Project | What it is | Use here |
|---|---|---|
| [Panzerschrek/Square-Wheel](https://github.com/Panzerschrek/Square-Wheel) | A modern software-rendered engine (Rust) | Speed: it builds one texture per visible polygon each frame (texture times lightmap, at the right mipmap), splits the screen into regions for its threads, and draws the world back to front from the BSP without a z-buffer |
| [Quake II's software renderer, reviewed by Fabien Sanglard](https://fabiensanglard.net/quake2/quake2_software_renderer.php) | A write-up of id's renderer | Speed: the surface cache and span-based drawing named in part 1 come from this family of renderers |
| [Slipyx/UT99](https://github.com/Slipyx/UT99) | UT99's UnrealScript sources (469b), exported for reference; no licence stated | Bugs: what the game's scripts expect of the native functions the engine still lacks (movers' encroachment, bot AI, screen flashes). Reference only |
| [OldUnreal/UnrealTournamentPatches](https://github.com/OldUnreal/UnrealTournamentPatches) | The community patch for UT (releases and issue tracker; the engine source is not public) | Bugs: release notes and issues describe how 469 behaves where it differs from 436 |
| [CacoFFF/XC-UT99-Old](https://github.com/CacoFFF/XC-UT99-Old) | XC_Engine, native extensions for UT99 | Bugs: possibly its collision and path code, for the mover and semisolid problems. Not looked at beyond its name |
| [zefir1990/SurrealEngine-Emscripten](https://github.com/zefir1990/SurrealEngine-Emscripten) | A web build of SurrealEngine with an OpenGL ES 2.0 renderer in progress | Another reduced-platform build of the same engine: where its build differs from upstream's |

Not to be used: repositories that host Epic's original Unreal Engine 1 source (one turned up in the search). That code is Epic's and has no licence that allows copying it into this port.
