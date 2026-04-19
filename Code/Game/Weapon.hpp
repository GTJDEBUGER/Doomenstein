#pragma once
#include "Game/Actor.hpp"
#include "Game/WeaponDefinition.hpp"
#include "Engine/Math/Vec3.hpp"

//-----------------------------------------------------------------------------------------------
class RandomNumberGenerator;
class Clock;

//-----------------------------------------------------------------------------------------------
class Weapon {
public:
	Weapon(WeaponDefinition const& definition, Actor* owner);
	~Weapon();

	void Fire(Vec3 aimDirection);
	Vec3 GetRandomDirectionInCone(Vec3 const& forward, float coneDegrees) const;

public:
	WeaponDefinition const& m_definition;
	Actor* m_owner;

private:
	RandomNumberGenerator* m_randomGenerator;
	Clock* m_fireClock;
};