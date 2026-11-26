#pragma once

// The MiSTer's gamepad (MiSTer hybrid core): joystick 1 as SDL's "mister"
// driver presents it, whatever the pad is, since Main_MiSTer has already
// mapped it to the core's button list ("J1" in core/SurrealEngine.sv):
//   buttons 0..7: Fire, Alt Fire, Jump, Duck, Next Weapon, Prev Weapon,
//                 Scores, Menu; 28 / 29: the MiSTer menu's OK / Back
//   hat 0: the d-pad; axes 0..3: the two sticks; axes 4 / 5: L2 / R2
// The core's default ("jn") puts these on an 8BitDo Pro style pad as
// R, L, B, A, X, Y, Select, Start, with the triggers firing as well.

#include <map>
#include <string>

class Engine;

class MisterGamepad
{
public:
	// The engine's bindings for the pad's buttons and sticks (Engine::keybindings)
	static void SetBindings(std::map<std::string, std::string>& keybindings);
	// Once a frame, after the window's events: what changed goes to the engine
	static void Poll(Engine* engine);
};
