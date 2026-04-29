#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Game/Map.hpp"
#include "Game/PlayerController.hpp"
#include "Game/Actor.hpp"
#include "Game/Weapon.hpp"

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Game/MapDefinition.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Core/VertexUtils.hpp"


//---------------------------------------------------------------------------------------------------
Game::Game()
{
	m_gameClock = new Clock();

	m_UICamera1 = new Camera(
		Vec2(0.f, 0.f), 
		g_gameConfig->m_screenSize
	);

	m_UICamera2 = new Camera(
		Vec2(0.f, 0.f),
		g_gameConfig->m_screenSize
	);

	m_randomGenerator = new RandomNumberGenerator();

	m_curMap = new Map(this, MapDefinition::s_definitions[g_gameConfig->m_defaultMap]);

	InitializeShaders();
	InitializeShaderConstants();
	InitializeSkySphere();
}

//---------------------------------------------------------------------------------------------------
Game::~Game()
{
	delete m_skySphereVertexBuffer;
	m_skySphereVertexBuffer = nullptr;
	delete m_skySphereIndexBuffer;
	m_skySphereIndexBuffer = nullptr;

	delete m_curMap;
	m_curMap = nullptr;

	if (m_playerKeyboardController != nullptr) {
		delete m_playerKeyboardController;
		m_playerKeyboardController = nullptr;
	}
	if (m_playerGamepadController != nullptr) {
		delete m_playerGamepadController;
		m_playerGamepadController = nullptr;
	}
	m_controllerHandleSequence.clear();

	delete m_randomGenerator;
	m_randomGenerator = nullptr;

	delete m_UICamera2;
	m_UICamera2 = nullptr;

	delete m_UICamera1;
	m_UICamera1 = nullptr;

	delete m_gameClock;
	m_gameClock = nullptr;
}

//---------------------------------------------------------------------------------------------------
void Game::Update()
{
	//-----------------------------------------------------------------------------------------------
	//Swap game state
	if (m_nextGameState != m_curGameState) {
		m_curGameState = m_nextGameState;
	}

	//-----------------------------------------------------------------------------------------------
	//Update game mode
	if (m_curGameState == GAME_PLAYING_MODE) {
		//Update game constants
		g_engine->m_renderer->SetGameConstants((float)m_gameClock->GetTotalSeconds());

		//Handle cursor mode  
		if (g_engine->m_devConsole->IsOpen() || !g_engine->m_window->m_isWindowFocus) {
			g_engine->m_input->SetCursorMode(CursorMode::POINTER);
		}
		else {
			g_engine->m_input->SetCursorMode(CursorMode::FPS);
		}

		//-------------------------------------------------------------------------------------------
		if (m_playerKeyboardController != nullptr) {
			m_playerKeyboardController->UpdateInput();
			m_playerKeyboardController->UpdateCamera();
		}
		if (m_playerGamepadController != nullptr) {
			m_playerGamepadController->UpdateInput();
			m_playerGamepadController->UpdateCamera();
		}
		m_curMap->Update();
		UpdateCamerasShake();
		UpdateShaderConstants();
		UpdateUIs();
		UpdateDebugInfo();
	}
	
	//-----------------------------------------------------------------------------------------------
	//Update lobby mode (if needed)

	//-----------------------------------------------------------------------------------------------
	//Update attract mode
	if(m_curGameState == GAME_ATTRACT_MODE) {
		//Handle cursor mode 
		g_engine->m_input->SetCursorMode(CursorMode::POINTER);
		g_engine->m_input->SetFlydigiAdaptiveTrigger();

	}
}

