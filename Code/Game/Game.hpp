#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include <vector>

class Camera;
class RandomNumberGenerator;
class Clock;
class PlayerController;
class Map;
class Shader;
class VertexBuffer;
class IndexBuffer;
class Texture;
struct PostProcessingConstants;
struct GameConstants;

//-----------------------------------------------------------------------------------------------
enum GameState {
	GAME_MODE_NONE = -1,
	GAME_ATTRACT_MODE,
	GAME_LOBBY_MODE,
	GAME_PLAYING_MODE
};

//-----------------------------------------------------------------------------------------------
class Game {
public:
	Game();
	~Game();
	void                    Update();
	void	                Render() const;
	void	                SetNextGameState(GameState nextState);
	GameState const         GetCurGameState() const;
	void	                AddCameraShake(float amp, int cameraIndex);
	void                    InitializePlayerActors();
	float                   GetWaterHeightAt(Vec2 const& pos) const;

public:
	Camera* m_UICamera1 = nullptr;
	Camera* m_UICamera2 = nullptr;
	RandomNumberGenerator* m_randomGenerator = nullptr;
	Clock* m_gameClock = nullptr;
	int                     m_activePlayerNum = 0;
	PlayerController* m_playerKeyboardController = nullptr;
	PlayerController* m_playerGamepadController = nullptr;
	std::vector<PlayerController*> m_controllerHandleSequence;
	Map* m_curMap = nullptr;
	Shader* m_skySphereShader = nullptr;
	Shader* m_seaShader = nullptr;
	Shader* m_underwaterShader = nullptr;
	Shader* m_fogShader = nullptr;
	Shader* m_volumeLightShader = nullptr;
	Shader* m_brightFilterShader = nullptr;
	Shader* m_horizontalBlurShader = nullptr;
	Shader* m_verticalBlurShader = nullptr;
	Shader* m_horizontalBlurWithDepthShader = nullptr;
	Shader* m_verticalBlurWithDepthShader = nullptr;
	Shader* m_horizobtalBilateralBlurShader = nullptr;
	Shader* m_verticalBilateralBlurShader = nullptr;
	Shader* m_bloomShader = nullptr;
	Shader* m_SSDOShader = nullptr;
	Shader* m_SSDOBlendShader = nullptr;
	//Shader*                 m_SSGIShader                    = nullptr;
	//Shader*                 m_SSGIBlendShader               = nullptr;
	Shader* m_FXAAShader = nullptr;
	Shader* m_vignetteShader = nullptr;

	bool                    m_isDrawDebug = false;
	std::string             m_nextMusic;

private:
	void                    InitializeShaders();
	void                    InitializeShaderConstants();
	void                    InitializeSkySphere();
	void                    InitializeSea();
	void                    UpdateShaderConstants();
	void                    UpdateCamerasShake();
	void                    UpdateDebugInfo();
	void                    UpdateUIs();
	void                    SubmitGameConstants(float isStencilPass) const;

	void                    RenderAttractMode() const;
	void                    RenderLobbyMode(int controllerIndex) const;
	void                    RenderGamePlay(Camera const& viewCamera) const;
	void                    RenderGameUI(int controllerIndex) const;
	void                    RenderWorldGrids() const;
	void                    RenderSkySphere(Camera const& viewCamera) const;
	void                    RenderSea(Camera const& viewCamera) const;

	void                    DecayCameraShake();

private:
	GameState                 m_curGameState = GAME_ATTRACT_MODE;
	GameState                 m_nextGameState = GAME_ATTRACT_MODE;
	std::vector<Vertex_TBN>   m_skySphereVerts;
	std::vector<unsigned int> m_skySphereIndexs;
	VertexBuffer*             m_skySphereVertexBuffer = nullptr;
	IndexBuffer*              m_skySphereIndexBuffer = nullptr;
	std::vector<Vertex_TBN>   m_seaVerts;
	std::vector<unsigned int> m_seaIndexs;
	VertexBuffer*             m_seaVertexBuffer = nullptr;
	IndexBuffer*              m_seaIndexBuffer = nullptr;
	Texture*                  m_seaNormalTexture = nullptr;
	Texture*                  m_seaFoamTexture = nullptr;
	Texture*                  m_seaFoamNormalTexture = nullptr;
	float                     m_seaWaveBaseAngle = 0.f;
	float                     m_seaWaveSpectrum[16][3];
	std::vector<Vec4>         m_seaWavesDirK_Speed_Phase;
	std::vector<Vec4>         m_seaWavesSteep_A_Dx_Dy;
	std::vector<Vec4>         m_seaSpirals_Center_Radius_Intensity;
	std::vector<PostProcessingConstants*> m_postProcessingCBOs;
	GameConstants*            m_gameConstantsCBO = nullptr;
	std::vector<Vertex>       m_cinemaVerts1;
	std::vector<Vertex>       m_cinemaVerts2;
	std::vector<Vertex>       m_HUDVerts1;
	std::vector<Vertex>       m_HUDVerts2;
	std::vector<Vertex>       m_HUDTextVerts1;
	std::vector<Vertex>       m_HUDTextVerts2;
	std::vector<Vertex>       m_reticleVerts1;
	std::vector<Vertex>       m_reticleVerts2;
	std::vector<Vertex>       m_bossHealthVerts1;
	std::vector<Vertex>       m_bossHealthVerts2;

	float                     m_curCamera1ShakeAmp = 0.f;
	float                     m_curCamera2ShakeAmp = 0.f;
	SoundID                   m_curMusicID = 0;
	SoundPlaybackID           m_curMusicPlaybackID = MISSING_SOUND_ID;
	SoundID                   m_curBackgroundMusicID = 0;
	SoundPlaybackID           m_curBackgroundMusicPlaybackID = MISSING_SOUND_ID;
	SoundPlaybackID           m_prevMusicPlaybackID = MISSING_SOUND_ID;
	float                     m_musicFadeTimer = 0.f;
	float                     m_musicFadeDuration = 3.0f;

	float                     m_flythroughDuration = 20.f;
	bool                      m_skyChanged = false;
	float                     m_skyChangeStartTime = 0.f;
	float                     m_skyChangeDuration1 = 10.f;
	float                     m_skyChangeDuration2 = 5.f;
};