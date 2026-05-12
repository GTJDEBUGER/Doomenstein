#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"

//-----------------------------------------------------------------------------------------------
class Camera;
class Map;

//-----------------------------------------------------------------------------------------------
class TempActor {
public:
	TempActor(Map* map);
	virtual ~TempActor() = 0;
	virtual void Update(float deltaSeconds) = 0;
	virtual void Render(Camera const& viewCamera) const = 0;

public:
	Map*  m_map = nullptr;
	float m_runTime = 0.f;
	bool  m_isDead = false;

	Vec3 m_position = Vec3(0.f, 0.f, 0.f);
	Vec3 m_velocity = Vec3(0.f, 0.f, 0.f);
	Vec3 m_acceleration = Vec3(0.f, 0.f, 0.f);

	Vec3  m_pointLightOffset = Vec3(0.f, 0.f, 0.f);
	Rgba8 m_pointLightColor = Rgba8::WHITE;
	float m_pointLightIntensity = 0.f;
	float m_pointLightRadius = 0.f;
	bool  m_pointLightVolumetric = false;
};