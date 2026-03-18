#pragma once
#include "Game/Entity.hpp"

//-----------------------------------------------------------------------------------------------
class Camera;
class Clock;

//-----------------------------------------------------------------------------------------------
class Player: public Entity {
public:
	~Player();
	Player(Game* game, Vec3 startPos);

	void          Update(float deltaSeconds) override;
	void          Render() const override;

public:
	bool          m_isResetTransform = false;
	bool          m_isRun = false;
	Vec2          m_viewInput;
	Vec3          m_moveInput;
	float         m_viewRollInput;
	Camera*       m_playerCamera;
	Clock*        m_playerClock;
};