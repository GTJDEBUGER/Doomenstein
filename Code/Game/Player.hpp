#pragma once
#include "Game/Game.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"

//-----------------------------------------------------------------------------------------------
class Camera;
class Clock;
struct Vec2;
struct Vec3;
struct EulerAngles;

//-----------------------------------------------------------------------------------------------
class Player {
public:
	~Player();
	Player(Game* game, Vec3 startPos);

	void          Update();
	void          Render() const;

public:
	Game*         m_game;
	Vec3		  m_position;
	EulerAngles   m_orientation;
	Vec3		  m_velocity;
	EulerAngles	  m_angularVelocity;

	bool          m_canMove = true;
	bool          m_isResetTransform = false;
	bool          m_isRun = false;
	Vec2          m_viewInput;
	Vec3          m_moveInput;
	float         m_viewRollInput;
	Camera*       m_playerCamera;
	Clock*        m_playerClock;
};