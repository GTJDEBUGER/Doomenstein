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
	if (g_gameConfig->m_defaultMap == "SeaMap") {
		InitializeSea();
	}
}

//---------------------------------------------------------------------------------------------------
Game::~Game()
{
	if (m_curMusicPlaybackID != MISSING_SOUND_ID) {
		g_engine->m_audio->StopSound(m_curMusicPlaybackID);
	}
	if (m_curBackgroundMusicPlaybackID != MISSING_SOUND_ID) {
		g_engine->m_audio->StopSound(m_curBackgroundMusicPlaybackID);
	}
	if (m_prevMusicPlaybackID != MISSING_SOUND_ID) {
		g_engine->m_audio->StopSound(m_prevMusicPlaybackID);
	}

	for (PostProcessingConstants* cbo : m_postProcessingCBOs) {
		delete cbo;
	}
	m_postProcessingCBOs.clear();
	delete m_gameConstantsCBO;
	m_gameConstantsCBO = nullptr;

	delete m_seaVertexBuffer;
	m_seaVertexBuffer = nullptr;
	delete m_seaIndexBuffer;
	m_seaIndexBuffer = nullptr;

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
		// Update background music
		SoundID gameMusic = g_engine->m_audio->CreateOrGetSound(m_nextMusic);
		SoundID backgroundMusic = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_environmentBackgroundMusic);

		if (m_curMusicID != gameMusic) {
			if (m_prevMusicPlaybackID != MISSING_SOUND_ID) {
				g_engine->m_audio->StopSound(m_prevMusicPlaybackID);
			}
			m_prevMusicPlaybackID = m_curMusicPlaybackID;

			if (gameMusic != MISSING_SOUND_ID) {
				m_curMusicPlaybackID = g_engine->m_audio->StartSound(gameMusic, true, 0.0f);
			}
			else {
				m_curMusicPlaybackID = MISSING_SOUND_ID;
			}

			m_curMusicID = gameMusic;
			m_musicFadeTimer = m_musicFadeDuration;
		}

		if (m_curBackgroundMusicID != backgroundMusic) {
			if (m_curBackgroundMusicPlaybackID != MISSING_SOUND_ID) {
				g_engine->m_audio->StopSound(m_curBackgroundMusicPlaybackID);
			}
			m_curBackgroundMusicPlaybackID = g_engine->m_audio->StartSound(backgroundMusic, true, g_gameConfig->m_musicVolume * 0.5f);
			m_curBackgroundMusicID = backgroundMusic;
		}

		if (m_musicFadeTimer > 0.0f) {
			float deltaSeconds = (float)m_gameClock->GetDeltaSeconds();
			m_musicFadeTimer -= deltaSeconds;

			float fadeFraction = 1.0f - (m_musicFadeTimer / m_musicFadeDuration);
			fadeFraction = GetClampedZeroToOne(fadeFraction);

			float smoothFade = SmoothStep3(fadeFraction);

			float newVolume = Interpolate(0.0f, g_gameConfig->m_musicVolume, smoothFade);
			float oldVolume = Interpolate(g_gameConfig->m_musicVolume, 0.0f, smoothFade);

			if (m_curMusicPlaybackID != MISSING_SOUND_ID) {
				g_engine->m_audio->SetSoundPlaybackVolume(m_curMusicPlaybackID, newVolume);
			}
			if (m_prevMusicPlaybackID != MISSING_SOUND_ID) {
				g_engine->m_audio->SetSoundPlaybackVolume(m_prevMusicPlaybackID, oldVolume);
			}

			if (m_musicFadeTimer <= 0.0f) {
				if (m_prevMusicPlaybackID != MISSING_SOUND_ID) {
					g_engine->m_audio->StopSound(m_prevMusicPlaybackID);
					m_prevMusicPlaybackID = MISSING_SOUND_ID;
				}
				if (m_curMusicPlaybackID != MISSING_SOUND_ID) {
					g_engine->m_audio->SetSoundPlaybackVolume(m_curMusicPlaybackID, g_gameConfig->m_musicVolume);
				}
			}
		}

		if (m_curMusicPlaybackID != MISSING_SOUND_ID) g_engine->m_audio->SetSoundPlaybackSpeed(m_curMusicPlaybackID, (float)m_gameClock->GetTimeScale());
		if (m_curBackgroundMusicPlaybackID != MISSING_SOUND_ID) g_engine->m_audio->SetSoundPlaybackSpeed(m_curBackgroundMusicPlaybackID, (float)m_gameClock->GetTimeScale());

		//Update game constants
		float runTime = (float)m_gameClock->GetTotalSeconds();
		float linearFlythroughProgress = GetClampedZeroToOne(runTime / m_flythroughDuration);
		float flythroughProgress = SmoothStep3(linearFlythroughProgress);
		m_seaSpirals_Center_Radius_Intensity[0].w = Interpolate(0.f, 25.f, SmoothStop5(linearFlythroughProgress));
		Actor* bossActor = m_curMap->GetActorByHandle(*m_curMap->m_bossActorHandle);
		float skyChangeProgress = 0.f;
		if (bossActor != nullptr) {
			float bossHealthPercent = bossActor->m_curHealth / bossActor->m_definition.m_health;
			if (bossHealthPercent < 0.5f && !m_skyChanged) {
				m_skyChanged = true;
				m_skyChangeStartTime = runTime;
			}
			if (m_skyChanged) {
				float linearSkyChangeProgress = GetClampedZeroToOne((runTime - m_skyChangeStartTime) / m_skyChangeDuration1);
				skyChangeProgress = SmoothStop2(linearSkyChangeProgress);
			}
		}
		else {
			if (m_skyChanged) {
				m_skyChanged = false;
				m_skyChangeStartTime = runTime;
				m_nextMusic = "";
			}
			if (!m_skyChanged) {
				float linearSkyChangeProgress = GetClampedZeroToOne((runTime - m_skyChangeStartTime) / m_skyChangeDuration2);
				skyChangeProgress = SmoothStart2(linearSkyChangeProgress);
			}
		}
		m_gameConstantsCBO->GameRunTime = runTime;
		m_gameConstantsCBO->WeatherCoverage = bossActor != nullptr ? Interpolate(0.45f, 0.2f, flythroughProgress) : Interpolate(0.2f, 0.45f, skyChangeProgress);
		m_gameConstantsCBO->WeatherDensity = bossActor != nullptr ? Interpolate(6.0f, 12.f, flythroughProgress) : Interpolate(12.f, 6.f, skyChangeProgress);
		m_gameConstantsCBO->WeatherAbsorption = bossActor != nullptr ? Interpolate(2.0f, 2.5f, flythroughProgress) : Interpolate(2.5f, 2.0f, skyChangeProgress);
		m_gameConstantsCBO->WeatherDarkness = bossActor != nullptr ? Interpolate(0.4f, 0.1f, flythroughProgress) : Interpolate(0.1f, 0.4f, skyChangeProgress);
		m_gameConstantsCBO->WeatherCloudMinHeight = bossActor != nullptr ? Interpolate(5000.f, 2000.f, flythroughProgress) : Interpolate(2000.f, 5000.f, skyChangeProgress);
		m_gameConstantsCBO->WeatherCloudMaxHeight = bossActor != nullptr ? Interpolate(12000.f, 10000.f, flythroughProgress) : Interpolate(10000.f, 12000.f, skyChangeProgress);
		m_gameConstantsCBO->StormCenter = Vec2(80.f, 80.f);
		m_gameConstantsCBO->StormRadius = bossActor != nullptr ? Interpolate(0.f, 5000.f, skyChangeProgress) : Interpolate(5000.f, 0.f, skyChangeProgress);
		m_gameConstantsCBO->StormTwistStrength = 1.2f;
		m_gameConstantsCBO->StormFunnelDepth = bossActor != nullptr ? Interpolate(0.f, 500.f, skyChangeProgress) : Interpolate(500.f, 0.f, skyChangeProgress);
		m_gameConstantsCBO->StormEyeRadius = bossActor != nullptr ? Interpolate(0.f, 2000.f, skyChangeProgress) : Interpolate(2000.f, 0.f, skyChangeProgress);
		m_curMap->m_sunIntensity = bossActor != nullptr ? Interpolate(0.85f, 0.4f, flythroughProgress) : Interpolate(0.3f, 0.85f, skyChangeProgress);
		m_curMap->m_ambientIntensity = bossActor != nullptr ? Interpolate(0.35f, 0.2f, flythroughProgress) : Interpolate(0.1f, 0.35f, skyChangeProgress);
		m_curMap->m_sunIntensity = bossActor != nullptr ? Interpolate(m_curMap->m_sunIntensity, 0.3f, skyChangeProgress) : m_curMap->m_sunIntensity;
		m_curMap->m_ambientIntensity = bossActor != nullptr ? Interpolate(m_curMap->m_ambientIntensity, 0.1f, skyChangeProgress) : m_curMap->m_ambientIntensity;
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

		if (m_controllerHandleSequence.size() > 0) {
			Camera* playerCam = m_controllerHandleSequence[0]->m_playerCamera;
			Vec3 camPos = playerCam->GetPosition();

			Vec3 camForward, camLeft, camUp;
			playerCam->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(camForward, camLeft, camUp);

			g_engine->m_audio->UpdateListener(0, camPos, camForward, camUp);
		}

		UpdateCamerasShake();
		UpdateShaderConstants();
		UpdateUIs();
		UpdateDebugInfo();
	}

	//-----------------------------------------------------------------------------------------------
	//Update lobby mode (if needed)
	if (m_curGameState == GAME_LOBBY_MODE) {
	}

	//-----------------------------------------------------------------------------------------------
	//Update attract mode
	if (m_curGameState == GAME_ATTRACT_MODE) {
		// Update background music
		if (m_curGameState == GAME_ATTRACT_MODE) {
			m_musicFadeTimer = 0.f;

			if (m_prevMusicPlaybackID != MISSING_SOUND_ID) {
				g_engine->m_audio->StopSound(m_prevMusicPlaybackID);
				m_prevMusicPlaybackID = MISSING_SOUND_ID;
			}

			SoundID mainMenuMusic = g_engine->m_audio->CreateOrGetSound(g_gameConfig->m_mainMenuMusic);

			if (m_curMusicID != mainMenuMusic) {

				if (m_curMusicPlaybackID != MISSING_SOUND_ID) {
					g_engine->m_audio->StopSound(m_curMusicPlaybackID);
				}

				if (m_curBackgroundMusicPlaybackID != MISSING_SOUND_ID) {
					g_engine->m_audio->StopSound(m_curBackgroundMusicPlaybackID);
					m_curBackgroundMusicPlaybackID = MISSING_SOUND_ID;
					m_curBackgroundMusicID = 0;
				}

				m_curMusicPlaybackID = g_engine->m_audio->StartSound(mainMenuMusic, true, g_gameConfig->m_musicVolume);
				m_curMusicID = mainMenuMusic;

				m_nextMusic = g_gameConfig->m_gameMusic0;
			}
		}
		if (m_curMusicPlaybackID != MISSING_SOUND_ID) g_engine->m_audio->SetSoundPlaybackSpeed(m_curMusicPlaybackID, (float)m_gameClock->GetTimeScale());

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
			SubmitGameConstants(0.0f);
			RenderSkySphere(*controller->m_playerCamera);
			RenderGamePlay(*controller->m_playerCamera);
			if (g_gameConfig->m_defaultMap == "SeaMap") {
				g_engine->m_renderer->ResolveCurScene(controller->m_playerCamera->GetViewPort());

				g_engine->m_renderer->SetGameConstants((float)m_gameClock->GetTotalSeconds(), 0.0f);
				g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
				g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
				g_engine->m_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
				RenderSea(*controller->m_playerCamera);

				g_engine->m_renderer->SetGameConstants((float)m_gameClock->GetTotalSeconds(), 1.0f);
				g_engine->m_renderer->SetBlendMode(BlendMode::COLOR_DISABLE);

				g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_FRONT_ZFAIL);
				g_engine->m_renderer->SetDepthMode(DepthMode::Z_FAIL_INCREMENT);
				RenderSea(*controller->m_playerCamera);

				g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK_ZFAIL);
				g_engine->m_renderer->SetDepthMode(DepthMode::Z_FAIL_DECREMENT);
				RenderSea(*controller->m_playerCamera);

			}

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
				Rgba8(0, 0, 0, 200),
				controller->GetPossessedActor()->m_isDead ?
				1.f - controller->GetPossessedActor()->m_deadTimer / controller->GetPossessedActor()->m_definition.m_corpseLifetime :
				0.f,
				1.0f,
				0.4f,
				controller->GetPossessedActor()->m_isDead ?
				1.f - controller->GetPossessedActor()->m_deadTimer / controller->GetPossessedActor()->m_definition.m_corpseLifetime :
				0.f
			);
			g_engine->m_renderer->SetStatesIfChanged();

			//SSDO
			g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_SSDOShader, SamplerMode::BILINEAR_CLAMP);

			for (int j = 0; j < 4; j++) {
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

			//Underwater
			if (g_gameConfig->m_defaultMap == "SeaMap") {
				g_engine->m_renderer->BindTexture(m_seaNormalTexture, TextureSlot::METALLIC);
				g_engine->m_renderer->BindTexture(nullptr, TextureSlot::SHADOWMAP);
				g_engine->m_renderer->SetSamplerMode(SamplerMode::ANISOTROPIC_WARP, SamplerSlot::SLOT1);
				g_engine->m_renderer->RenderPostProcessing(controller->m_playerCamera->GetViewPort(), m_underwaterShader, SamplerMode::BILINEAR_CLAMP);
				g_engine->m_renderer->CopyCurPPResultToOriginal();
			}

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
		g_engine->m_renderer->ClearScreen(Rgba8(0, 0, 0));
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
		g_engine->m_renderer->BeginCamera(*m_UICamera1, true);

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
float Game::GetWaterHeightAt(Vec2 const& pos) const {
	float baseGridZ = -2.5f;

	if (g_gameConfig->m_defaultMap != "SeaMap") {
		return -9999.f;
	}

	float time = (float)m_gameClock->GetTotalSeconds();

	Vec2 spatialWarp(
		sinf(pos.y * 0.002f + time * 0.1f),
		cosf(pos.x * 0.002f - time * 0.1f)
	);
	Vec2 samplePoint = pos + spatialWarp * 15.0f;

	float gerstnerOffsetZ = 0.f;

	int mainWaveIndices[4] = { 0, 5, 10, 1 };

	for (int i = 0; i < 4; i++) {
		int waveIdx = mainWaveIndices[i];
		float waveActiveMask = m_seaWavesSteep_A_Dx_Dy[waveIdx].x > 0.0001f ? 1.0f : 0.0f;

		if (waveActiveMask == 0.0f) continue;

		float f = m_seaWavesDirK_Speed_Phase[waveIdx].x * samplePoint.x +
			m_seaWavesDirK_Speed_Phase[waveIdx].y * samplePoint.y -
			m_seaWavesDirK_Speed_Phase[waveIdx].z * time +
			m_seaWavesDirK_Speed_Phase[waveIdx].w;

		float a = m_seaWavesSteep_A_Dx_Dy[waveIdx].y;
		gerstnerOffsetZ += (a * sinf(f));
	}

	float whirlpoolOffsetZ = 0.f;
	float maxWhirlpoolFalloff = 0.f;

	for (size_t j = 0; j < m_seaSpirals_Center_Radius_Intensity.size(); j++) {
		float intensity = m_seaSpirals_Center_Radius_Intensity[j].w;
		if (std::abs(intensity) < 0.001f) continue;

		Vec2 center(m_seaSpirals_Center_Radius_Intensity[j].x, m_seaSpirals_Center_Radius_Intensity[j].y);
		float radius = m_seaSpirals_Center_Radius_Intensity[j].z;

		if (std::abs(pos.x - center.x) > radius || std::abs(pos.y - center.y) > radius) {
			continue;
		}

		Vec2 offset = pos - center;
		float distSq = offset.x * offset.x + offset.y * offset.y;
		float radSq = radius * radius;
		if (radSq < 0.0001f) radSq = 0.0001f;

		if (distSq < radSq) {
			float dist = sqrtf(distSq);
			float safeRadius = radius < 0.001f ? 0.001f : radius;

			float r_norm = dist / safeRadius;
			if (r_norm < 0.0f) r_norm = 0.0f;

			float f_z = 1.0f - r_norm;
			float falloff_z = f_z * f_z;
			whirlpoolOffsetZ += -intensity * falloff_z;

			float f_suppress = 1.0f - distSq / radSq;
			if (f_suppress < 0.0f) f_suppress = 0.0f;

			float currentFalloff = f_suppress * f_suppress;
			if (currentFalloff > maxWhirlpoolFalloff) {
				maxWhirlpoolFalloff = currentFalloff;
			}
		}
	}

	float waveSuppression = 1.0f + (0.2f - 1.0f) * maxWhirlpoolFalloff;
	float localWaveZ = gerstnerOffsetZ * waveSuppression + whirlpoolOffsetZ;

	return baseGridZ + localWaveZ;
}

//---------------------------------------------------------------------------------------------------
void Game::InitializeShaders() {
	m_skySphereShader = g_engine->m_renderer->CreateShader("SkySphere", VertexType::PCUTBN);
	m_seaShader = g_engine->m_renderer->CreateShader("GerstnerSea", VertexType::PCUTBN);
	m_underwaterShader = g_engine->m_renderer->CreateShader("UnderwaterCamera", VertexType::PCU);
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
	m_gameConstantsCBO = new GameConstants();
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
		Vec3(0.f, 0.f, 0.f),
		725.f,
		Rgba8::WHITE,
		AABB2::ZERO_TO_ONE,
		16,
		8,
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
void Game::InitializeSea() {
	unsigned int xNum = 1500;
	unsigned int yNum = 1500;

	unsigned int gridStartIndex = static_cast<unsigned int>(m_seaVerts.size());

	AddVertexForGrid3D(
		m_seaVerts,
		m_seaIndexs,
		xNum,
		yNum,
		1.f
	);

	unsigned int bottomPointIndex = static_cast<unsigned int>(m_seaVerts.size());

	m_seaVerts.push_back(
		Vertex_TBN(
			Vec3(0.f, 0.f, -6000.f),
			Rgba8::WHITE,
			Vec2(0.5f, 0.5f),
			Vec3(1.f, 0.f, 0.f),
			Vec3(0.f, 1.f, 0.f),
			Vec3(0.f, 0.f, -1.f)
		)
	);

	for (unsigned int x = 0; x < xNum; ++x) {
		m_seaIndexs.push_back(bottomPointIndex);
		m_seaIndexs.push_back(gridStartIndex + x);
		m_seaIndexs.push_back(gridStartIndex + x + 1);
	}

	unsigned int topRowStart = gridStartIndex + yNum * (xNum + 1);
	for (unsigned int x = 0; x < xNum; ++x) {
		m_seaIndexs.push_back(bottomPointIndex);
		m_seaIndexs.push_back(topRowStart + x + 1);
		m_seaIndexs.push_back(topRowStart + x);
	}

	for (unsigned int y = 0; y < yNum; ++y) {
		m_seaIndexs.push_back(bottomPointIndex);
		m_seaIndexs.push_back(gridStartIndex + (y + 1) * (xNum + 1));
		m_seaIndexs.push_back(gridStartIndex + y * (xNum + 1));
	}

	for (unsigned int y = 0; y < yNum; ++y) {
		m_seaIndexs.push_back(bottomPointIndex);
		m_seaIndexs.push_back(gridStartIndex + y * (xNum + 1) + xNum);
		m_seaIndexs.push_back(gridStartIndex + (y + 1) * (xNum + 1) + xNum);
	}

	m_seaVertexBuffer = g_engine->m_renderer->CreateVertexBuffer(static_cast<unsigned int>(m_seaVerts.size()) * sizeof(Vertex_TBN), sizeof(Vertex_TBN));
	g_engine->m_renderer->CopyCPUToGPU(
		m_seaVerts.data(),
		static_cast<unsigned int>(m_seaVerts.size() * sizeof(Vertex_TBN)),
		m_seaVertexBuffer
	);
	m_seaIndexBuffer = g_engine->m_renderer->CreateIndexBuffer(static_cast<unsigned int>(m_seaIndexs.size()) * sizeof(unsigned int));
	g_engine->m_renderer->CopyCPUToGPU(
		m_seaIndexs.data(),
		static_cast<unsigned int>(m_seaIndexs.size() * sizeof(unsigned int)),
		m_seaIndexBuffer
	);
	m_seaNormalTexture = g_engine->m_renderer->CreateTextureFromFile("Data/Images/SeaNormal.png");
	m_seaFoamTexture = g_engine->m_renderer->CreateTextureFromFile("Data/Images/SeaFoam.png");
	m_seaFoamNormalTexture = g_engine->m_renderer->CreateTextureFromFile("Data/Images/SeaFoamNormal.png");

	m_seaWaveBaseAngle = -90.f;

	m_seaWaveSpectrum[0][0] = 120.f; m_seaWaveSpectrum[0][1] = 0.00f; m_seaWaveSpectrum[0][2] = 0.11f;
	m_seaWaveSpectrum[1][0] = 73.f; m_seaWaveSpectrum[1][1] = 0.08f; m_seaWaveSpectrum[1][2] = 0.13f;
	m_seaWaveSpectrum[2][0] = 41.f; m_seaWaveSpectrum[2][1] = -0.08f; m_seaWaveSpectrum[2][2] = 0.14f;
	m_seaWaveSpectrum[3][0] = 23.f; m_seaWaveSpectrum[3][1] = 0.15f; m_seaWaveSpectrum[3][2] = 0.14f;
	m_seaWaveSpectrum[4][0] = 11.f; m_seaWaveSpectrum[4][1] = -0.15f; m_seaWaveSpectrum[4][2] = 0.15f;

	m_seaWaveSpectrum[5][0] = 95.f; m_seaWaveSpectrum[5][1] = -0.95f; m_seaWaveSpectrum[5][2] = 0.08f;
	m_seaWaveSpectrum[6][0] = 53.f; m_seaWaveSpectrum[6][1] = -0.85f; m_seaWaveSpectrum[6][2] = 0.10f;
	m_seaWaveSpectrum[7][0] = 29.f; m_seaWaveSpectrum[7][1] = -1.15f; m_seaWaveSpectrum[7][2] = 0.12f;
	m_seaWaveSpectrum[8][0] = 13.f; m_seaWaveSpectrum[8][1] = -1.00f; m_seaWaveSpectrum[8][2] = 0.13f;
	m_seaWaveSpectrum[9][0] = 6.1f; m_seaWaveSpectrum[9][1] = -1.20f; m_seaWaveSpectrum[9][2] = 0.11f;

	m_seaWaveSpectrum[10][0] = 83.f; m_seaWaveSpectrum[10][1] = 1.15f; m_seaWaveSpectrum[10][2] = 0.07f;
	m_seaWaveSpectrum[11][0] = 61.f; m_seaWaveSpectrum[11][1] = 1.25f; m_seaWaveSpectrum[11][2] = 0.09f;
	m_seaWaveSpectrum[12][0] = 37.f; m_seaWaveSpectrum[12][1] = 1.05f; m_seaWaveSpectrum[12][2] = 0.11f;
	m_seaWaveSpectrum[13][0] = 17.f; m_seaWaveSpectrum[13][1] = 1.30f; m_seaWaveSpectrum[13][2] = 0.12f;
	m_seaWaveSpectrum[14][0] = 8.7f; m_seaWaveSpectrum[14][1] = 1.15f; m_seaWaveSpectrum[14][2] = 0.10f;
	m_seaWaveSpectrum[15][0] = 3.1f; m_seaWaveSpectrum[15][1] = 1.25f; m_seaWaveSpectrum[15][2] = 0.09f;

	for (int i = 0; i < 16; i++) {
		float angle = m_seaWaveBaseAngle + m_seaWaveSpectrum[i][1];
		float dirX = cosf(angle);
		float dirY = sinf(angle);

		float steepness = m_seaWaveSpectrum[i][2] * 0.8f;
		float wavelength = m_seaWaveSpectrum[i][0];

		float k = 2.0f * 3.1415926f / wavelength;
		float c = sqrtf(9.8f / k);
		float a = steepness / k;

		float phaseOffset = m_randomGenerator->RollRandomFloatInRange(0.0f, 2.0f * 3.1415926f);

		m_seaWavesDirK_Speed_Phase.push_back(Vec4(dirX * k, dirY * k, c * k, phaseOffset));
		m_seaWavesSteep_A_Dx_Dy.push_back(Vec4(steepness, a, dirX, dirY));
	}
	m_seaSpirals_Center_Radius_Intensity.push_back(Vec4(80.f, 80.f, 80.f, 0.f));
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateShaderConstants() {
	for (int i = 0; i < m_controllerHandleSequence.size(); i++) {
		m_postProcessingCBOs[i]->ScreenProjectionMatrix = m_controllerHandleSequence[i]->m_playerCamera->GetProjectionMat();
		m_postProcessingCBOs[i]->InverseScreenProjectionMatrix = m_controllerHandleSequence[i]->m_playerCamera->GetInverseProjectionMat();
		Mat44 worldToRenderer = m_controllerHandleSequence[i]->m_playerCamera->GetCameraToRendererTransform();
		worldToRenderer.Append(m_controllerHandleSequence[i]->m_playerCamera->GetWorldToCameraTransform());
		m_postProcessingCBOs[i]->WorldToRendererTransform = worldToRenderer;
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
	if (m_isDrawDebug) {
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
}

//---------------------------------------------------------------------------------------------------
void Game::UpdateUIs() {
	for (int i = 0; i < m_controllerHandleSequence.size(); i++) {
		Actor* possessedActor = m_controllerHandleSequence[i]->GetPossessedActor();
		if (possessedActor != nullptr && possessedActor->m_equippedWeapon != nullptr) {
			Camera* UICamera = i == 0 ? m_UICamera1 : m_UICamera2;
			std::vector<Vertex>* cinemaVerts = i == 0 ? &m_cinemaVerts1 : &m_cinemaVerts2;
			std::vector<Vertex>* HUDVerts = i == 0 ? &m_HUDVerts1 : &m_HUDVerts2;
			std::vector<Vertex>* HUDTextVerts = i == 0 ? &m_HUDTextVerts1 : &m_HUDTextVerts2;
			std::vector<Vertex>* rectileVerts = i == 0 ? &m_reticleVerts1 : &m_reticleVerts2;
			std::vector<Vertex>* bossHealthVerts = i == 0 ? &m_bossHealthVerts1 : &m_bossHealthVerts2;

			cinemaVerts->clear();
			HUDVerts->clear();
			rectileVerts->clear();
			HUDTextVerts->clear();
			bossHealthVerts->clear();

			WeaponDefinition weaponDef = possessedActor->m_equippedWeapon->m_definition;
			Vec2 viewBottomLeft = UICamera->GetOrthoBottomLeft();
			Vec2 viewTopRight = UICamera->GetOrthoTopRight();
			Vec2 viewCenter = (viewBottomLeft + viewTopRight) * 0.5f;
			float viewWidth = viewTopRight.x - viewBottomLeft.x;
			float viewHeight = viewTopRight.y - viewBottomLeft.y;
			float hudHeight = viewWidth / (13.625f * m_controllerHandleSequence.size());

			AddVertexsForAABB2D(
				*cinemaVerts,
				AABB2(
					viewBottomLeft,
					Vec2(viewTopRight.x, viewBottomLeft.y + viewHeight * 0.075f)
				),
				Rgba8::BLACK
			);
			AddVertexsForAABB2D(
				*cinemaVerts,
				AABB2(
					Vec2(viewBottomLeft.x, viewTopRight.y - viewHeight * 0.075f),
					viewTopRight
				),
				Rgba8::BLACK
			);

			if (m_curMap->m_bossActorHandle != nullptr && m_curMap->m_bossActorHandle->IsValid()) {
				Actor* bossActor = m_curMap->GetActorByHandle(*m_curMap->m_bossActorHandle);
				if (bossActor != nullptr) {
					AddVertexsForAABB2D(
						*bossHealthVerts,
						AABB2(
							Vec2(viewCenter.x - 600.f, viewTopRight.y - 10.f),
							Vec2(viewCenter.x + 600.f, viewTopRight.y - 5.f)
						),
						Rgba8(150, 150, 150)
					);
					float healthPercent = bossActor->m_curHealth / bossActor->m_definition.m_health;
					AddVertexsForAABB2D(
						*bossHealthVerts,
						AABB2(
							Vec2(viewCenter.x - 600.f, viewTopRight.y - 10.f),
							Vec2(viewCenter.x - 600.f + 1200.f * healthPercent, viewTopRight.y - 5.f)
						),
						Rgba8(255, 0, 0)
					);
				}
			}

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
				Stringf("%.1f%%", possessedActor->m_curHealth / possessedActor->m_definition.m_health * 100.f),
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
void Game::SubmitGameConstants(float isStencilPass) const {
	if (m_gameConstantsCBO == nullptr) return;

	g_engine->m_renderer->SetGameConstants(
		m_gameConstantsCBO->GameRunTime,
		isStencilPass,
		m_gameConstantsCBO->WeatherCoverage,
		m_gameConstantsCBO->WeatherDensity,
		m_gameConstantsCBO->WeatherAbsorption,
		m_gameConstantsCBO->WeatherDarkness,
		m_gameConstantsCBO->WeatherCloudMinHeight,
		m_gameConstantsCBO->WeatherCloudMaxHeight,
		m_gameConstantsCBO->StormCenter,
		m_gameConstantsCBO->StormRadius,
		m_gameConstantsCBO->StormTwistStrength,
		m_gameConstantsCBO->StormFunnelDepth,
		m_gameConstantsCBO->StormEyeRadius
	);
}

//---------------------------------------------------------------------------------------------------
void Game::RenderAttractMode() const {
	Texture* testTexture = g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/AttractMode.png");

	std::vector<Vertex> tempMesh;
	float time = (float)m_gameClock->GetTotalSeconds();

	AddVertexsForAABB2D(
		tempMesh,
		AABB2(
			Vec2(0.f, 0.f),
			g_gameConfig->m_screenSize
		),
		Rgba8::WHITE
	);

	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_engine->m_renderer->BindTexture(testTexture);
	g_engine->m_renderer->DrawVertexArray((int)tempMesh.size(), tempMesh.data());

	tempMesh.clear();

	std::string text = "-PRESS SPACE PLAY GAME WITH KEYBOARD AND MOUSE-";
	float fontSize = 10;
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
		m_controllerHandleSequence[controllerIndex]->m_gamepadID != -1 ? "Gamepad Control" : "Keyboard and Mouse Control",
		AABB2(
			Vec2(g_gameConfig->m_screenCenter.x - titleSize, g_gameConfig->m_screenCenter.y - 2.f * titleSize),
			Vec2(g_gameConfig->m_screenCenter.x + titleSize, g_gameConfig->m_screenCenter.y)
		),
		titleSize * 0.5f,
		Rgba8::WHITE,
		1.f,
		Vec2(0.5f, 0.5f),
		TextBoxMode::OVERRUN
	);
	g_defaultFont->AddVertsForTextBox2D(
		tempMesh,
		m_controllerHandleSequence[controllerIndex]->m_gamepadID != -1 ? "Press START to start game" : "Press SPACE to start game",
		AABB2(
			Vec2(g_gameConfig->m_screenCenter.x - titleSize, g_gameConfig->m_screenCenter.y - 4.f * titleSize),
			Vec2(g_gameConfig->m_screenCenter.x + titleSize, g_gameConfig->m_screenCenter.y - 2.f * titleSize)
		),
		titleSize * 0.5f,
		Rgba8::WHITE,
		1.f,
		Vec2(0.5f, 0.5f),
		TextBoxMode::OVERRUN
	);
	g_defaultFont->AddVertsForTextBox2D(
		tempMesh,
		m_controllerHandleSequence[controllerIndex]->m_gamepadID != -1 ? "Press BACK to leave game" : "Press ESCAPE to leave game",
		AABB2(
			Vec2(g_gameConfig->m_screenCenter.x - titleSize, g_gameConfig->m_screenCenter.y - 5.f * titleSize),
			Vec2(g_gameConfig->m_screenCenter.x + titleSize, g_gameConfig->m_screenCenter.y - 3.f * titleSize)
		),
		titleSize * 0.5f,
		Rgba8::WHITE,
		1.f,
		Vec2(0.5f, 0.5f),
		TextBoxMode::OVERRUN
	);
	g_defaultFont->AddVertsForTextBox2D(
		tempMesh,
		m_controllerHandleSequence[controllerIndex]->m_gamepadID != -1 ? "Press SPACE to join player with keyboard and mouse control" : "Press START to join player with game pad control",
		AABB2(
			Vec2(g_gameConfig->m_screenCenter.x - titleSize, g_gameConfig->m_screenCenter.y - 6.f * titleSize),
			Vec2(g_gameConfig->m_screenCenter.x + titleSize, g_gameConfig->m_screenCenter.y - 4.f * titleSize)
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
	if (m_controllerHandleSequence[controllerIndex]->GetPossessedActor() != nullptr &&
		m_controllerHandleSequence[controllerIndex]->GetPossessedActor()->m_equippedWeapon != nullptr)
		if (m_controllerHandleSequence[controllerIndex]->m_cameraMode != PlayerCameraMode::FREE_CAMERA) {
			WeaponDefinition weaponDef = m_controllerHandleSequence[controllerIndex]->GetPossessedActor()->m_equippedWeapon->m_definition;
			g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

			g_engine->m_renderer->BindTexture(weaponDef.m_hud.m_baseTexture);
			g_engine->m_renderer->DrawVertexArray(m_HUDVerts1);

			g_engine->m_renderer->BindTexture(weaponDef.m_hud.m_reticleTexture);
			g_engine->m_renderer->DrawVertexArray(controllerIndex == 0 ? m_reticleVerts1 : m_reticleVerts2);

			g_engine->m_renderer->BindTexture(&g_defaultFont->GetTexture());
			g_engine->m_renderer->DrawVertexArray(controllerIndex == 0 ? m_HUDTextVerts1 : m_HUDTextVerts2);

			g_engine->m_renderer->BindTexture(nullptr);
			g_engine->m_renderer->DrawVertexArray(controllerIndex == 0 ? m_bossHealthVerts1 : m_bossHealthVerts2);
		}
		else {
			g_engine->m_renderer->BindTexture(nullptr);
			g_engine->m_renderer->DrawVertexArray(controllerIndex == 0 ? m_cinemaVerts1 : m_cinemaVerts2);
		}
}

//---------------------------------------------------------------------------------------------------
void Game::RenderWorldGrids() const {
	std::vector<Vertex> tempMesh;
	int gridNum = 200;
	float normalLineThickness = 0.02f;
	float gridSize = 5.f;
	Rgba8 lineColor(160, 160, 160);
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

		AddVertexForAABB3D(tempMesh, yLine, isEmphasize ? Rgba8::GREEN : lineColor);
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
			Vec3(1.f, 1.f, 1.f)
		)
	);
	g_engine->m_renderer->DrawIndexedVertexBuffer(m_skySphereVertexBuffer, m_skySphereIndexBuffer, static_cast<unsigned int>(m_skySphereIndexs.size()));
}

//---------------------------------------------------------------------------------------------------
void Game::RenderSea(Camera const& viewCamera) const {
	g_engine->m_renderer->SetWaveConstants(
		(unsigned int)m_seaWavesDirK_Speed_Phase.size(),
		m_seaWavesDirK_Speed_Phase.data(),
		m_seaWavesSteep_A_Dx_Dy.data(),
		(unsigned int)m_seaSpirals_Center_Radius_Intensity.size(),
		m_seaSpirals_Center_Radius_Intensity.data()
	);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP, SamplerSlot::SLOT0);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::ANISOTROPIC_WARP, SamplerSlot::SLOT1);
	g_engine->m_renderer->BindTexture(m_seaNormalTexture, TextureSlot::DIFFUSE_SCREEN);
	g_engine->m_renderer->BindTexture(m_seaFoamTexture, TextureSlot::NORMAL_ORIGINALSCREEN);
	g_engine->m_renderer->BindTexture(m_seaFoamNormalTexture, TextureSlot::AO_SCREENDEPTH);
	g_engine->m_renderer->BindResolvedSceneTextures(
		TextureSlot::PARALLAX_SCREENNORMAL,
		TextureSlot::ROUGHNESS_SCREENDEPTHSTENCIL,
		TextureSlot::METALLIC
	);
	g_engine->m_renderer->BindShader(m_seaShader);
	Vec3 camPos = viewCamera.GetPosition();
	float gridSize = 1.f;
	float snappedX = (float)floor(camPos.x / gridSize) * gridSize;
	float snappedY = (float)floor(camPos.y / gridSize) * gridSize;
	g_engine->m_renderer->SetModelConstants(
		Mat44::MakeTransform3D(
			Vec3(snappedX, snappedY, -2.5f),
			EulerAngles(),
			Vec3(1.f, 1.f, 1.f)
		)
	);
	g_engine->m_renderer->DrawIndexedVertexBuffer(m_seaVertexBuffer, m_seaIndexBuffer, static_cast<unsigned int>(m_seaIndexs.size()));
	g_engine->m_renderer->UnbindResolvedSceneTextures(
		TextureSlot::PARALLAX_SCREENNORMAL,
		TextureSlot::ROUGHNESS_SCREENDEPTHSTENCIL,
		TextureSlot::METALLIC
	);
}

//---------------------------------------------------------------------------------------------------
void Game::DecayCameraShake() {
	m_curCamera1ShakeAmp = GetClamped(m_curCamera1ShakeAmp - (float)m_gameClock->GetDeltaSeconds() * g_gameConfig->m_cameraShakeDecay, 0.f, g_gameConfig->m_cameraShakeDecay);
	m_curCamera2ShakeAmp = GetClamped(m_curCamera2ShakeAmp - (float)m_gameClock->GetDeltaSeconds() * g_gameConfig->m_cameraShakeDecay, 0.f, g_gameConfig->m_cameraShakeDecay);
}