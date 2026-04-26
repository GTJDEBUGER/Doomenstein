#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/PlayerController.hpp"
#include "Game/Map.hpp"
#include "Game/Actor.hpp"
#include "Game/Weapon.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine//Renderer/Camera.hpp"
#include "Game/TileDefinition.hpp"
#include "Game/MapDefinition.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/StringUtils.hpp"

GameConfig* g_gameConfig = nullptr;
App* g_app = nullptr;
BitmapFont* g_defaultFont = nullptr;

//-----------------------------------------------------------------------------------------------
App::App()
{
	LoadGameConfig();
	m_screenCamera = new Camera(Vec2(0.f, 0.f), g_gameConfig->m_screenSize);

	EngineConfig config;
	config.m_windowConfig.m_clientAspect = g_gameConfig->m_screenAspect;
	config.m_windowConfig.m_windowTitle = g_gameConfig->m_windowName;
	config.m_devConsoleConfig.m_camera = m_screenCamera;
	new Engine(config);

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_engine->m_renderer;
	DebugRenderSystemStartup(debugRenderConfig);

	g_engine->m_eventSystem->SubscribeEventCallbackFunction("quit", &Command_Quit, "Close whole game application.", true);
	g_engine->m_eventSystem->SubscribeEventCallbackFunction("guide", &Command_Guide, "Show how to control the game.", true);

	TileDefinition::InitializeTileDefs("Data/Images/Terrain_8x8.png");
	MapDefinition::InitializeMapDefs("Data/Definitions/MapDefinitions.xml");
	ActorDefinition::InitializeActorDefs("Data/Definitions/ActorDefinitions.xml");
	ActorDefinition::InitializeActorDefs("Data/Definitions/ProjectileActorDefinitions.xml");
	WeaponDefinition::InitializeWeaponDefs("Data/Definitions/WeaponDefinitions.xml");

	g_defaultFont = g_engine->m_renderer->CreateOrGetBitmapFontFromFile("Data/Fonts/SquirrelFixedFont.png");
	m_game = new Game();

	PrintGameControlGuide();
	LoadTextureResources();
	LoadAudioResources();
}

//-----------------------------------------------------------------------------------------------
App::~App()
{
	delete m_screenCamera;
	m_screenCamera = nullptr;

	delete m_game;
	m_game = nullptr;

	delete g_engine;
	g_engine = nullptr;

	delete g_gameConfig;
	g_gameConfig = nullptr;
}

//-----------------------------------------------------------------------------------------------
void App::RunMainLoop() {
	while (!IsQuitting())
	{
		RunFrame();
	}
}

//-----------------------------------------------------------------------------------------------
void App::RunFrame()
{
	//-------------------------------------------------------------------------------------------
	g_engine->BeginFrame();		
		HandlePlayerInput();

		if (m_isShutdown) {
			m_isQuitting = true;
			DebugRenderSystemShutdown();
			m_isShutdown = false;
			return;
		}

		DebugRenderBeginFrame();
			Update();
			Render();
		DebugRenderEndFrame();

	g_engine->EndFrame();

}

//-----------------------------------------------------------------------------------------------
void App::Update() {
	//-------------------------------------------------------------------------------------------
	Clock::TickSystemClock();
	m_game->Update();
}

//-----------------------------------------------------------------------------------------------
void App::Render() {
	//Render game
	//-------------------------------------------------------------------------------------------
	m_game->Render();

	//Render Dev Console
	//-------------------------------------------------------------------------------------------
	g_engine->m_devConsole->Render(AABB2(Vec2(0.f, 0.f), g_gameConfig->m_screenSize));
}

//-----------------------------------------------------------------------------------------------
bool App::IsQuitting() {
	return m_isQuitting;
}

//-----------------------------------------------------------------------------------------------
void App::Reboot()
{
	delete m_game;
	m_game = new Game();
}

