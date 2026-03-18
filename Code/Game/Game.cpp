#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Game/Entity.hpp"
#include "Game/Player.hpp"
#include "Game/CubeProp.hpp"
#include "Game/SphereProp.hpp"
#include "Game/CylinderProp.hpp"
#include "Game/ConeProp.hpp"

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"


//---------------------------------------------------------------------------------------------------
Game::Game()
{
	m_gameClock = new Clock();

	m_UICamera = new Camera(
		Vec2(0.f, 0.f), 
		Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)
	);

	m_randomGenerator = new RandomNumberGenerator();

	InitialGameEntities();

	DebugAddBasis(
		Mat44::MakeTransform3D(
			Vec3(0.f, 0.f, 0.f),
			EulerAngles(0.f, 0.f, 0.f),
			Vec3(1.f, 1.f, 1.f)
		),
		-1.f,
		1.f,
		0.125f
	);
	DebugAddWorldText(
		"X Forward",
		Mat44::MakeTransform3D(
			Vec3(1.f, 0.f, 0.25f),
			EulerAngles(90.f, 0.f, 0.f),
			Vec3(1.f, 1.f, 1.f)
		),
		0.1f,
		Vec2(0.5f, 0.5f),
		-1.f,
		Rgba8::RED
	);
	DebugAddWorldText(
		"Y Left",
		Mat44::MakeTransform3D(
			Vec3(0.f, 1.f, 0.25f),
			EulerAngles(0.f, 0.f, 0.f),
			Vec3(1.f, 1.f, 1.f)
		),
		0.1f,
		Vec2(0.5f, 0.5f),
		-1.f,
		Rgba8::GREEN
	);
	DebugAddWorldText(
		"Z Up",
		Mat44::MakeTransform3D(
			Vec3(0.f, -0.25f, 1.f),
			EulerAngles(0.f, 0.f, 90.f),
			Vec3(1.f, 1.f, 1.f)
		),
		0.1f,
		Vec2(0.5f, 0.5f),
		-1.f,
		Rgba8::BLUE
	);
}

//---------------------------------------------------------------------------------------------------
Game::~Game()
{
	DeleteGameEntities();

	delete m_randomGenerator;
	m_randomGenerator = nullptr;

	delete m_UICamera;
	m_UICamera = nullptr;

	delete m_gameClock;
	m_gameClock = nullptr;
}

//---------------------------------------------------------------------------------------------------
void Game::Update()
{
	DebugAddScreenText(
		Stringf(
			"Time: %.1f | FPS: %.1f | Scale: %1.f ", 
			(float) m_gameClock->GetTotalSeconds(),
			1.f / (float)m_gameClock->GetDeltaSeconds(),
			(float)m_gameClock->GetTimeScale()
		),
		AABB2(
			Vec2(SCREEN_SIZE_X - 400.f, SCREEN_SIZE_Y - 24.f),
			Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)
		),
		24.f,
		Vec2(0.f, 0.5f),
		0.f,
		Rgba8::WHITE
	);
	//-----------------------------------------------------------------------------------------------
	//Swap game state
	if (m_nextGameState != m_curGameState) {
		m_curGameState = m_nextGameState;
	}

	//-----------------------------------------------------------------------------------------------
	//Update game mode
	if (m_curGameState == GAME_PLAYING_MODE) {
		//Handle cursor mode  
		if (g_engine->m_devConsole->IsOpen() || !g_engine->m_window->m_isWindowFocus) {
			g_engine->m_input->SetCursorMode(CursorMode::POINTER);
		}
		else {
			g_engine->m_input->SetCursorMode(CursorMode::FPS);
		}

		//-------------------------------------------------------------------------------------------
		UpdateCameras();
		UpdateGameEntities();
	}
	
	//-----------------------------------------------------------------------------------------------
	//Update attract mode
	if(m_curGameState == GAME_ATTRACT_MODE) {
		//Handle cursor mode 
		g_engine->m_input->SetCursorMode(CursorMode::POINTER);

	}

	DebugAddMessage(
		Stringf(
			"Player Position: (X=%.1f, Y=%.1f, Z=%.1f)",
			m_player->m_position.x,
			m_player->m_position.y,
			m_player->m_position.z
		),
		0.f
	);
}

