#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Math/Vec2.hpp"
#include <vector>

class Camera;
class RandomNumberGenerator;
class Clock;
class PlayerController;
class Map;
class Shader;
class VertexBuffer;
class IndexBuffer;
struct PostProcessingConstants;

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

public:
	Camera*                 m_UICamera1                     = nullptr;
	Camera*                 m_UICamera2                     = nullptr;
	RandomNumberGenerator*  m_randomGenerator               = nullptr;
	Clock*                  m_gameClock                     = nullptr;
	int                     m_activePlayerNum               = 0;
	PlayerController*       m_playerKeyboardController      = nullptr;
	PlayerController*       m_playerGamepadController       = nullptr;
	std::vector<PlayerController*> m_controllerHandleSequence;
	Map*                    m_curMap                        = nullptr;
	Shader*                 m_skySphereShader               = nullptr;
	Shader*                 m_fogShader                     = nullptr;
	Shader*                 m_volumeLightShader             = nullptr;
	Shader*                 m_brightFilterShader            = nullptr;
	Shader*                 m_horizontalBlurShader          = nullptr;
	Shader*                 m_verticalBlurShader            = nullptr;
	Shader*                 m_horizontalBlurWithDepthShader = nullptr;
	Shader*                 m_verticalBlurWithDepthShader   = nullptr;
	Shader*                 m_horizobtalBilateralBlurShader = nullptr;
	Shader*                 m_verticalBilateralBlurShader   = nullptr;
	Shader*                 m_bloomShader                   = nullptr;
	Shader*                 m_SSDOShader                    = nullptr;
	Shader*                 m_SSDOBlendShader               = nullptr;
	//Shader*                 m_SSGIShader                    = nullptr;
	//Shader*                 m_SSGIBlendShader               = nullptr;
	Shader*                 m_FXAAShader                    = nullptr;
	Shader*                 m_vignetteShader                = nullptr;

	bool                    m_isDrawDebug                   = false;

private:
	void                    InitializeShaders();
	void                    InitializeShaderConstants();
	void                    InitializeSkySphere();
	void                    UpdateShaderConstants();
	void                    UpdateCamerasShake();
	void                    UpdateDebugInfo();
	void                    UpdateUIs();

	void                    RenderAttractMode() const;
	void                    RenderLobbyMode(int controllerIndex) const;
	void                    RenderGamePlay(Camera const& viewCamera) const;
	void                    RenderGameUI(int controllerIndex) const;
	void                    RenderWorldGrids() const;
	void                    RenderSkySphere(Camera const& viewCamera) const;

	void                    DecayCameraShake();
	 
private:
	GameState                 m_curGameState                   = GAME_ATTRACT_MODE;
	GameState                 m_nextGameState                  = GAME_ATTRACT_MODE;
	std::vector<Vertex_TBN>   m_skySphereVerts;
	std::vector<unsigned int> m_skySphereIndexs;
	VertexBuffer*             m_skySphereVertexBuffer          = nullptr;
	IndexBuffer*              m_skySphereIndexBuffer           = nullptr;
	std::vector<PostProcessingConstants*> m_postProcessingCBOs;
	std::vector<Vertex>       m_HUDVerts1;
	std::vector<Vertex>       m_HUDVerts2;
	std::vector<Vertex>       m_HUDTextVerts1;
	std::vector<Vertex>       m_reticleVerts1;
	std::vector<Vertex>       m_HUDTextVerts2;
	std::vector<Vertex>       m_reticleVerts2;
						    
	float                     m_curCamera1ShakeAmp             = 0.f;
	float                     m_curCamera2ShakeAmp             = 0.f;
};