//-----------------------------------------------------------------------------------------------
void App::HandlePlayerInput(){

	//Keyboard input
	//-------------------------------------------------------------------------------------------
	//Debug Game---------------------------------------------------------------------------------
	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_TILDE)) {
		g_engine->m_devConsole->ToggleOpen();
	}

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_F1)) {
		m_game->m_isDrawDebug = !m_game->m_isDrawDebug;
	}

	if (g_engine->m_input->IsKeyDown('T') || g_engine->m_input->WasKeyJustPressed('T')) {
		if (g_engine->m_input->WasKeyJustPressed('T')) {
			SoundID slowDownAudio = g_engine->m_audio->CreateOrGetSound("Data/Audio/Debug_TestAudio.mp3");
			g_engine->m_audio->StartSound(slowDownAudio, false);
		}

		m_game->m_gameClock->SetTimeScale(g_gameConfig->m_debugSlowdownTimescale);
	}

	if (g_engine->m_input->IsKeyDown('H') || g_engine->m_input->WasKeyJustPressed('H')) {
		if (g_engine->m_input->WasKeyJustPressed('H')) {
			SoundID slowDownAudio = g_engine->m_audio->CreateOrGetSound("Data/Audio/Debug_TestAudio.mp3");
			g_engine->m_audio->StartSound(slowDownAudio, false);
		}
		m_game->m_gameClock->SetTimeScale(g_gameConfig->m_debugSpeedupTimescale);
	}

	if (g_engine->m_input->WasKeyJustPressed('P')) {
		m_game->m_gameClock->TogglePause();
	}

	if (g_engine->m_input->WasKeyJustPressed('O')) {
		m_game->m_gameClock->StepSingleFrame();
	}

	if (g_engine->m_input->WasKeyJustReleased('T')) {
		m_game->m_gameClock->SetTimeScale(1.f);
	}

	if (g_engine->m_input->WasKeyJustReleased('H')) {
		m_game->m_gameClock->SetTimeScale(1.f);
	}

	//Control Game-------------------------------------------------------------------------------
	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_ESC)) {
		if (m_game->GetCurGameState() == GAME_ATTRACT_MODE) {
			m_isQuitting = true;
		}
		else {
			m_game->SetNextGameState(GAME_ATTRACT_MODE);
		}
	}

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_SPACE)) {
		if (m_game->GetCurGameState() == GAME_ATTRACT_MODE) {
			delete m_game;
			m_game = new Game();
			m_game->SetNextGameState(GAME_PLAYING_MODE);
		}
	}

	//Toggle player camera mode
	if (g_engine->m_input->WasKeyJustPressed('F')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			if (m_game->m_playerController->m_cameraMode == PlayerCameraMode::ACTOR_CAMERA) {
				m_game->m_playerController->m_cameraMode = PlayerCameraMode::FREE_CAMERA;
			}
			else if (m_game->m_playerController->m_cameraMode == PlayerCameraMode::FREE_CAMERA){
				m_game->m_playerController->m_cameraMode = PlayerCameraMode::ACTOR_CAMERA;
			}
		}
	}

	//Switch player controller possessed actor to next one
	if (g_engine->m_input->WasKeyJustPressed('N')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerController->m_cameraMode != PlayerCameraMode::FREE_CAMERA) {
			m_game->m_playerController->SwitchToNextPossessibleActor();
		}
	}

	//Player move input
	Vec3 moveInput = Vec3(0,0,0);
	if (g_engine->m_input->IsKeyDown('A')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.y += 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('D')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.y -= 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('W')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.x += 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('S')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.x -= 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('Z')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.z += 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('C')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.z -= 1.f;
		}
	}
	m_game->m_playerController->m_inputActions.moveInput = moveInput.GetNormalized();

	//Player view input
	m_game->m_playerController->m_inputActions.viewInput = g_engine->m_input->GetCursorClientDelta();

	//Player run input
	if (g_engine->m_input->IsKeyDown(KEYCODE_SHIFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->m_inputActions.isRun = true;
		}
	}
	if (g_engine->m_input->WasKeyJustReleased(KEYCODE_SHIFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->m_inputActions.isRun = false;
		}
	}

	//Player equip weapon input
	if (g_engine->m_input->WasKeyJustPressed('1')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->GetPossessedActor()->EquipWeapon(0);
		}
	}
	else if (g_engine->m_input->WasKeyJustPressed('2')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->GetPossessedActor()->EquipWeapon(1);
		}
	}

	//PLayer attack input
	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_LEFT_MOUSE) && m_game->GetCurGameState() == GAME_PLAYING_MODE) {
		m_game->m_playerController->m_inputActions.isAttack = true;
	}
	else if (g_engine->m_input->WasKeyJustReleased(KEYCODE_LEFT_MOUSE) && m_game->GetCurGameState() == GAME_PLAYING_MODE) {
		m_game->m_playerController->m_inputActions.isAttack = false;
	}

	//Player jump input
	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_SPACE) && m_game->GetCurGameState() == GAME_PLAYING_MODE) {
		if (m_game->m_playerController->m_playerStates.isGrounded)
			m_game->m_playerController->m_inputActions.isJump = true;
	}

	//Xbox controller input
	//-------------------------------------------------------------------------------------------
	//Control Game-------------------------------------------------------------------------------
	if (g_engine->m_input->GetController(0).GetLeftStick().GetMagnitude() > 0.f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->m_inputActions.moveInput.x = g_engine->m_input->GetController(0).GetLeftStick().GetPosition().x;
			m_game->m_playerController->m_inputActions.moveInput.z = -g_engine->m_input->GetController(0).GetLeftStick().GetPosition().y;
		}
	}

	if (m_game->m_playerController->m_inputActions.moveInput.y == 0) {
		if (g_engine->m_input->GetController(0).IsButtonDown(GAMEPAD_LEFT_SHOULDER)) {
			if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
				m_game->m_playerController->m_inputActions.moveInput.y -= 1.f;
			}
		}
		if (g_engine->m_input->GetController(0).IsButtonDown(GAMEPAD_RIGHT_SHOULDER)) {
			if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
				m_game->m_playerController->m_inputActions.moveInput.y += 1.f;
			}
		}
	}

	if (g_engine->m_input->GetController(0).GetRightStick().GetMagnitude() > 0.f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->m_inputActions.viewInput.x = -g_gameConfig->m_playerViewControllerMultiplier * g_engine->m_input->GetController(0).GetRightStick().GetPosition().x;
			m_game->m_playerController->m_inputActions.viewInput.y = g_gameConfig->m_playerViewControllerMultiplier * g_engine->m_input->GetController(0).GetRightStick().GetPosition().y;
		}
	}

	if (g_engine->m_input->GetController(0).WasButtonJustPressed(XboxButtonID::GAMEPAD_A)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->m_inputActions.isRun = true;
		}
	}
	if (g_engine->m_input->GetController(0).WasButtonJustReleased(XboxButtonID::GAMEPAD_A)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerController->m_inputActions.isRun = false;
		}
	}
}

