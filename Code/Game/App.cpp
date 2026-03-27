#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Player.hpp"
#include "Game/Map.hpp"

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
#include "Engine/Renderer/DebugRenderSystem.hpp"

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

	TileDefinition::InitializeTileDefs();
	MapDefinition::InitializeMapDefs();

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

	if (g_engine->m_input->IsKeyDown(KEYCODE_F2)) {
		m_game->m_curMap->m_sunDirection.x -= 0.1f;
	}

	if (g_engine->m_input->IsKeyDown(KEYCODE_F3)) {
		m_game->m_curMap->m_sunDirection.x += 0.1f;
	}

	if (g_engine->m_input->IsKeyDown(KEYCODE_F4)) {
		m_game->m_curMap->m_sunDirection.y -= 0.1f;
	}

	if (g_engine->m_input->IsKeyDown(KEYCODE_F5)) {
		m_game->m_curMap->m_sunDirection.y += 0.1f;
	}

	if (g_engine->m_input->IsKeyDown(KEYCODE_F6)) {
		m_game->m_curMap->m_sunIntensity -= 0.05f;
		if (m_game->m_curMap->m_sunIntensity < 0.f) {
			m_game->m_curMap->m_sunIntensity = 0.f;
		}
	}

	if (g_engine->m_input->IsKeyDown(KEYCODE_F7)) {
		m_game->m_curMap->m_sunIntensity += 0.05f;
	}

	DebugAddWorldArrow(
		m_game->m_curMap->GetMapWorldCenter() + Vec3(0,0,20.f),
		m_game->m_curMap->GetMapWorldCenter() + Vec3(0, 0, 20.f) + m_game->m_curMap->m_sunDirection.GetNormalized() * 10.f,
		1.f,
		0.f,
		Rgba8::YELLOW,
		Rgba8::YELLOW
	);

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_F12)) {
		Reboot();
		m_isShutdown = true;
		return;
	}

	if (g_engine->m_input->IsKeyDown('T') || g_engine->m_input->WasKeyJustPressed('T')) {
		if (g_engine->m_input->WasKeyJustPressed('T')) {
			SoundID slowDownAudio = g_engine->m_audio->CreateOrGetSound("Data/Audio/Debug_TestAudio.mp3");
			g_engine->m_audio->StartSound(slowDownAudio, false);
		}

		m_game->m_gameClock->SetTimeScale(g_gameConfig->m_debugSlowdownTimescale);
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

	if (g_engine->m_input->WasKeyJustPressed('1')) {
		DebugAddWorldCylinder(
			m_game->m_player->m_position,
			m_game->m_player->m_position + m_game->m_player->m_orientation.GetForwardDir_IFwd_JLeft_KUp() * 20.f,
			0.0625f,
			10.f,
			Rgba8::YELLOW,
			Rgba8::YELLOW,
			DebugRenderMode::X_RAY
		);
	}

	if (g_engine->m_input->IsKeyDown('2')) {
		DebugAddWorldSphere(
			Vec3(
				m_game->m_player->m_position.x,
				m_game->m_player->m_position.y,
				0.f
			),
			0.25f,
			60.f,
			Rgba8(150, 75, 0),
			Rgba8(150, 75, 0),
			DebugRenderMode::USE_DEPTH
		);
	}

	if (g_engine->m_input->WasKeyJustPressed('3')) {
		DebugAddWorldWireSphere(
			m_game->m_player->m_position + m_game->m_player->m_orientation.GetForwardDir_IFwd_JLeft_KUp() * 2.f,
			1.f,
			5.f,
			Rgba8::GREEN,
			Rgba8::RED,
			DebugRenderMode::USE_DEPTH
		);
	}

	if (g_engine->m_input->WasKeyJustPressed('4')) {
		DebugAddBasis(
			Mat44::MakeTransform3D(
				m_game->m_player->m_position,
				m_game->m_player->m_orientation,
				Vec3(1.f, 1.f, 1.f)
			),
			20.f,
			1.f,
			0.1f
		);
	}

	if (g_engine->m_input->WasKeyJustPressed('5')) {
		DebugAddWorldBillboardText(
			Stringf(
				"Position(%.1f, %.1f, %.1f) Orientation(%.1f, %.1f, %.1f)", 
				m_game->m_player->m_position.x, 
				m_game->m_player->m_position.y, 
				m_game->m_player->m_position.z,
				m_game->m_player->m_orientation.m_yawDegrees,
				m_game->m_player->m_orientation.m_pitchDegrees,
				m_game->m_player->m_orientation.m_rollDegrees
			),
			m_game->m_player->m_position + m_game->m_player->m_orientation.GetForwardDir_IFwd_JLeft_KUp() * 3.f,
			0.1f,
			Vec2(0.5f,0.5f),
			10.f,
			Rgba8::WHITE,
			Rgba8::RED,
			DebugRenderMode::USE_DEPTH
		);
	}

	if (g_engine->m_input->WasKeyJustPressed('6')) {
		DebugAddWorldWireCylinder(
			m_game->m_player->m_position + Vec3(0,0,-0.5f),
			m_game->m_player->m_position + Vec3(0,0,0.5f),
			0.5f,
			10.f,
			Rgba8::WHITE,
			Rgba8::RED,
			DebugRenderMode::USE_DEPTH
		);
	}

	if (g_engine->m_input->WasKeyJustPressed('7')) {
		DebugAddMessage(
			Stringf(
				"Current Camera Orientation(%.2f, %.2f, %.2f)",
				m_game->m_player->m_playerCamera->GetOrientation().m_yawDegrees,
				m_game->m_player->m_playerCamera->GetOrientation().m_pitchDegrees,
				m_game->m_player->m_playerCamera->GetOrientation().m_rollDegrees
			),
			5.f
		);
	}

	if (g_engine->m_input->WasKeyJustPressed('8')) {
		m_game->AddCameraShake(20.f);
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

	//Player move input
	Vec3 moveInput = Vec3(0,0,0);
	if (g_engine->m_input->IsKeyDown('A')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.x -= 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('D')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.x += 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('W')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.z -= 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('S')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.z += 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('Z')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.y -= 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('C')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			moveInput.y += 1.f;
		}
	}
	m_game->m_player->m_moveInput = moveInput.GetNormalized();

	//PLayer view input
	m_game->m_player->m_viewInput = g_engine->m_input->GetCursorClientDelta();

	//Player roll view input
	float viewRollInput = 0.f;
	if (g_engine->m_input->IsKeyDown('Q')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			viewRollInput -= 1.f;
		}
	}
	if (g_engine->m_input->IsKeyDown('E')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			viewRollInput += 1.f;
		}
	}
	m_game->m_player->m_viewRollInput = viewRollInput;

	//Player reset transform input
	if (g_engine->m_input->WasKeyJustPressed('H')) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_isResetTransform = true;
		}
	}

	//Player run
	if (g_engine->m_input->IsKeyDown(KEYCODE_SHIFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_isRun = true;
		}
	}
	if (g_engine->m_input->WasKeyJustReleased(KEYCODE_SHIFT)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_isRun = false;
		}
	}

	//Xbox controller input
	//-------------------------------------------------------------------------------------------
	//Control Game-------------------------------------------------------------------------------
	if (g_engine->m_input->GetController(0).GetLeftStick().GetMagnitude() > 0.f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_moveInput.x = g_engine->m_input->GetController(0).GetLeftStick().GetPosition().x;
			m_game->m_player->m_moveInput.z = -g_engine->m_input->GetController(0).GetLeftStick().GetPosition().y;
		}
	}

	if (m_game->m_player->m_moveInput.y == 0) {
		if (g_engine->m_input->GetController(0).IsButtonDown(GAMEPAD_LEFT_SHOULDER)) {
			if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
				m_game->m_player->m_moveInput.y -= 1.f;
			}
		}
		if (g_engine->m_input->GetController(0).IsButtonDown(GAMEPAD_RIGHT_SHOULDER)) {
			if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
				m_game->m_player->m_moveInput.y += 1.f;
			}
		}
	}

	if (m_game->m_player->m_viewRollInput == 0.f) {
		if (g_engine->m_input->GetController(0).GetLeftTrigger() > 0.f) {
			if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
				m_game->m_player->m_viewRollInput -= 1.f;
			}
		}
		if (g_engine->m_input->GetController(0).GetRightTrigger() > 0.f) {
			if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
				m_game->m_player->m_viewRollInput += 1.f;
			}
		}
	}

	if (g_engine->m_input->GetController(0).GetRightStick().GetMagnitude() > 0.f) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_viewInput.x = -g_gameConfig->m_playerViewControllerMultiplier * g_engine->m_input->GetController(0).GetRightStick().GetPosition().x;
			m_game->m_player->m_viewInput.y = g_gameConfig->m_playerViewControllerMultiplier * g_engine->m_input->GetController(0).GetRightStick().GetPosition().y;
		}
	}
	
	if (g_engine->m_input->GetController(0).WasButtonJustPressed(XboxButtonID::GAMEPAD_START)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_isResetTransform = true;
		}
	}

	if (g_engine->m_input->GetController(0).WasButtonJustPressed(XboxButtonID::GAMEPAD_A)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_isRun = true;
		}
	}
	if (g_engine->m_input->GetController(0).WasButtonJustReleased(XboxButtonID::GAMEPAD_A)) {
		if (m_game->GetCurGameState() == GAME_PLAYING_MODE) {
			m_game->m_player->m_isRun = false;
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
	g_gameConfig->m_debugSlowdownTimescale = g_gameConfigBlackboard.GetValue("debugSlowdownTimescale", 0.f);
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
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-SPACE Start game at attract mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-ESC Switch from game mode to attract mode or shut down application in attract mode");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-Mouse(XAxis) Control player view yaw");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-Mouse(YAxis) Control player view pitch");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-Q/E Control player view roll");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-A/D Move left or right, relative to player orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-W/S Move forward or back, relative to player orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-Z/C Move down or up, relative to the world");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-H Reset position and orientation to zero");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-SHIFT Increase speed by a factor of 10 while held");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-P Pause the game");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-O Single step frame");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-T Slow motion mode while held");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-1 Spawn a line from the player along their forward direction");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-2 Spawn a sphere directly below the player position on the world XY-plane");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-3 Spawn a wireframe sphere in front of the player");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-4 Spawn a basis using the player current model matrix");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-5 Spawn a billboard text in font of player showing their position and orientation");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-6 Spawn a wireframe cylinder at the player position");
	g_engine->m_devConsole->AddLine(DevConsoleLineType::INFO_MESSAGE, "\t-7 Add a screen message with the current camera orientation");
}