#include "WeaponPickup.hpp"
#include "Game/Game.hpp"
#include "Game/PlayerController.hpp"
#include "Game/Map.hpp"
#include "Game/Actor.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include <cstdlib>

//-----------------------------------------------------------------------------------------------
WeaponPickup::WeaponPickup(Map* map) : TempActor(map) {
	m_pointLightIntensity = 10.f;
	m_pointLightRadius = 30.f;
	m_pointLightVolumetric = true;

	m_origCenterAlpha = m_centerAlpha;
	m_origEdgeAlpha = m_edgeAlpha;
	m_origLightIntensity = m_pointLightIntensity;
}

//-----------------------------------------------------------------------------------------------
WeaponPickup::~WeaponPickup() {
}

//-----------------------------------------------------------------------------------------------
float WeaponPickup::GetRandomFloat(float min, float max) const {
	return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

//-----------------------------------------------------------------------------------------------
void WeaponPickup::Update(float deltaSeconds) {
	if (m_isDead) return;

	m_runTime += deltaSeconds;

	if (!m_isExploding) {
		CheckPlayerCollision();

		m_particleSpawnTimer -= deltaSeconds;
		if (m_particleSpawnTimer <= 0.f) {
			SpawnParticles(m_particlesPerSpawn);
			m_particleSpawnTimer = m_particleSpawnInterval;
		}
	}
	else {
		m_explosionTimer += deltaSeconds;
		if (m_explosionTimer >= m_explosionDuration) {
			m_isDead = true;
			m_centerAlpha = 0;
			m_edgeAlpha = 0;
			m_pointLightIntensity = 0.f;
			return;
		}

		float fraction = m_explosionTimer / m_explosionDuration;
		m_centerAlpha = (unsigned char)(m_origCenterAlpha * (1.0f - fraction));
		m_edgeAlpha = (unsigned char)(m_origEdgeAlpha * (1.0f - fraction));
		m_pointLightIntensity = m_origLightIntensity * (1.0f - fraction);
	}

	UpdateParticles(deltaSeconds);
}

//-----------------------------------------------------------------------------------------------
void WeaponPickup::CheckPlayerCollision() {
	if (!m_map || !m_map->m_game) return;

	bool isPickedUp = false;

	for (PlayerController* controller : m_map->m_game->m_controllerHandleSequence) {
		if (!controller) continue;

		Actor* player = controller->GetPossessedActor();
		if (!player || player->m_isDead) continue;

		Vec2 playerCenterXY(player->m_position.x, player->m_position.y);
		FloatRange playerZRange(player->m_position.z, player->m_position.z + player->m_definition.m_collision.m_height);

		if (DoZCylinderAndSphereOverlap3D(
			playerCenterXY,
			player->m_definition.m_collision.m_radius,
			playerZRange,
			m_position,
			m_mainBaseRadius)) {
			isPickedUp = true;
			break;
		}
	}

	if (isPickedUp) {
		m_isExploding = true;

		for (PlayerController* controller : m_map->m_game->m_controllerHandleSequence) {
			if (controller) {
				controller->m_isUnlockFishrod = true;
				controller->GetPossessedActor()->EquipWeapon(2);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
void WeaponPickup::SpawnParticles(int count) {
	for (int i = 0; i < count; ++i) {
		WeaponPickupParticle p;

		float angle = GetRandomFloat(0.f, 360.f);
		float dist = GetRandomFloat(m_particleDistMin, m_particleDistMax);

		p.m_startLocalPos = Vec3(0.f, dist * CosDegrees(angle), dist * SinDegrees(angle));
		p.m_currentLocalPos = p.m_startLocalPos;

		p.m_lifetime = GetRandomFloat(m_particleLifetimeMin, m_particleLifetimeMax);
		p.m_baseRadius = GetRandomFloat(m_particleRadiusMin, m_particleRadiusMax);
		p.m_phaseOffset = GetRandomFloat(0.f, 360.f);

		m_particles.push_back(p);
	}
}

//-----------------------------------------------------------------------------------------------
void WeaponPickup::UpdateParticles(float deltaSeconds) {
	for (int i = 0; i < (int)m_particles.size(); ++i) {
		WeaponPickupParticle& p = m_particles[i];
		p.m_normalizedAge += deltaSeconds / p.m_lifetime;

		float t = p.m_normalizedAge;
		if (t > 1.0f) t = 1.0f;

		if (m_isExploding) {
			float outwardMultiplier = 1.0f + (m_explosionTimer / m_explosionDuration) * 5.0f;
			p.m_currentLocalPos = p.m_startLocalPos * outwardMultiplier;
		}
		else {
			p.m_currentLocalPos = p.m_startLocalPos * (1.0f - (t * t));
		}
	}

	for (int i = (int)m_particles.size() - 1; i >= 0; --i) {
		if (m_particles[i].m_normalizedAge >= 1.0f) {
			m_particles.erase(m_particles.begin() + i);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void WeaponPickup::GenerateWobblyBlob(std::vector<Vertex_TBN>& verts, Vec3 const& center, float baseRadius, float time, float phase) const {
	int numSlices = 32;
	Vec3 normal(1.f, 0.f, 0.f);
	Vec3 tangent(0.f, 1.f, 0.f);
	Vec3 bitangent(0.f, 0.f, 1.f);

	Rgba8 centerColor = m_baseColor;
	centerColor.a = m_centerAlpha;

	Rgba8 edgeColor = m_baseColor;
	edgeColor.a = m_edgeAlpha;

	std::vector<Vec3> perimeterPoints;
	for (int i = 0; i < numSlices; ++i) {
		float theta = (float)i * (360.f / (float)numSlices);

		float wiggle = SinDegrees(theta * m_wave1.frequency + time * m_wave1.speed + phase) * m_wave1.amplitude +
			SinDegrees(theta * m_wave2.frequency + time * m_wave2.speed + phase) * m_wave2.amplitude +
			SinDegrees(theta * m_wave3.frequency + time * m_wave3.speed + phase) * m_wave3.amplitude;

		float r = baseRadius * (1.0f + wiggle);
		perimeterPoints.push_back(center + Vec3(0.f, r * CosDegrees(theta), r * SinDegrees(theta)));
	}

	for (int i = 0; i < numSlices; ++i) {
		int nextIndex = (i + 1) % numSlices;

		verts.push_back(Vertex_TBN(center, centerColor, Vec2(0.5f, 0.5f), tangent, bitangent, normal));

		verts.push_back(Vertex_TBN(perimeterPoints[i], edgeColor, Vec2(0.f, 0.f), tangent, bitangent, normal));
		verts.push_back(Vertex_TBN(perimeterPoints[nextIndex], edgeColor, Vec2(0.f, 0.f), tangent, bitangent, normal));
	}
}

//-----------------------------------------------------------------------------------------------
void WeaponPickup::Render(Camera const& viewCamera) const {
	if (m_isDead) return;

	std::vector<Vertex_TBN> tempVerts;

	GenerateWobblyBlob(tempVerts, Vec3(), m_mainBaseRadius, m_runTime, 0.f);

	for (auto const& p : m_particles) {
		float scale = 1.0f;

		if (p.m_normalizedAge < m_particleBurstFraction && m_particleBurstFraction > 0.f) {
			float burstT = p.m_normalizedAge / m_particleBurstFraction;
			scale = burstT * (2.0f - burstT);
		}
		else {
			scale = 1.0f - (p.m_normalizedAge * p.m_normalizedAge);
		}

		GenerateWobblyBlob(tempVerts, p.m_currentLocalPos, p.m_baseRadius * scale, m_runTime, p.m_phaseOffset);
	}

	Mat44 camModelMatrix = viewCamera.GetCameraToWorldTransform();
	Mat44 modelMatrix = GetBillboardTransform(
		BillboardType::FULL_FACING,
		camModelMatrix,
		m_position
	);

	g_engine->m_renderer->SetModelConstants(modelMatrix);
	g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);

	g_engine->m_renderer->BindTexture(nullptr);
	g_engine->m_renderer->DrawVertexArray(tempVerts);

	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}