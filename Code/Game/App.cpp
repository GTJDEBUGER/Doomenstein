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

	m_screenCamera->SetViewPort(
		AABB2(
			Vec2(0.f, 0.f),
			Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
		)
	);

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

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_F2)) {
		DebugAddMessage(
			Stringf("CurCamPos: (%.1f, %.1f, %.1f) CurCamOri: (%.1f, %.1f, %.1f)",
				m_game->m_playerKeyboardController->m_playerCamera->GetPosition().x,
				m_game->m_playerKeyboardController->m_playerCamera->GetPosition().y,
				m_game->m_playerKeyboardController->m_playerCamera->GetPosition().z,
				m_game->m_playerKeyboardController->m_playerCamera->GetOrientation().m_yawDegrees,
				m_game->m_playerKeyboardController->m_playerCamera->GetOrientation().m_pitchDegrees,
				m_game->m_playerKeyboardController->m_playerCamera->GetOrientation().m_rollDegrees
			),
			60.f,
			Rgba8::WHITE,
			Rgba8::RED
		);
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


		SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
		g_engine->m_audio->StartSound(clickAudio, false);
	}

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_SPACE)) {
		if (m_game->GetCurGameState() == GAME_ATTRACT_MODE) {
			delete m_game;
			m_game = new Game();
			m_game->m_playerKeyboardController = new PlayerController(m_game->m_curMap);
			m_game->m_playerKeyboardController->m_handleIndex = 0;
			m_game->m_controllerHandleSequence.push_back(m_game->m_playerKeyboardController);
			m_game->m_activePlayerNum += 1;
			m_game->SetNextGameState(GAME_LOBBY_MODE);

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);
		}
		else if (m_game->GetCurGameState() == GAME_LOBBY_MODE && m_game->m_playerKeyboardController==nullptr) {
			m_game->m_playerKeyboardController = new PlayerController(m_game->m_curMap);
			m_game->m_playerKeyboardController->m_handleIndex = 1;
			m_game->m_controllerHandleSequence.push_back(m_game->m_playerKeyboardController);
			m_game->m_activePlayerNum += 1;

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);
		}
		else if (m_game->GetCurGameState() == GAME_LOBBY_MODE && m_game->m_playerKeyboardController != nullptr) {
			m_game->InitializePlayerActors();
			m_game->SetNextGameState(GAME_PLAYING_MODE);

			if (m_game->m_activePlayerNum > 1) {
				g_engine->m_audio->Set3DAudioEnabled(false);
			}else {
				g_engine->m_audio->Set3DAudioEnabled(true);
			}

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);
		}
	}

	//Toggle player camera mode
	if (g_engine->m_input->WasKeyJustPressed('F')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_activePlayerNum == 1) {
			if (m_game->m_playerKeyboardController != nullptr) {
				if (m_game->m_playerKeyboardController->m_cameraMode == PlayerCameraMode::ACTOR_CAMERA) {
					m_game->m_playerKeyboardController->m_cameraMode = PlayerCameraMode::FREE_CAMERA;
				}
				else if (m_game->m_playerKeyboardController->m_cameraMode == PlayerCameraMode::FREE_CAMERA) {
					m_game->m_playerKeyboardController->m_cameraMode = PlayerCameraMode::ACTOR_CAMERA;
				}

				SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
				g_engine->m_audio->StartSound(clickAudio, false);
			}
		}
	}

	//Switch player controller possessed actor to next one
	if (g_engine->m_input->WasKeyJustPressed('N') && m_game->m_activePlayerNum == 1) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerKeyboardController->m_cameraMode != PlayerCameraMode::FREE_CAMERA) {
			m_game->m_playerKeyboardController->SwitchToNextPossessibleActor();

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);

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
	if (m_game->m_playerKeyboardController !=nullptr)
		m_game->m_playerKeyboardController->m_inputActions.moveInput = moveInput.GetNormalized();

	//Player view input
	if (m_game->m_playerKeyboardController != nullptr)
		m_game->m_playerKeyboardController->m_inputActions.viewInput = g_engine->m_input->GetCursorClientDelta();

	//Player run input
	if (g_engine->m_input->IsKeyDown(KEYCODE_SHIFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerKeyboardController->m_inputActions.isRun = true;
		}
	}
	if (g_engine->m_input->WasKeyJustReleased(KEYCODE_SHIFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerKeyboardController->m_inputActions.isRun = false;
		}
	}

	//Player equip weapon input
	if (g_engine->m_input->WasKeyJustPressed('1')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerKeyboardController->GetPossessedActor()->EquipWeapon(0);
		}
	}
	else if (g_engine->m_input->WasKeyJustPressed('2')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_playerKeyboardController->GetPossessedActor()->EquipWeapon(1);
		}
	}
	else if (g_engine->m_input->WasKeyJustPressed('3')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			if (m_game->m_playerKeyboardController->m_isUnlockFishrod)
				m_game->m_playerKeyboardController->GetPossessedActor()->EquipWeapon(2);
		}
	}
	else if (g_engine->m_input->WasKeyJustPressed('4')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			if (m_game->m_playerKeyboardController->m_haveFish)
			m_game->m_playerKeyboardController->GetPossessedActor()->EquipWeapon(3);
		}
	}

	//PLayer attack input
	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_LEFT_MOUSE) && m_game->GetCurGameState() == GAME_PLAYING_MODE) {
		m_game->m_playerKeyboardController->m_inputActions.isAttack = true;
	}
	else if (g_engine->m_input->WasKeyJustReleased(KEYCODE_LEFT_MOUSE) && m_game->GetCurGameState() == GAME_PLAYING_MODE) {
		m_game->m_playerKeyboardController->m_inputActions.isAttack = false;
	}

	//Player jump input
	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_SPACE) && m_game->GetCurGameState() == GAME_PLAYING_MODE) {
		if (m_game->m_playerKeyboardController != nullptr && m_game->m_playerKeyboardController->m_playerStates.isGrounded)
			m_game->m_playerKeyboardController->m_inputActions.isJump = true;
	}

	//Xbox controller input
	//-------------------------------------------------------------------------------------------
	//Control Game-------------------------------------------------------------------------------
	if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_BACK)) {
		if (m_game->GetCurGameState() == GAME_ATTRACT_MODE) {
			m_isQuitting = true;
		}
		else {
			m_game->SetNextGameState(GAME_ATTRACT_MODE);
		}

		SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
		g_engine->m_audio->StartSound(clickAudio, false);

	}

	if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_START)) {
		if (m_game->GetCurGameState() == GAME_ATTRACT_MODE) {
			delete m_game;
			m_game = new Game();
			m_game->m_playerGamepadController = new PlayerController(m_game->m_curMap);
			m_game->m_playerGamepadController->m_handleIndex = 0;
			m_game->m_playerGamepadController->m_gamepadID = 0;
			m_game->m_controllerHandleSequence.push_back(m_game->m_playerGamepadController);
			m_game->m_activePlayerNum += 1;
			m_game->SetNextGameState(GAME_LOBBY_MODE);

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);
		}
		else if (m_game->GetCurGameState() == GAME_LOBBY_MODE && m_game->m_playerGamepadController == nullptr) {
			m_game->m_playerGamepadController = new PlayerController(m_game->m_curMap);
			m_game->m_playerGamepadController->m_handleIndex = 1;
			m_game->m_playerGamepadController->m_gamepadID = 0;
			m_game->m_controllerHandleSequence.push_back(m_game->m_playerGamepadController);
			m_game->m_activePlayerNum += 1;

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);
		}
		else if (m_game->GetCurGameState() == GAME_LOBBY_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->SetNextGameState(GAME_PLAYING_MODE);
			m_game->InitializePlayerActors();

			if (m_game->m_activePlayerNum > 1) {
				g_engine->m_audio->Set3DAudioEnabled(false);
			}
			else {
				g_engine->m_audio->Set3DAudioEnabled(true);
			}

			SoundID clickAudio = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
			g_engine->m_audio->StartSound(clickAudio, false);
		}
	}

	if (g_engine->m_input->GetController(0).GetLeftStick().GetMagnitude() > 0.1f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController !=nullptr) {
			m_game->m_playerGamepadController->m_inputActions.moveInput.y = -g_engine->m_input->GetController(0).GetLeftStick().GetPosition().x;
			m_game->m_playerGamepadController->m_inputActions.moveInput.x = g_engine->m_input->GetController(0).GetLeftStick().GetPosition().y;
		}
	} 
	else {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.moveInput = Vec3(0.f, 0.f, 0.f);
		}
	}

	if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_A)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			if (m_game->m_playerGamepadController != nullptr && m_game->m_playerGamepadController->m_playerStates.isGrounded)
				m_game->m_playerGamepadController->m_inputActions.isJump = true;
		}
	}

	if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_DPAD_UP)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->GetPossessedActor()->EquipWeapon(0);
		}
	}
	else if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_DPAD_DOWN)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->GetPossessedActor()->EquipWeapon(1);
		}
	}
	else if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_DPAD_LEFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			if (m_game->m_playerGamepadController->m_isUnlockFishrod)
				m_game->m_playerGamepadController->GetPossessedActor()->EquipWeapon(2);
		}
	}
	else if (g_engine->m_input->GetController(0).WasButtonJustPressed(GAMEPAD_DPAD_RIGHT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			if (m_game->m_playerGamepadController->m_haveFish)
				m_game->m_playerGamepadController->GetPossessedActor()->EquipWeapon(3);
		}
	}

	if (g_engine->m_input->GetController(0).GetRightStick().GetMagnitude() > 0.f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.viewInput.x = -g_gameConfig->m_playerViewControllerSensitivity * g_engine->m_input->GetController(0).GetRightStick().GetPosition().x;
			m_game->m_playerGamepadController->m_inputActions.viewInput.y = g_gameConfig->m_playerViewControllerSensitivity * g_engine->m_input->GetController(0).GetRightStick().GetPosition().y;
		}
	}else {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.viewInput = Vec2(0.f, 0.f);
		}
	}

	if (g_engine->m_input->GetController(0).GetLeftTrigger() > 0.1f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.isRun = true;
		}
	}
	else {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.isRun = false;
		}
	}

	if (g_engine->m_input->GetController(0).GetRightTrigger() > 0.99f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.isAttack = true;
		}
	}
	else
	{
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE && m_game->m_playerGamepadController != nullptr) {
			m_game->m_playerGamepadController->m_inputActions.isAttack = false;
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
	g_gameConfig->m_playerViewControllerSensitivity = g_gameConfigBlackboard.GetValue("playerViewControllerSensitivity", 0.f);
	g_gameConfig->m_musicVolume = g_gameConfigBlackboard.GetValue("musicVolume", 0.f);
	g_gameConfig->m_mainMenuMusic = g_gameConfigBlackboard.GetValue("mainMenuMusic", "Error");
	g_gameConfig->m_gameMusic0 = g_gameConfigBlackboard.GetValue("gameMusic0", "Error");
	g_gameConfig->m_gameMusic1 = g_gameConfigBlackboard.GetValue("gameMusic1", "Error");
	g_gameConfig->m_gameMusic2 = g_gameConfigBlackboard.GetValue("gameMusic2", "Error");
	g_gameConfig->m_environmentBackgroundMusic = g_gameConfigBlackboard.GetValue("environmentBackgroundMusic", "Error");
	g_gameConfig->m_soundEffectVolume = g_gameConfigBlackboard.GetValue("soundEffectVolume", 0.f);
	g_gameConfig->m_buttonClickSound = g_gameConfigBlackboard.GetValue("buttonClickSound", "Error");
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
	g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_mainMenuMusic);
	g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_gameMusic0);
	g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_gameMusic1);
	g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_gameMusic2);
	g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_environmentBackgroundMusic);
	g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_buttonClickSound);
}

//-----------------------------------------------------------------------------------------------
void App::PrintGameControlGuide() {
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "Doomenstein");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "Control Guide:");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-SPACE or START-                 \tJoin player at attract mode or start game at lobby mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-ESC or BACK-                    \tSwitch from game mode to attract mode or shut down application in attract mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-Mouse or Right Stick-           \tControl player view");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-LeftMouse or Right Trigger-     \tFire current possessed actors equipped weapon");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-A/D or Left Stick X Axis-       \tMove left or right, relative to player orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-W/S or Right Stick Y Axis-      \tMove forward or back, relative to player orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-SHIFT or Left Trigger-          \tIncrease speed while held");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-SPACE or Gamepad-A-             \tJump");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-1 or D pad-Up-                  \tSwitch to weapon 1");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-2 or D pad-Down-                \tSwitch to weapon 2");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-F-                              \tToggle free camera mode (Single player only)");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-N-                              \tPossess next valid actor in map (single player only)");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-P-                              \tPause the game");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-O-                              \tSingle step frame");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-T-                              \tSlow down game while held");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "-H-                              \tSpeed up game while held");
}