//---------------------------------------------------------------------------------------------------
void Game::Render() const
{
	//-----------------------------------------------------------------------------------------------
	if (m_curGameState == GAME_PLAYING_MODE) {
		//-------------------------------------------------------------------------------------------
		g_engine->m_renderer->ClearScreen(Rgba8(50, 50, 50));
		//For GameScene
		g_engine->m_renderer->BeginCamera(*(m_player->m_playerCamera));

		RenderSkySphere();
		RenderWorldGrids();
		RenderTestGamePlay();

		DebugRenderWorld(*(m_player->m_playerCamera));

		g_engine->m_renderer->EndCamera(*(m_player->m_playerCamera));

		//-------------------------------------------------------------------------------------------
		//For UI
		g_engine->m_renderer->BeginCamera(*m_UICamera);
		
		RenderTestGameUI();

		DebugRenderScreen(*(m_UICamera));

		g_engine->m_renderer->EndCamera(*m_UICamera);
	}

	//-----------------------------------------------------------------------------------------------
	if (m_curGameState == GAME_ATTRACT_MODE) {
		//-------------------------------------------------------------------------------------------
		g_engine->m_renderer->ClearScreen(Rgba8(100, 100, 100));
		//For AttractMode
		g_engine->m_renderer->BeginCamera(*m_UICamera);

		RenderTestAttractMode();

		g_engine->m_renderer->EndCamera(*m_UICamera);
	}
}


//--------------------------------------------------------------------------------------------------
void Game::SetNextGameState(GameState nextState) {
	m_nextGameState = nextState;
}

//--------------------------------------------------------------------------------------------------
GameState const Game::GetCurGameState() const {
	return m_curGameState;
}

