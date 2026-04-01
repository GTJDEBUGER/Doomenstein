#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Math/Vec2.hpp"
#include <vector>

class Camera;
class RandomNumberGenerator;
class Clock;
class Player;
class Map;
class Shader;
class VertexBuffer;
class IndexBuffer;
struct PostProcessingConstants;

//-----------------------------------------------------------------------------------------------
enum GameState {
	GAME_MODE_NONE = -1,
	GAME_ATTRACT_MODE,
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
	void	                AddCameraShake(float amp);

public:
	Camera*                 m_UICamera                      = nullptr;
	RandomNumberGenerator*  m_randomGenerator               = nullptr;
	Clock*                  m_gameClock                     = nullptr;
	Player*                 m_player                        = nullptr;
	Map*                    m_curMap                        = nullptr;
	Shader*                 m_skySphereShader               = nullptr;
	Shader*                 m_brightFilterShader            = nullptr;
	Shader*                 m_horizontalBlurShader          = nullptr;
	Shader*                 m_verticalBlurShader            = nullptr;
	Shader*                 m_horizontalBlurWithDepthShader = nullptr;
	Shader*                 m_verticalBlurWithDepthShader   = nullptr;
	Shader*                 m_bloomShader                   = nullptr;
	Shader*                 m_pixelizeShader                = nullptr;
	Shader*                 m_SSDOShader                    = nullptr;
	Shader*                 m_SSDOBlendShader               = nullptr;
	Shader*                 m_crosshairShader               = nullptr;
	Shader*                 m_toneMappingShader             = nullptr;

private:
	void                    InitializeShaders();
	void                    InitializeShaderConstants();
	void                    UpdateShaderConstants();
	void                    InitializeSkySphere();
	void                    UpdateSkySphere();
	void                    UpdateCameras();
	void                    UpdateDebugInfo();

	void                    RenderAttractMode() const;
	void                    RenderGamePlay() const;
	void                    RenderGameUI() const;
	void                    RenderWorldGrids() const;
	void                    RenderSkySphere() const;

	void                    DecayCameraShake();
	 
private:
	GameState                 m_curGameState                  = GAME_ATTRACT_MODE;
	GameState                 m_nextGameState                 = GAME_ATTRACT_MODE;
	std::vector<Vertex_TBN>   m_skySphereVerts;
	std::vector<unsigned int> m_skySphereIndexs;
	VertexBuffer*             m_skySphereVertexBuffer         = nullptr;
	IndexBuffer*              m_skySphereIndexBuffer          = nullptr;
	PostProcessingConstants*  m_postProcessingCBO             = nullptr;
						    
	float                   m_curCameraShakeAmp             = 0.f;
};