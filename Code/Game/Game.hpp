#pragma once
#include "Game/GameCommon.hpp"
#include "Game/Entity.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include <vector>

class Camera;
class RandomNumberGenerator;
class Clock;
class Player;

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
	Camera*                 m_UICamera                   = nullptr;
	RandomNumberGenerator*  m_randomGenerator            = nullptr;
	Clock*                  m_gameClock                  = nullptr;
	Player*                 m_player                     = nullptr;

private:
	void                    InitialGameEntities();
	void                    UpdateGameEntities();
	void                    DeleteGameEntities();
	void                    UpdateCameras();

	void                    RenderTestAttractMode() const;
	void                    RenderTestGamePlay() const;
	void                    RenderTestGameUI() const;
	void                    RenderWorldGrids() const;
	void                    RenderSkySphere() const;

	void                    DecayCameraShake();

private:
	std::vector<Entity*>    m_gameEntites;
	GameState               m_curGameState                  = GAME_ATTRACT_MODE;
	GameState               m_nextGameState                 = GAME_ATTRACT_MODE;
						    
	float                   m_curCameraShakeAmp             = 0.f;
};