//--------------------------------------------------------------------------------------------------
void Game::AddCameraShake(float amp) {
	m_curCameraShakeAmp = GetClamped(m_curCameraShakeAmp + amp, 0.f, CAMERA_SHAKE_MAX_AMP);
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateCameras() {
	DecayCameraShake();
	m_player->m_playerCamera->Translate2D(Vec2(m_randomGenerator->RollRandomFloatZeroToOne() * m_curCameraShakeAmp,
		                            m_randomGenerator->RollRandomFloatZeroToOne() * m_curCameraShakeAmp));
}

//---------------------------------------------------------------------------------------------------
void Game::InitialGameEntities() {
	m_player = new Player(this, Vec3(0, 0, 0));
	m_gameEntites.push_back(dynamic_cast<Entity*>(m_player));
	m_gameEntites.push_back(dynamic_cast<Entity*>(new CubeProp(this, Vec3(2.5f, 2.5f, 0.5f), 1.f, false, 45.f, 30.f)));
	m_gameEntites.push_back(dynamic_cast<Entity*>(new CubeProp(this, Vec3(7.5f, 2.5f, 0.5f), 1.f, true)));
	m_gameEntites.push_back(dynamic_cast<Entity*>(new SphereProp(this, Vec3(7.5f, -2.5f, 1.f), 1.f, 32, 16, 45.f)));
	m_gameEntites.push_back(dynamic_cast<Entity*>(new CylinderProp(this, Vec3(2.5f, -2.5f, 0), Vec3(2.5f, -2.5f, 2.f), 1.f)));
	m_gameEntites.push_back(dynamic_cast<Entity*>(new ConeProp(this, Vec3(2.5f, -7.5f, 0), Vec3(2.5f, -7.5f, 2.f), 1.f)));
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateGameEntities() {
	for (int i = 0; i < m_gameEntites.size(); i++) {
		if (m_gameEntites[i] != nullptr) {
			m_gameEntites[i]->Update((float)m_gameClock->GetDeltaSeconds());
		}
	}
}

//---------------------------------------------------------------------------------------------------
void Game::DeleteGameEntities() {
	for (int i = 0; i < m_gameEntites.size(); i++) {
		delete m_gameEntites[i];
		m_gameEntites[i] = nullptr;
	}
}

//---------------------------------------------------------------------------------------------------
void Game::RenderTestAttractMode() const {
	Texture* testTexture = g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/TestTransparent.png");

	std::vector<Vertex> tempMesh;
	float time = (float)m_gameClock->GetTotalSeconds();

	//2D ring rendering
	AddVertexsForRing2D(tempMesh, Vec2(SCREEN_CENTER_X, SCREEN_CENTER_Y),
		SinDegrees(time * 120.f) * 25.f + 125.f, 10.f, Rgba8(255, 255, 255, 128));
	g_engine->m_renderer->BindTexture(nullptr);
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());
	tempMesh.clear();

	//Fake 3D cube rendering
	float L = 50.f;
	Vec3 p[8] = {
		Vec3(-L, -L, -L), Vec3(L, -L, -L), Vec3(L,  L, -L), Vec3(-L,  L, -L),
		Vec3(-L, -L,  L), Vec3(L, -L,  L), Vec3(L,  L,  L), Vec3(-L,  L,  L)
	};

	float yaw = time * 45.f;
	float pitch = time * 30.f;

	struct Face {
		int indices[4];
		float depth = 0.f;
	};

	Face faces[6] = {
		{ {0, 1, 2, 3}, 0.f }, { {4, 5, 6, 7}, 0.f },
		{ {0, 4, 7, 3}, 0.f }, { {1, 5, 6, 2}, 0.f },
		{ {0, 1, 5, 4}, 0.f }, { {3, 2, 6, 7}, 0.f } 
	};

	for (int i = 0; i < 6; i++) {
		Vec3 center;
		for (int j = 0; j < 4; j++) center += p[faces[i].indices[j]];
		center /= 4.f;

		float z = center.x * SinDegrees(yaw) + center.z * CosDegrees(yaw);
		faces[i].depth = center.y * SinDegrees(pitch) + z * CosDegrees(pitch);
	}

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5 - i; j++) {
			if (faces[j].depth < faces[j + 1].depth) {
				std::swap(faces[j], faces[j + 1]);
			}
		}
	}

	int subdiv = 4;

	for (int i = 0; i < 6; i++) {
		Face& f = faces[i];
		Vec3 corners[4] = { p[f.indices[0]], p[f.indices[1]], p[f.indices[2]], p[f.indices[3]] };

		for (int vy = 0; vy < subdiv; vy++) {
			for (int vx = 0; vx < subdiv; vx++) {

				float u_steps[2] = { (float)vx / subdiv, (float)(vx + 1) / subdiv };
				float v_steps[2] = { (float)vy / subdiv, (float)(vy + 1) / subdiv };

				Vertex quad[4];
				for (int r = 0; r < 2; r++) {
					for (int c = 0; c < 2; c++) {
						float u = u_steps[c];
						float v = v_steps[r];

						Vec3 pos3D = corners[0] * (1.f - u) * (1.f - v) +
							corners[1] * u * (1.f - v) +
							corners[2] * u * v +
							corners[3] * (1.f - u) * v;

						float rx = pos3D.x * CosDegrees(yaw) - pos3D.z * SinDegrees(yaw);
						float rz = pos3D.x * SinDegrees(yaw) + pos3D.z * CosDegrees(yaw);
						float ry = pos3D.y * CosDegrees(pitch) - rz * SinDegrees(pitch);
						rz = pos3D.y * SinDegrees(pitch) + rz * CosDegrees(pitch);

						float cameraZ = -400.f;
						float perspectiveScale = 400.f / (rz - cameraZ);
						float screenX = SCREEN_CENTER_X + (rx * perspectiveScale);
						float screenY = SCREEN_CENTER_Y + (ry * perspectiveScale);

						Vec2 uv_base[4] = { Vec2(1.f, 1.f), Vec2(0.f, 1.f), Vec2(0.f, 0.f), Vec2(1.f, 0.f) };
						Vec2 uv = uv_base[0] * (1.f - u) * (1.f - v) +
							uv_base[1] * u * (1.f - v) +
							uv_base[2] * u * v +
							uv_base[3] * (1.f - u) * v;

						quad[r * 2 + c] = Vertex(Vec3(screenX, screenY, 0.f), Rgba8(255,255,255), uv);
					}
				}

				tempMesh.push_back(quad[0]); tempMesh.push_back(quad[1]); tempMesh.push_back(quad[3]);
				tempMesh.push_back(quad[0]); tempMesh.push_back(quad[3]); tempMesh.push_back(quad[2]);
			}
		}
	}
	g_engine->m_renderer->BindTexture(testTexture);
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());

	tempMesh.clear();
	std::string text = "ProtoGame3D";
	float fontSize = 50;
	float shadowSize = 51;
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(SCREEN_CENTER_X - text.size()*0.5f*shadowSize, SCREEN_CENTER_Y-0.5f*shadowSize-130.f),
		shadowSize,
		text,
		Rgba8(50, 50, 50, 255));
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(SCREEN_CENTER_X - text.size() * 0.5f * fontSize, SCREEN_CENTER_Y - 0.5f * fontSize - 125.f),
		fontSize,
		text,
		Rgba8(225, 225, 225, 255));

	text = "-PRESS SPACE START GAME-";
	fontSize = 10;
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(SCREEN_CENTER_X - text.size() * 0.5f * fontSize,2.f*fontSize),
		fontSize,
		text,
		Rgba8(250, 250, 250, (unsigned char)(SinDegrees(time*240.f)*100.f+155.f)));

	g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
	g_engine->m_renderer->BindTexture(&g_defaultFont->GetTexture());
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());
}

