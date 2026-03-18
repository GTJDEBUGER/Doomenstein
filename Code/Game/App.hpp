#pragma once
#include "Engine/Core/EventSystem.hpp"


class App;
class Game;
class Camera;
class BitmapFont;
class Clock;

extern App* g_app;
extern BitmapFont* g_defaultFont;

//-----------------------------------------------------------------------------------------------
class App
{
public:
	App();
	~App();
	void RunMainLoop();
	void RunFrame();
	void Update();
	void Render();
	bool IsQuitting();
	void Reboot();

	static bool Command_Quit(EventArgs& args);
	static bool Command_Guide(EventArgs& args);

public:
	Game*   m_game                      = nullptr;
	Camera* m_screenCamera              = nullptr;
					                    
	bool m_isQuitting                   = false;
	bool m_isShutdown                   = false;
	bool m_isDrawDebugInfo              = false;

private:
	void HandlePlayerInput();
	void LoadTextureResources();
	void LoadAudioResources();
	void PrintGameControlGuide();
};