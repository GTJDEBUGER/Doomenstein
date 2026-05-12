#pragma once
#include "Game/TempActor.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
class Texture;
struct Vertex_TBN;

//-----------------------------------------------------------------------------------------------
struct WeaponPickupParticle {
	Vec3 m_startLocalPos;
	Vec3 m_currentLocalPos;
	float m_normalizedAge = 0.f;
	float m_lifetime = 1.0f;
	float m_baseRadius = 0.5f;
	float m_phaseOffset = 0.f;
};

struct WiggleWave {
	float frequency;
	float speed;
	float amplitude;

	WiggleWave(float freq, float spd, float amp) : frequency(freq), speed(spd), amplitude(amp) {}
};

//-----------------------------------------------------------------------------------------------
class WeaponPickup : public TempActor {
public:
	WeaponPickup(Map* map);
	~WeaponPickup();
	void Update(float deltaSeconds);
	void Render(Camera const& viewCamera) const;

private:
	void GenerateWobblyBlob(std::vector<Vertex_TBN>& verts, Vec3 const& center, float baseRadius, float time, float phase) const;
	void SpawnParticles(int count);
	void UpdateParticles(float deltaSeconds);
	float GetRandomFloat(float min, float max) const;
	void CheckPlayerCollision();

public:
	unsigned char m_centerAlpha = 255;
	unsigned char m_edgeAlpha = 160;
	Rgba8         m_baseColor = Rgba8::WHITE;

	float m_mainBaseRadius = 5.f;
	float m_mainPulseSpeed = 90.f;

	WiggleWave m_wave1 = WiggleWave(3.0f, 180.0f, 0.10f);
	WiggleWave m_wave2 = WiggleWave(5.0f, -240.0f, 0.05f);
	WiggleWave m_wave3 = WiggleWave(2.0f, 90.0f, 0.15f);

	float m_particleSpawnInterval = 0.2f;
	int   m_particlesPerSpawn = 2;

	float m_particleDistMin = 8.0f;
	float m_particleDistMax = 12.0f;

	float m_particleLifetimeMin = 0.8f;
	float m_particleLifetimeMax = 1.8f;

	float m_particleRadiusMin = 0.15f;
	float m_particleRadiusMax = 0.4f;

	float m_particleBurstFraction = 0.15f;

	std::vector<WeaponPickupParticle> m_particles;
	float m_particleSpawnTimer = 0.f;

	bool  m_isExploding = false;
	float m_explosionTimer = 0.f;
	float m_explosionDuration = 1.0f;
	unsigned char m_origCenterAlpha = 255;
	unsigned char m_origEdgeAlpha = 160;
	float m_origLightIntensity = 10.f;
};