//---------------------------------------------------------------------------------------------------
void Game::RenderTestGamePlay() const {
	for (int i = 0; i < m_gameEntites.size(); i++) {
		if (m_gameEntites[i] != nullptr) {
			m_gameEntites[i]->Render();
		}
	}
}

//---------------------------------------------------------------------------------------------------
void Game::RenderTestGameUI() const {
	
}

//---------------------------------------------------------------------------------------------------
void Game::RenderWorldGrids() const {
	std::vector<Vertex> tempMesh;
	int gridNum = 200;
	float normalLineThickness = 0.02f;
	float gridSize = 5.f;
	Rgba8 lineColor(160,160,160);
	for (int i = 0; i <= gridNum; i++) {
		bool isEmphasize = (i % 5 == 0);
		bool isBaseAxis = i == (int)(gridNum * 0.5f);
		float lineThickness = isBaseAxis ? normalLineThickness * 2.f : (isEmphasize ? normalLineThickness * 1.5f : normalLineThickness);
		AABB3 yLine(
			Vec3(
				(-gridNum * 0.5f + i) * gridSize - lineThickness, 
				-gridNum * 0.5f * gridSize, 
				-lineThickness
			),
			Vec3(
				(-gridNum * 0.5f + i) * gridSize + lineThickness, 
				gridNum * 0.5f * gridSize, 
				lineThickness
			)
		);
		AABB3 xLine(
			Vec3(
				-gridNum * 0.5f * gridSize, 
				(-gridNum * 0.5f + i) * gridSize - lineThickness, 
				-lineThickness),
			Vec3(
				gridNum * 0.5f * gridSize, 
				(-gridNum * 0.5f + i) * gridSize + lineThickness, 
				lineThickness
			)
		);

		AddVertexForAABB3D(tempMesh, yLine,  isEmphasize ? Rgba8::GREEN : lineColor);
		AddVertexForAABB3D(tempMesh, xLine, isEmphasize ? Rgba8::RED : lineColor);
	}

	g_engine->m_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_renderer->BindTexture(nullptr);
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());
}

//---------------------------------------------------------------------------------------------------
void Game::RenderSkySphere() const {
	std::vector<Vertex> tempMesh;
	AddVertexForSphere3D(
		tempMesh,
		m_player->m_playerCamera->GetPosition(),
		1000.f,
		Rgba8::WHITE,
		AABB2::ZERO_TO_ONE,
		32,
		16,
		true
	);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_engine->m_renderer->BindTexture(
		g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/SkySphere.png")
	);
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());
}

//---------------------------------------------------------------------------------------------------
void Game::DecayCameraShake() {
	m_curCameraShakeAmp = GetClamped(m_curCameraShakeAmp - (float)m_gameClock->GetDeltaSeconds() * CAMERA_SHAKE_DECAYSPEED, 0.f, CAMERA_SHAKE_DECAYSPEED);
}