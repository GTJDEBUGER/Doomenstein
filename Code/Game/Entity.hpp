#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/Rgba8.hpp"

class Game;

//-----------------------------------------------------------------------------------------------
class Entity {
public:
	virtual ~Entity()=0;
	Entity(Game* game, Vec3 startPos);

	virtual void          Update(float deltaSeconds)                     = 0;
	virtual void          Render() const;
				          
	Vec3                  GetForwardVector() const;
				          
public:
	Game*                 m_game                                         = nullptr;			          
	Vec3                  m_position;
	Vec3                  m_velocity;
	EulerAngles           m_orientation;
	EulerAngles           m_angularVelocity;
	Rgba8                 m_color = Rgba8::WHITE;

	bool                  m_isFlash;
	bool                  m_isGarbage;
};