//---------------------------------------------------------------------------------------------------
void Game::Render() const
{
	//-----------------------------------------------------------------------------------------------
	if (m_curGameState == GAME_PLAYING_MODE) {
		if (m_controllerHandleSequence.size() == 1) {
			m_controllerHandleSequence[0]->m_playerCamera->SetViewPort(
				AABB2(
					Vec2(0.f, 0.f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
				)
			);
		}
		else {
			m_controllerHandleSequence[0]->m_playerCamera->SetViewPort(
				AABB2(
					Vec2(0.f, 0.f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y * 0.5f)
				)
			);

			m_controllerHandleSequence[1]->m_playerCamera->SetViewPort(
				AABB2(
					Vec2(0.f, g_engine->m_window->GetClientDimensions().y * 0.5f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
				)
			);
		}
		//-------------------------------------------------------------------------------------------
		g_engine->m_renderer->ClearScreen(Rgba8(50, 50, 50));
		//For ShadowMap
		g_engine->m_renderer->BeginShadowCamera(*(m_curMap->m_sunShadowCamera));

		m_curMap->RenderShadowmap();

		g_engine->m_renderer->EndShadowCamera(*(m_curMap->m_sunShadowCamera));

		for (int i = 0; i < m_controllerHandleSequence.size(); i++) {
			PlayerController* controller = m_controllerHandleSequence[i];
			//For GameScene
			g_engine->m_renderer->BeginCamera(*controller->m_playerCamera);

			RenderSkySphere(*controller->m_playerCamera);
			RenderGamePlay(*controller->m_playerCamera);

			g_engine->m_renderer->EndCamera(*controller->m_playerCamera);

			DebugRenderWorld(*controller->m_playerCamera);

			//-------------------------------------------------------------------------------------------
			//Post processing 
			Vec2 screenDims((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y);
			AABB2 vp = controller->m_playerCamera->GetViewPort();
			Vec4 vpBoundsUV(vp.m_mins.x / screenDims.x, vp.m_mins.y / screenDims.y, vp.m_maxs.x / screenDims.x, vp.m_maxs.y / screenDims.y);

			g_engine->m_renderer->ResolveSceneToPostProcessing(controller->m_playerCamera->GetViewPort());
			g_engine->m_renderer->SetPostProcessingConstants(
				m_postProcessingCBOs[i]->ScreenProjectionMatrix,
				m_postProcessingCBOs[i]->InverseScreenProjectionMatrix,
				m_postProcessingCBOs[i]->WorldToRendererTransform,
				m_postProcessingCBOs[i]->SSDO_Samples,
				m_postProcessingCBOs[i]->SSDO_NoiseScale,
				m_postProcessingCBOs[i]->SSDO_Radius,
				m_postProcessingCBOs[i]->SSDO_Bias,
				m_postProcessingCBOs[i]->ScreenResolution,
				m_postProcessingCBOs[i]->CameraNear,
				m_postProcessingCBOs[i]->CameraFar,
				vpBoundsUV,
				Rgba8::BLACK,
				controller->GetPossessedActor()->m_isDead ? 
					1.f - controller->GetPossessedActor()->m_deadTimer / controller->GetPossessedActor()->m_definition.m_corpseLifetime :
					0.f,
				1.0f,
				0.4f
			);
			g_engine->m_renderer->SetStatesIfChanged();

			//SSDO
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_SSDOShader, SamplerMode::BILINEAR_CLAMP);

			for (int j = 0; j < 8; j++) {
				g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_horizontalBlurWithDepthShader, SamplerMode::BILINEAR_CLAMP);
				g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_verticalBlurWithDepthShader, SamplerMode::BILINEAR_CLAMP);
			}

			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_SSDOBlendShader);
			g_engine->m_renderer->CopyCurPPResultToOriginal();

			//SSGI
			/*
			g_engine->m_renderer->RenderPostProcessing(m_SSGIShader, SamplerMode::BILINEAR_CLAMP);
			for (int i = 0; i < 2; i++) {
				g_engine->m_renderer->RenderPostProcessing(m_horizontalBlurShader, SamplerMode::BILINEAR_CLAMP);
				g_engine->m_renderer->RenderPostProcessing(m_verticalBlurShader, SamplerMode::BILINEAR_CLAMP);
			}
			for (int i = 0; i < 2; i++) {
				g_engine->m_renderer->RenderPostProcessing(m_horizontalBlurWithDepthShader, SamplerMode::BILINEAR_CLAMP);
				g_engine->m_renderer->RenderPostProcessing(m_verticalBlurWithDepthShader, SamplerMode::BILINEAR_CLAMP);
			}
			g_engine->m_renderer->RenderPostProcessing(m_SSGIBlendShader);

			g_engine->m_renderer->CopyCurPPResultToOriginal();*/


			//Fog
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_fogShader, SamplerMode::BILINEAR_CLAMP);
			g_engine->m_renderer->CopyCurPPResultToOriginal();

			//Volume Light
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_volumeLightShader, SamplerMode::BILINEAR_CLAMP);
			g_engine->m_renderer->CopyCurPPResultToOriginal();


			//Bloom
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_brightFilterShader, SamplerMode::BILINEAR_CLAMP);
			for (int j = 0; j < 4; j++) {
				g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_horizontalBlurShader, SamplerMode::BILINEAR_CLAMP);
				g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_verticalBlurShader, SamplerMode::BILINEAR_CLAMP);
			}
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_bloomShader);

			//FXAA
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_FXAAShader, SamplerMode::BILINEAR_CLAMP);

			//Vignette
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_vignetteShader, SamplerMode::BILINEAR_CLAMP);

			g_engine->m_renderer->RenderPostProcessingToBackBuffer(controller->m_playerCamera->GetViewPort());
		}

		//-------------------------------------------------------------------------------------------
		//For UI
		if (m_activePlayerNum == 1) {
			m_UICamera1->SetViewPort(
				AABB2(
					Vec2(0.f, 0.f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
				)
			);


			g_engine->m_renderer->BeginCamera(*m_UICamera1, true);
			RenderGameUI(0);
			g_engine->m_renderer->EndCamera(*m_UICamera1);
		}
		else if (m_activePlayerNum == 2) {
			m_UICamera1->SetViewPort(
				AABB2(
					Vec2(0.f, 0.f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y * 0.5f)
				)
			);
			m_UICamera2->SetViewPort(
				AABB2(
					Vec2(0.f, (float)g_engine->m_window->GetClientDimensions().y * 0.5f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
				)
			);
			g_engine->m_renderer->BeginCamera(*m_UICamera1, true);
			RenderGameUI(0);
			g_engine->m_renderer->EndCamera(*m_UICamera1);

			g_engine->m_renderer->BeginCamera(*m_UICamera2, true);
			RenderGameUI(1);
			g_engine->m_renderer->EndCamera(*m_UICamera2);
		}

		DebugRenderScreen(*(g_app->m_screenCamera));
	}

	//-----------------------------------------------------------------------------------------------
	if (m_curGameState == GAME_LOBBY_MODE) {
		//-------------------------------------------------------------------------------------------
		g_engine->m_renderer->ClearScreen(Rgba8(100, 100, 100));
		if (m_activePlayerNum == 1) {
			m_UICamera1->SetViewPort(
				AABB2(
					Vec2(0.f, 0.f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
				)
			);
			g_engine->m_renderer->BeginCamera(*m_UICamera1, true);
			RenderLobbyMode(0);
			g_engine->m_renderer->EndCamera(*m_UICamera1);
		}
		else if (m_activePlayerNum == 2) {
			m_UICamera1->SetViewPort(
				AABB2(
					Vec2(0.f, 0.f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y * 0.5f)
				)
			);
			m_UICamera2->SetViewPort(
				AABB2(
					Vec2(0.f, (float)g_engine->m_window->GetClientDimensions().y * 0.5f),
					Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
				)
			);

			g_engine->m_renderer->BeginCamera(*m_UICamera1, true);
			RenderLobbyMode(0);
			g_engine->m_renderer->EndCamera(*m_UICamera1);
			g_engine->m_renderer->BeginCamera(*m_UICamera2, true);
			RenderLobbyMode(1);
			g_engine->m_renderer->EndCamera(*m_UICamera2);
		}
	}


	//-----------------------------------------------------------------------------------------------
	if (m_curGameState == GAME_ATTRACT_MODE) {
		//-------------------------------------------------------------------------------------------
		g_engine->m_renderer->ClearScreen(Rgba8(100, 100, 100));
		m_UICamera1->SetViewPort(
			AABB2(
				Vec2(0.f, 0.f),
				Vec2((float)g_engine->m_window->GetClientDimensions().x, (float)g_engine->m_window->GetClientDimensions().y)
			)
		);
		//For AttractMode
		g_engine->m_renderer->BeginCamera(*m_UICamera1,true);

		RenderAttractMode();

		g_engine->m_renderer->EndCamera(*m_UICamera1);
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
void Game::AddCameraShake(float amp, int controllerIndex) {
	if (controllerIndex == 0) {
		m_curCamera1ShakeAmp = GetClamped(m_curCamera1ShakeAmp + amp, 0.f, g_gameConfig->m_cameraShakeAmp);
	}
	else if (controllerIndex == 1) {
		m_curCamera2ShakeAmp = GetClamped(m_curCamera2ShakeAmp + amp, 0.f, g_gameConfig->m_cameraShakeAmp);
	}
}

//--------------------------------------------------------------------------------------------------
void Game::InitializePlayerActors() {
	for (int i = 0; i < m_activePlayerNum; i++) {
		SpawnInfo playerSpawnInfo;
		playerSpawnInfo.m_actorName = "Marine";
		Actor* spawnPoint = m_curMap->GetRandomSpwanPoint();
		playerSpawnInfo.m_spawnPosition = spawnPoint != nullptr ? spawnPoint->m_position : Vec3(0.f, 0.f, 0.f);
		playerSpawnInfo.m_spawnOrientation = spawnPoint != nullptr ? spawnPoint->m_orientation : EulerAngles(0.f, 0.f, 0.f);

		m_curMap->SpawnPlayerActor(playerSpawnInfo, m_controllerHandleSequence[i]);
	}
}

//---------------------------------------------------------------------------------------------------
void Game::InitializeShaders() {
	m_skySphereShader = g_engine->m_renderer->CreateShader("SkySphere", VertexType::PCUTBN);
	m_fogShader = g_engine->m_renderer->CreateShader("Fog", VertexType::PCU);
	m_volumeLightShader = g_engine->m_renderer->CreateShader("VolumeLight", VertexType::PCU);
	m_brightFilterShader = g_engine->m_renderer->CreateShader("BrightFilter", VertexType::PCU);
	m_horizontalBlurShader = g_engine->m_renderer->CreateShader("HorizontalGaussianBlur", VertexType::PCU);
	m_verticalBlurShader = g_engine->m_renderer->CreateShader("VerticalGaussianBlur", VertexType::PCU);
	m_horizontalBlurWithDepthShader = g_engine->m_renderer->CreateShader("HorizontalGaussianBlurWithDepth", VertexType::PCU);
	m_verticalBlurWithDepthShader = g_engine->m_renderer->CreateShader("VerticalGaussianBlurWithDepth", VertexType::PCU);
	m_horizobtalBilateralBlurShader = g_engine->m_renderer->CreateShader("HorizontalBilateralBlur", VertexType::PCU);
	m_verticalBilateralBlurShader = g_engine->m_renderer->CreateShader("VerticalBilateralBlur", VertexType::PCU);
	m_bloomShader = g_engine->m_renderer->CreateShader("Bloom", VertexType::PCU);
	m_SSDOShader = g_engine->m_renderer->CreateShader("SSDO", VertexType::PCU);
	m_SSDOBlendShader = g_engine->m_renderer->CreateShader("SSDOBlend", VertexType::PCU);
	//m_SSGIShader = g_engine->m_renderer->CreateShader("SSGI", VertexType::PCU);
	//m_SSGIBlendShader = g_engine->m_renderer->CreateShader("SSGIBlend", VertexType::PCU);
	m_FXAAShader = g_engine->m_renderer->CreateShader("FXAA", VertexType::PCU);
	m_vignetteShader = g_engine->m_renderer->CreateShader("Vignette", VertexType::PCU);
}

//---------------------------------------------------------------------------------------------------
void Game::InitializeShaderConstants() {
	for (int i = 0; i < 2; i++) {
		m_postProcessingCBOs.push_back(new PostProcessingConstants());
		for (int j = 0; j < 64; ++j)
		{
			Vec3 sample(
				m_randomGenerator->RollRandomFloatZeroToOne() * 2.f - 1.f,
				m_randomGenerator->RollRandomFloatZeroToOne() * 2.f - 1.f,
				m_randomGenerator->RollRandomFloatZeroToOne()
			);

			sample = sample.GetNormalized();
			sample *= m_randomGenerator->RollRandomFloatZeroToOne();

			float scale = (float)j / 64.0f;
			scale = Interpolate(0.01f, 1.0f, scale * scale);
			sample *= scale;

			m_postProcessingCBOs[i]->SSDO_Samples[j] = Vec4(sample.x, sample.y, sample.z, 0.0f);
		}
		m_postProcessingCBOs[i]->SSDO_NoiseScale = Vec2(
			g_gameConfig->m_screenSize.x / 64.f,
			g_gameConfig->m_screenSize.y / 64.f
		);
		m_postProcessingCBOs[i]->SSDO_Radius = 1.28f;
		m_postProcessingCBOs[i]->SSDO_Bias = 0.04f;
		m_postProcessingCBOs[i]->ScreenResolution = g_gameConfig->m_screenSize;
	}
}

//---------------------------------------------------------------------------------------------------
void Game::InitializeSkySphere() {
	AddVertexForSphere3D(
		m_skySphereVerts,
		m_skySphereIndexs,
		Vec3(0.f,0.f,0.f),
		500.f,
		Rgba8::WHITE,
		AABB2::ZERO_TO_ONE,
		32,
		16,
		true
	);

	m_skySphereVertexBuffer = g_engine->m_renderer->CreateVertexBuffer(static_cast<unsigned int>(m_skySphereVerts.size()) * sizeof(Vertex_TBN), sizeof(Vertex_TBN));
	g_engine->m_renderer->CopyCPUToGPU(
		m_skySphereVerts.data(),
		static_cast<unsigned int>(m_skySphereVerts.size() * sizeof(Vertex_TBN)),
		m_skySphereVertexBuffer
	);
	m_skySphereIndexBuffer = g_engine->m_renderer->CreateIndexBuffer(static_cast<unsigned int>(m_skySphereIndexs.size()) * sizeof(unsigned int));
	g_engine->m_renderer->CopyCPUToGPU(
		m_skySphereIndexs.data(),
		static_cast<unsigned int>(m_skySphereIndexs.size() * sizeof(unsigned int)),
		m_skySphereIndexBuffer
	);
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateShaderConstants() {
	for (int i = 0; i < m_controllerHandleSequence.size(); i++) {
		m_postProcessingCBOs[i]->ScreenProjectionMatrix = m_controllerHandleSequence[i]->m_playerCamera->GetProjectionMat();
		m_postProcessingCBOs[i]->InverseScreenProjectionMatrix = m_controllerHandleSequence[i]->m_playerCamera->GetInverseProjectionMat();
		Mat44 worldToRenderer = m_controllerHandleSequence[i]->m_playerCamera->GetCameraToRendererTransform();
		worldToRenderer.Append(m_controllerHandleSequence[i]->m_playerCamera->GetWorldToCameraTransform());
		m_postProcessingCBOs[i]->WorldToRendererTransform = worldToRenderer;
		m_postProcessingCBOs[i]->CameraNear = m_controllerHandleSequence[i]->m_playerCamera->GetPerspNear();
		m_postProcessingCBOs[i]->CameraFar = m_controllerHandleSequence[i]->m_playerCamera->GetPerspFar();
	}
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateCamerasShake() {
	DecayCameraShake();
	Vec3 viewForward;
	Vec3 viewLeft;
	Vec3 viewUp;
	if (m_controllerHandleSequence.size() > 0) {
		m_controllerHandleSequence[0]->m_playerCamera->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(viewForward, viewLeft, viewUp);
		m_controllerHandleSequence[0]->m_playerCamera->Translate3D(
			viewLeft * (m_randomGenerator->RollRandomFloatZeroToOne() * 2.f - 1.f) * m_curCamera1ShakeAmp * (float)m_gameClock->GetDeltaSeconds() +
			viewUp * (m_randomGenerator->RollRandomFloatZeroToOne() * 2.f - 1.f) * m_curCamera1ShakeAmp * (float)m_gameClock->GetDeltaSeconds()
		);
	}
	
	if (m_controllerHandleSequence.size() > 1) {
		m_controllerHandleSequence[1]->m_playerCamera->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(viewForward, viewLeft, viewUp);
		m_controllerHandleSequence[1]->m_playerCamera->Translate3D(
			viewLeft * (m_randomGenerator->RollRandomFloatZeroToOne() * 2.f - 1.f) * m_curCamera2ShakeAmp * (float)m_gameClock->GetDeltaSeconds() +
			viewUp * (m_randomGenerator->RollRandomFloatZeroToOne() * 2.f - 1.f) * m_curCamera2ShakeAmp * (float)m_gameClock->GetDeltaSeconds()
		);
	}
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateDebugInfo() {
	DebugAddScreenText(
		Stringf("<style:color=255,225,0;shadow=true>[Game Clock] Time: %.2f | FPS: %.1f | Time Scale %.2f </style>", 
			m_gameClock->GetTotalSeconds(), 
			1.0 / m_gameClock->GetDeltaSeconds(),
			m_gameClock->GetTimeScale()
		), 
		AABB2(
			Vec2(0.f, g_gameConfig->m_screenSize.y - 15.f),
			Vec2(g_gameConfig->m_screenSize.x - 5.f, g_gameConfig->m_screenSize.y - 5.f)
		),
		10.f,
		Vec2(1.f, 0.5f),
		0.f
	);

	float mapSimulationHours = m_curMap->m_timeOfDay;
	DebugAddScreenText(
		Stringf("<style:color=255,225,0;shadow=true>[Map Simulation Time] %c%c:%c%c:%c%c </style>",
			(mapSimulationHours / 10.f) >= 1.f ? (int)(mapSimulationHours / 10.f) % 10 + '0' : '0',
			(int)mapSimulationHours % 10 + '0',
			(mapSimulationHours * 60.f / 10.f) >= 1.f ? (int)(mapSimulationHours * 60.f / 10.f) % 6 + '0' : '0',
			(int)(mapSimulationHours * 60.f) % 10 + '0',
			(mapSimulationHours * 3600.f / 10.f) >= 1.f ? (int)(mapSimulationHours * 3600.f / 10.f) % 6 + '0' : '0',
			(int)(mapSimulationHours * 3600.f) % 10 + '0'
		),
		AABB2(
			Vec2(0.f, g_gameConfig->m_screenSize.y - 35.f),
			Vec2(g_gameConfig->m_screenSize.x - 5.f, g_gameConfig->m_screenSize.y - 25.f)
		),
		10.f,
		Vec2(1.f, 0.5f),
		0.f
	);
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateUIs() {
	for (int i = 0; i < m_controllerHandleSequence.size(); i++) {
		Actor* possessedActor = m_controllerHandleSequence[i]->GetPossessedActor();
		if (possessedActor != nullptr && possessedActor->m_equippedWeapon != nullptr) {
			Camera* UICamera = i == 0 ? m_UICamera1 : m_UICamera2;
			std::vector<Vertex>* HUDVerts = i == 0 ? &m_HUDVerts1 : &m_HUDVerts2;
			std::vector<Vertex>* rectileVerts = i == 0 ? &m_reticleVerts1 : &m_reticleVerts2;
			std::vector<Vertex>* HUDTextVerts = i == 0 ? &m_HUDTextVerts1 : &m_HUDTextVerts2;

			HUDVerts->clear();
			rectileVerts->clear();
			HUDTextVerts->clear();

			WeaponDefinition weaponDef = possessedActor->m_equippedWeapon->m_definition;
			Vec2 viewBottomLeft = UICamera->GetOrthoBottomLeft();
			Vec2 viewTopRight = UICamera->GetOrthoTopRight();
			Vec2 viewCenter = (viewBottomLeft + viewTopRight) * 0.5f;
			float viewWidth = viewTopRight.x - viewBottomLeft.x;
			float hudHeight = viewWidth / (13.625f * m_controllerHandleSequence.size());

			AddVertexsForAABB2D(
				*HUDVerts,
				AABB2(
					viewBottomLeft,
					Vec2(viewTopRight.x, viewBottomLeft.y + hudHeight)
				),
				Rgba8::WHITE
			);

			Vec2 rectileHalfSize = weaponDef.m_hud.m_reticleSize.GetVec2() * 0.5f;
			AddVertexsForAABB2D(
				*rectileVerts,
				AABB2(
					Vec2(viewCenter.x - rectileHalfSize.x, viewCenter.y - rectileHalfSize.y),
					Vec2(viewCenter.x + rectileHalfSize.x, viewCenter.y + rectileHalfSize.y)
				),
				Rgba8::WHITE
			);

			g_defaultFont->AddVertsForTextBox2D(
				*HUDTextVerts,
				Stringf("%d", m_controllerHandleSequence[i]->m_killCount),
				AABB2(
					viewBottomLeft,
					Vec2(viewBottomLeft.x + viewWidth * 0.133f, hudHeight)
				),
				15.f
			);

			g_defaultFont->AddVertsForTextBox2D(
				*HUDTextVerts,
				Stringf("%s", weaponDef.m_name.data()),
				AABB2(
					Vec2(viewBottomLeft.x + viewWidth * 0.133f, viewBottomLeft.y),
					Vec2(viewBottomLeft.x + viewWidth * 0.24f, hudHeight)
				),
				15.f
			);

			g_defaultFont->AddVertsForTextBox2D(
				*HUDTextVerts,
				Stringf("%.0f/%.0f", possessedActor->m_curHealth, possessedActor->m_definition.m_health),
				AABB2(
					Vec2(viewBottomLeft.x + viewWidth * 0.24f, viewBottomLeft.y),
					Vec2(viewBottomLeft.x + viewWidth * 0.374f, hudHeight)
				),
				15.f
			);

			g_defaultFont->AddVertsForTextBox2D(
				*HUDTextVerts,
				Stringf("%d", m_controllerHandleSequence[i]->m_deadCount),
				AABB2(
					Vec2(viewBottomLeft.x + viewWidth * 0.871f, viewBottomLeft.y),
					Vec2(viewBottomLeft.x + viewWidth, hudHeight)
				),
				15.f
			);
		}
	}
}

//---------------------------------------------------------------------------------------------------
void Game::RenderAttractMode() const {
	Texture* testTexture = g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/TestTransparent.png");

	std::vector<Vertex> tempMesh;
	float time = (float)m_gameClock->GetTotalSeconds();

	//2D ring rendering
	AddVertexsForRing2D(tempMesh, g_gameConfig->m_screenCenter,
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
						float screenX = g_gameConfig->m_screenCenter.x + (rx * perspectiveScale);
						float screenY = g_gameConfig->m_screenCenter.y + (ry * perspectiveScale);

						Vec2 uv_base[4] = { Vec2(1.f, 1.f), Vec2(0.f, 1.f), Vec2(0.f, 0.f), Vec2(1.f, 0.f) };
						Vec2 uv = uv_base[0] * (1.f - u) * (1.f - v) +
							uv_base[1] * u * (1.f - v) +
							uv_base[2] * u * v +
							uv_base[3] * (1.f - u) * v;

						quad[r * 2 + c] = Vertex(Vec3(screenX, screenY, 0.f), Rgba8(255, 255, 255), uv);
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
	std::string text = "Doomenstein";
	float fontSize = 50;
	float shadowSize = 51;
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(g_gameConfig->m_screenCenter.x - text.size() * 0.5f * shadowSize, g_gameConfig->m_screenCenter.y - 0.5f * shadowSize - 130.f),
		shadowSize,
		text,
		Rgba8(50, 50, 50, 255));
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(g_gameConfig->m_screenCenter.x - text.size() * 0.5f * fontSize, g_gameConfig->m_screenCenter.y - 0.5f * fontSize - 125.f),
		fontSize,
		text,
		Rgba8(225, 225, 225, 255));

	text = "-PRESS SPACE PLAY GAME WITH KEYBOARD AND MOUSE-";
	fontSize = 10;
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(g_gameConfig->m_screenCenter.x - text.size() * 0.5f * fontSize, 4.f * fontSize),
		fontSize,
		text,
		Rgba8(250, 250, 250, (unsigned char)(SinDegrees(time * 240.f) * 100.f + 155.f)));
	text = "-PRESS START PLAY GAME WITH GAMEPAD-";
	g_defaultFont->AddVertsForText2D(
		tempMesh,
		Vec2(g_gameConfig->m_screenCenter.x - text.size() * 0.5f * fontSize, 2.f * fontSize),
		fontSize,
		text,
		Rgba8(250, 250, 250, (unsigned char)(SinDegrees(time * 240.f) * 100.f + 155.f)));

	g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
	g_engine->m_renderer->BindTexture(&g_defaultFont->GetTexture());
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());
}

//---------------------------------------------------------------------------------------------------
void Game::RenderLobbyMode(int controllerIndex) const {
	std::vector<Vertex> tempMesh;
	float titleSize = 40;
	g_defaultFont->AddVertsForTextBox2D(
		tempMesh,
		Stringf("Player %d", controllerIndex + 1),
		AABB2(
			Vec2(g_gameConfig->m_screenCenter.x - titleSize, g_gameConfig->m_screenCenter.y - titleSize), 
			Vec2(g_gameConfig->m_screenCenter.x + titleSize, g_gameConfig->m_screenCenter.y + titleSize)
		),
		titleSize,
		Rgba8::WHITE,
		1.f,
		Vec2(0.5f, 0.5f),
		TextBoxMode::OVERRUN
	);
	g_defaultFont->AddVertsForTextBox2D(
		tempMesh,
		m_controllerHandleSequence[controllerIndex]->m_gamepadID!=-1 ? "Gamepad Control" : "Keyboard and Mouse Control",
		AABB2(
			Vec2(g_gameConfig->m_screenCenter.x - titleSize, g_gameConfig->m_screenCenter.y - 2.f * titleSize),
			Vec2(g_gameConfig->m_screenCenter.x + titleSize, g_gameConfig->m_screenCenter.y )
		),
		titleSize * 0.5f,
		Rgba8::WHITE,
		1.f,
		Vec2(0.5f, 0.5f),
		TextBoxMode::OVERRUN
	);

	g_engine->m_renderer->BindTexture(&g_defaultFont->GetTexture());
	g_engine->m_renderer->DrawVertexArray(tempMesh);
}

//---------------------------------------------------------------------------------------------------
void Game::RenderGamePlay(Camera const& viewCamera) const {
	m_curMap->Render(viewCamera);
}

//---------------------------------------------------------------------------------------------------
void Game::RenderGameUI(int controllerIndex) const {
	if (m_controllerHandleSequence[controllerIndex]->GetPossessedActor() != nullptr && m_controllerHandleSequence[controllerIndex]->GetPossessedActor()->m_equippedWeapon != nullptr &&
		m_controllerHandleSequence[controllerIndex]->m_cameraMode != PlayerCameraMode::FREE_CAMERA) {
		WeaponDefinition weaponDef = m_controllerHandleSequence[controllerIndex]->GetPossessedActor()->m_equippedWeapon->m_definition;
		g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

		g_engine->m_renderer->BindTexture(weaponDef.m_hud.m_baseTexture);
		g_engine->m_renderer->DrawVertexArray(m_HUDVerts1);

		g_engine->m_renderer->BindTexture(weaponDef.m_hud.m_reticleTexture);
		g_engine->m_renderer->DrawVertexArray(controllerIndex==0 ? m_reticleVerts1 : m_reticleVerts2);

		g_engine->m_renderer->BindTexture(&g_defaultFont->GetTexture());
		g_engine->m_renderer->DrawVertexArray(controllerIndex==0 ? m_HUDTextVerts1 : m_HUDTextVerts2);
	}
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
void Game::RenderSkySphere(Camera const& viewCamera) const {
	g_engine->m_renderer->BindShader(m_skySphereShader);
	g_engine->m_renderer->SetModelConstants(
		Mat44::MakeTransform3D(
			viewCamera.GetPosition(),
			EulerAngles(),
			Vec3(1.f,1.f,1.f)
		)
	);
	g_engine->m_renderer->DrawIndexedVertexBuffer(m_skySphereVertexBuffer, m_skySphereIndexBuffer, static_cast<unsigned int>(m_skySphereIndexs.size()));
}

//---------------------------------------------------------------------------------------------------
void Game::DecayCameraShake() {
	m_curCamera1ShakeAmp = GetClamped(m_curCamera1ShakeAmp - (float)m_gameClock->GetDeltaSeconds() * g_gameConfig->m_cameraShakeDecay, 0.f, g_gameConfig->m_cameraShakeDecay);
	m_curCamera2ShakeAmp = GetClamped(m_curCamera2ShakeAmp - (float)m_gameClock->GetDeltaSeconds() * g_gameConfig->m_cameraShakeDecay, 0.f, g_gameConfig->m_cameraShakeDecay);
}