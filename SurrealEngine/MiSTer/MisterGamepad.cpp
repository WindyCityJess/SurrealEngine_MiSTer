#include "Precomp.h"
#include "MisterGamepad.h"
#include "Engine.h"
#include "GameWindow.h"
#include <SDL2/SDL.h>
#include <cstdlib>

namespace
{
	enum { NumButtons = 8, ButtonMenu = 7, ButtonMenuOK = 28, ButtonMenuBack = 29 };
	enum { AxisLeftX, AxisLeftY, AxisRightX, AxisRightY, AxisL2, AxisR2 };

	// a stick's travel as the engine gets it; with the speeds below a stick
	// held over is what the keyboard's keys give (16 * 125 * 3.0 = 6000,
	// MoveForward's 300.0 * 20)
	const int StickRange = 125;
	const int DeadZone = 6500; // of 32767: about a fifth
	const int TriggerDown = 8000; // of 32767

	SDL_Joystick* joystick = nullptr;

	bool buttonDown[NumButtons];
	bool menuOKDown, menuBackDown;
	bool hatDown[4]; // up, down, left, right
	bool axisActive[4];

	const EInputKey buttonKeys[NumButtons] = { IK_Joy1, IK_Joy2, IK_Joy3, IK_Joy4, IK_Joy5, IK_Joy6, IK_Joy7, IK_Escape };
	const EInputKey hatKeys[4] = { IK_Up, IK_Down, IK_Left, IK_Right };
	const EInputKey axisKeys[4] = { IK_JoyX, IK_JoyY, IK_JoyU, IK_JoyV };

	void SetKey(Engine* engine, bool& state, bool down, EInputKey key)
	{
		if (state == down)
			return;
		state = down;
		if (down)
			engine->OnWindowKeyDown(key);
		else
			engine->OnWindowKeyUp(key);
	}

	// -StickRange..StickRange, 0 inside the dead zone and rising from its edge
	int StickValue(int raw)
	{
		int magnitude = std::abs(raw);
		if (magnitude <= DeadZone)
			return 0;
		int value = (magnitude - DeadZone) * StickRange / (32767 - DeadZone);
		if (value > StickRange) value = StickRange;
		return raw < 0 ? -value : value;
	}
}

void MisterGamepad::SetBindings(std::map<std::string, std::string>& keybindings)
{
	// What the game's own aliases of these names do (User.ini), spelled out so
	// that they do not depend on it
	keybindings["Joy1"] = "Button bFire|Fire";
	keybindings["Joy2"] = "Button bAltFire|AltFire";
	keybindings["Joy3"] = "Jump|Axis aUp Speed=+300.0";
	keybindings["Joy4"] = "Button bDuck|Axis aUp Speed=-300.0";
	keybindings["Joy5"] = "NextWeapon";
	keybindings["Joy6"] = "PrevWeapon";
	keybindings["Joy7"] = "ShowScores";
	// left stick: walk and strafe; right stick: turn and look
	keybindings["JoyX"] = "Axis aStrafe Speed=3.0";
	keybindings["JoyY"] = "Axis aBaseY Speed=3.0";
	keybindings["JoyU"] = "Axis aBaseX Speed=2.0";
	keybindings["JoyV"] = "Axis aLookUp Speed=1.0";
}

void MisterGamepad::Poll(Engine* engine)
{
	if (!joystick)
	{
		if (SDL_NumJoysticks() < 1)
			return;
		joystick = SDL_JoystickOpen(0);
		if (!joystick)
			return;
	}

	int numButtons = SDL_JoystickNumButtons(joystick);
	int numAxes = SDL_JoystickNumAxes(joystick);
	auto button = [&](int index) { return index < numButtons && SDL_JoystickGetButton(joystick, index) != 0; };
	auto axis = [&](int index) { return index < numAxes ? (int)SDL_JoystickGetAxis(joystick, index) : 0; };

	// the triggers fire as R and L do
	bool down[NumButtons];
	for (int i = 0; i < NumButtons; i++)
		down[i] = button(i);
	down[0] = down[0] || axis(AxisR2) > TriggerDown;
	down[1] = down[1] || axis(AxisL2) > TriggerDown;
	for (int i = 0; i < NumButtons; i++)
		SetKey(engine, buttonDown[i], down[i], buttonKeys[i]);

	// the MiSTer menu's own buttons confirm and go back in the game's menus
	SetKey(engine, menuOKDown, button(ButtonMenuOK), IK_Enter);
	SetKey(engine, menuBackDown, button(ButtonMenuBack) && !down[ButtonMenu], IK_Escape);

	// the d-pad is the cursor keys: the game's menus, and walking and turning
	// as the game binds them
	Uint8 hat = SDL_JoystickNumHats(joystick) > 0 ? SDL_JoystickGetHat(joystick, 0) : 0;
	SetKey(engine, hatDown[0], (hat & SDL_HAT_UP) != 0, hatKeys[0]);
	SetKey(engine, hatDown[1], (hat & SDL_HAT_DOWN) != 0, hatKeys[1]);
	SetKey(engine, hatDown[2], (hat & SDL_HAT_LEFT) != 0, hatKeys[2]);
	SetKey(engine, hatDown[3], (hat & SDL_HAT_RIGHT) != 0, hatKeys[3]);

	// Sticks: pushed up is forward and looking up, where SDL's value is negative
	int values[4] = { StickValue(axis(AxisLeftX)), -StickValue(axis(AxisLeftY)), StickValue(axis(AxisRightX)), -StickValue(axis(AxisRightY)) };
	for (int i = 0; i < 4; i++)
	{
		if (values[i] != 0)
		{
			engine->InputEvent(axisKeys[i], IST_Axis, values[i]);
			axisActive[i] = true;
		}
		else if (axisActive[i])
		{
			// back in the middle: the axis is taken out again
			engine->InputEvent(axisKeys[i], IST_Release);
			axisActive[i] = false;
		}
	}
}
