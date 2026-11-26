SurrealEngine for MiSTer
=================

Unreal Tournament on MiSTer FPGA as a hybrid port.

Everything about it (options, controls, help):
  https://github.com/WindyCityJess/SurrealEngine_MiSTer

Game files
  Copy everything in your game's folder into this folder,
  /media/fat/games/dnk/SurrealEngine/, all the files as they are:
    Unreal Tournament (version 436): the folders System, Maps, Textures,
    Sounds and Music (System/UnrealTournament.exe must be there)
  They can go on a USB drive instead, in /media/usb0/games/dnk/SurrealEngine/.

Playing
  1. Run danik_hybrid_ports once from the MiSTer's Scripts menu. It starts
     the game whenever its menu entry is loaded.
  2. Load the game from the Other menu, e.g. dnk-Unreal Tournament, or from the
     Hybrid Ports folder if you moved the games there in the launcher.
  Buttons are changed in the OSD under "Define SurrealEngine buttons". A button
  left without a name there does not reach the game at all: name every
  button, or leave the definition as it is.

If something is wrong
  Open the OSD while it happens and pick Save Diagnostic Report, the last
  line. Send the file it saves in games/dnk/diagnostics/ with a few words
  on what went wrong: https://github.com/WindyCityJess/SurrealEngine_MiSTer/issues

Credits and licenses
  Surreal Engine by Magnus Norddahl, Lupert Everett and contributors: zlib
  style (LICENSE-surrealengine.txt). Hybrid Core Framework by danik. SDL by Sam Lantinga and contributors: zlib
  (LICENSE-sdl.txt). MiSTer by Sorgelig and the MiSTer-devel contributors.
  Inspired by MiSTer Frontier by MiSTer Organize. The launcher: GPL-3.0
  (LICENSE-gpl3.txt); the FPGA core: GPL-2.0. Source:
  https://github.com/WindyCityJess/SurrealEngine_MiSTer
