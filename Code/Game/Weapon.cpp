#include "Game/Weapon.hpp"
#include "Game/Map.hpp"
#include "Game/ActorDefinition.hpp"
#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Engine.hpp"

//-----------------------------------------------------------------------------------------------
Weapon::Weapon(WeaponDefinition const& definition, Actor* owner) :
	m_definition(definition),
	m_owner(owner){
	m_randomGenerator = new RandomNumberGenerator();
	m_fireClock = new Clock(owner->m_map->m_game->m_gameClock);
	m_fireClock->Reset();
}

//-----------------------------------------------------------------------------------------------
Weapon::~Weapon() {
	delete m_randomGenerator;
	m_randomGenerator = nullptr;
}

//-----------------------------------------------------------------------------------------------
bool Weapon::Fire(Vec3 aimDirection) {
	if (m_fireClock->GetTotalSeconds() < m_definition.m_refireTime) {
		return false;
	}
	m_fireClock->Reset();

	//Handle ray attack
	if (m_definition.m_rayCount > 0) {
		for (unsigned int i = 0; i < m_definition.m_rayCount; i++) {
			Vec3 randomDirection = GetRandomDirectionInCone(aimDirection, m_definition.m_rayCone);
			Actor* hitActor = nullptr;
			RaycastResult3D hitActorResult = m_owner->m_map->RaycastWorldActors(
				m_owner->m_position + Vec3(0.f,0.f,m_owner->m_definition.m_actorCamera.m_eyeHeight),
				randomDirection,
				m_definition.m_rayRange,
				m_owner,
				&hitActor
			);
			RaycastResult3D hitWorldXYResult = m_owner->m_map->RaycastWorldXY(
				m_owner->m_position + Vec3(0.f, 0.f, m_owner->m_definition.m_actorCamera.m_eyeHeight),
				randomDirection,
				m_definition.m_rayRange
			);
			RaycastResult3D hitWorldZResult = m_owner->m_map->RaycastWorldZ(
				m_owner->m_position + Vec3(0.f, 0.f, m_owner->m_definition.m_actorCamera.m_eyeHeight),
				randomDirection,
				m_definition.m_rayRange
			);
			RaycastResult3D* nearestResult = &hitActorResult;
			if (hitWorldXYResult.m_didImpact && 
				(nearestResult->m_didImpact && hitWorldXYResult.m_impactDist < nearestResult->m_impactDist) ||
				(!nearestResult->m_didImpact)) {
				nearestResult = &hitWorldXYResult;
			}
			if (hitWorldZResult.m_didImpact &&
				(nearestResult->m_didImpact && hitWorldZResult.m_impactDist < nearestResult->m_impactDist) ||
				(!nearestResult->m_didImpact)) {
				nearestResult = &hitWorldZResult;
			}

			m_owner->m_map->SpawnActor(
				SpawnInfo{
					"PistalFireLight",
					m_owner->m_position +
					Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.725f) +
					aimDirection * m_owner->m_definition.m_collision.m_radius * 1.6f
				}
			);

			if (hitActorResult.m_didImpact && nearestResult == &hitActorResult && hitActor != nullptr) {
				m_owner->m_map->SpawnActor(
					SpawnInfo{
						"BloodSplatter",
						nearestResult->m_impactPos + nearestResult->m_impactNormal*0.05f
					}
				);

				float randomDamage = m_randomGenerator->RollRandomFloatInRange(m_definition.m_rayDamage.m_min, m_definition.m_rayDamage.m_max);
				hitActor->Damage(randomDamage, m_owner);
				hitActor->AddImpulse(randomDirection * m_definition.m_rayImpulse);
			}

			if (nearestResult->m_didImpact) {
				EulerAngles splatterRotation = nearestResult->m_impactNormal.GetOrientationDegrees();
				m_owner->m_map->SpawnActor(
					SpawnInfo{
						"BulletHit",
						nearestResult->m_impactPos + nearestResult->m_impactNormal * 0.1f,
						splatterRotation
					}
				);
			}


			g_engine->m_audio->StartSoundAt(
				m_definition.m_sounds.at("Fire"),
				nearestResult->m_didImpact ? nearestResult->m_impactPos : nearestResult->m_rayStartPos + nearestResult->m_rayFwdNormal * nearestResult->m_rayMaxLength,
				false, 
				g_gameConfig->m_soundEffectVolume
			);
		}
	}

	//Handle projectile attack
	if (m_definition.m_projectileCount > 0) {
		for (unsigned int i = 0; i < m_definition.m_projectileCount; i++) {
			Vec3 randomDirection = GetRandomDirectionInCone(aimDirection, m_definition.m_projectileCone);
			SpawnInfo projectileSpawnInfo;
			projectileSpawnInfo.m_actorName = m_definition.m_projectileActor;
			Actor* projectileActor = m_owner->m_map->SpawnActor(projectileSpawnInfo);
			projectileActor->m_velocity = randomDirection * m_definition.m_projectileSpeed;
			projectileActor->m_owner = m_owner;
			projectileActor->m_position = m_owner->m_position + 
				Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.5f) +
				randomDirection * m_owner->m_definition.m_collision.m_radius;

			g_engine->m_audio->StartSoundAt(
				m_definition.m_sounds.at("Fire"),
				m_owner->m_position +
				Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.5f) +
				randomDirection * m_owner->m_definition.m_collision.m_radius,
				false,
				g_gameConfig->m_soundEffectVolume
			);
		}
		m_owner->m_map->SpawnActor(
			SpawnInfo{
				"PlasmaFireLight",
				m_owner->m_position +
				Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.725f) +
				aimDirection * m_owner->m_definition.m_collision.m_radius * 1.6f
			}
		);
	}

	//Handle melee attack
	if (m_definition.m_meleeCount > 0) {
		for (unsigned int i = 0; i < m_definition.m_meleeCount; i++) {
			std::vector<Actor*> hitActors;

			g_engine->m_audio->StartSoundAt(
				m_definition.m_sounds.at("Fire"),
				m_owner->m_position + Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.5f),
				false,
				g_gameConfig->m_soundEffectVolume * 2.f,
				0.f,
				1.f,
				false,
				5.f,
				20000.f
			);

			if (m_owner->m_map->SectorDetectWorldActors(
				m_owner->m_position + Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.5f),
				aimDirection,
				m_definition.m_meleeRange,
				m_definition.m_meleeArc,
				m_owner,
				&hitActors
			)) {
				for (Actor* hitActor : hitActors) {
					Actor* attackerRoot = m_owner->m_isSubActor ? m_owner->m_owner : m_owner;
					Actor* victimRoot = hitActor->m_isSubActor ? hitActor->m_owner : hitActor;

					if (attackerRoot == victimRoot) {
						continue;
					}

					float randomDamage = m_randomGenerator->RollRandomFloatInRange(m_definition.m_meleeDamage.m_min, m_definition.m_meleeDamage.m_max);
					Vec3 toHitActor = (hitActor->m_position - m_owner->m_position).GetNormalized();
					hitActor->Damage(randomDamage, m_owner);
					hitActor->AddImpulse(toHitActor * m_definition.m_meleeImpulse);

					m_owner->m_map->SpawnActor(
						SpawnInfo{
							"BloodSplatter",
							hitActor->m_position + Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.5f) +
							-toHitActor * hitActor->m_definition.m_collision.m_radius,
						}
					);
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------------------------
Vec3 Weapon::GetRandomDirectionInCone(Vec3 const& forward, float coneDegrees) const {
	EulerAngles randomRotations(
		m_randomGenerator->RollRandomFloatInRange(-coneDegrees, coneDegrees),
		m_randomGenerator->RollRandomFloatInRange(-coneDegrees, coneDegrees),
		0.f
	);
	randomRotations += forward.GetOrientationDegrees();
	return randomRotations.GetForwardDir_IFwd_JLeft_KUp();
}