
#include "Precomp.h"
#include "Utils/Exception.h"
#include "Utils/Logger.h"
#include "Utils/CommandLine.h"
#include "GameApp.h"
#include "GameFolder.h"
#include "Engine.h"
#include "UI/WidgetResourceData.h"
#include "UI/ErrorWindow/ErrorWindow.h"
#include "UI/Launcher/LauncherWindow.h"
#include "Utils/File.h"
#include <stdexcept>
#include <iostream>
#ifdef MISTER_HYBRID
extern "C" {
#include "MiSTer/mister_port.h"
}
#endif

int GameApp::main(Array<std::string> args)
{
	InitWidgetResources("dark");

	try
	{
		CommandLine cmd(args);
		commandline = &cmd;

		if (ErrorWindow::CheckCrashReporter())
			return 0;

		if (commandline->HasArg("-h", "--help"))
		{
			std::cout << "SurrealEngine [--url=<mapname>] [--engineversion=X] [Path to game folder]\n";
			return 0;
		}

#ifdef MISTER_HYBRID
		// No launcher window on the MiSTer: the game is the folder the launch
		// script names (or the first one found)
		GameFolderSelection::UpdateList();
		if (GameFolderSelection::Games.empty())
		{
			MiSTer_ShowError(
				"No Unreal Tournament found. Copy the game's folders (System, Maps,\n"
				"Textures, Sounds, Music) to games/dnk/SurrealEngine/ on the SD card.\n"
				"Version 436 of the game is the one that works best.");
			DeinitWidgetResources();
			return 1;
		}
		{
			GameLaunchInfo info = GameFolderSelection::GetLaunchInfo(0);
			Engine engine(info);
			engine.Run();
		}
#else
		int selectedGameIndex = LauncherWindow::ExecModal();
		if (selectedGameIndex >= 0)
		{
			GameLaunchInfo info = GameFolderSelection::GetLaunchInfo(selectedGameIndex);
			Engine engine(info);
			engine.Run();
		}
#endif
	}
	catch (const std::exception& e)
	{
#ifdef MISTER_HYBRID
		std::cout << "Fatal: " << e.what() << std::endl;
		MiSTer_ShowError((std::string("The engine stopped: ") + std::string(e.what()).substr(0, 180)).c_str());
		DeinitWidgetResources();
		return 1;
#else
		ErrorWindow::ExecModal(e.what(), Logger::Get()->GetLog());
#endif
	}

	DeinitWidgetResources();
#ifdef MISTER_HYBRID
	return MiSTer_ExitCode();
#else
	return 0;
#endif
}