//-----------------------------------------------------------------------------------------------
bool App::Command_Quit([[maybe_unused]] EventArgs& args) {
	g_app->m_isShutdown = true;

	return true;
}

//-----------------------------------------------------------------------------------------------
bool App::Command_Guide([[maybe_unused]] EventArgs& args) {
	g_app->PrintGameControlGuide();
	return true;
}

//-----------------------------------------------------------------------------------------------
void App::LoadGameConfig() {
	g_gameConfig = new GameConfig();

	XmlDocument doc;
	doc.LoadFile("Data/GameConfig.xml");
	for (XmlElement* i = doc.FirstChildElement(); i != nullptr; i = i->NextSiblingElement()) {
		g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*i);
	}

	g_gameConfig->m_defaultMap = g_gameConfigBlackboard.GetValue("defaultMap", "?");
	g_gameConfig->m_windowName = g_gameConfigBlackboard.GetValue("windowName", "?");
	g_gameConfig->m_debugSlowdownTimescale = g_gameConfigBlackboard.GetValue("debugSlowdownTimescale", 1.f);
	g_gameConfig->m_debugSpeedupTimescale = g_gameConfigBlackboard.GetValue("debugSpeedupTimescale", 1.f);
	g_gameConfig->m_screenSize = g_gameConfigBlackboard.GetValue("screenSize", Vec2(0.f, 0.f));
	g_gameConfig->m_screenCenter = g_gameConfig->m_screenSize * 0.5f;
	g_gameConfig->m_screenAspect = g_gameConfig->m_screenSize.x / g_gameConfig->m_screenSize.y;
	g_gameConfig->m_cameraShakeAmp = g_gameConfigBlackboard.GetValue("cameraShakeAmp", 0.f);
	g_gameConfig->m_cameraShakeDecay = g_gameConfigBlackboard.GetValue("cameraShakeDecay", 0.f);
	g_gameConfig->m_playerMoveSpeed = g_gameConfigBlackboard.GetValue("playerMoveSpeed", 0.f);
	g_gameConfig->m_playerRunSpeed = g_gameConfigBlackboard.GetValue("playerRunSpeed", 0.f);
	g_gameConfig->m_playerViewYawSpeed = g_gameConfigBlackboard.GetValue("playerViewYawSpeed", 0.f);
	g_gameConfig->m_playerViewPitchSpeed = g_gameConfigBlackboard.GetValue("playerViewPitchSpeed", 0.f);
	g_gameConfig->m_playerViewRollSpeed = g_gameConfigBlackboard.GetValue("playerViewRollSpeed", 0.f);
	g_gameConfig->m_playerViewControllerMultiplier = g_gameConfigBlackboard.GetValue("playerViewControllerMultiplier", 0.f);
}

//-----------------------------------------------------------------------------------------------
void App::LoadTextureResources() {
	g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/TestTransparent.png");
	g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/TestUV.png");
	g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/SkySphere.png");
}

//-----------------------------------------------------------------------------------------------
void App::LoadAudioResources() {
	g_engine->m_audio->CreateOrGetSound("Data/Audio/Debug_TestAudio.mp3");
}

//-----------------------------------------------------------------------------------------------
void App::PrintGameControlGuide() {
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "Doomenstein");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "Control Guide:");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-SPACE-            \tStart game at attract mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-ESC-              \tSwitch from game mode to attract mode or shut down application in attract mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-Mouse(XAxis)-     \tControl player view yaw");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-Mouse(YAxis)-     \tControl player view pitch");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-Mouse(LeftButton)-\tFire current possessed actors equipped weapon");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-A/D-              \tMove left or right, relative to player orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-W/S-              \tMove forward or back, relative to player orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-Z/C-              \tMove up or down, relative to the world");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-SHIFT-            \tIncrease speed while held");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-SPACE-            \tJump");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-F-                \tToggle free camera mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-N-                \tPossess next valid actor in map");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-P-                \tPause the game");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-O-                \tSingle step frame");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-T-                \tSlow down game while held");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-H-                \tSpeed up game while held");
}