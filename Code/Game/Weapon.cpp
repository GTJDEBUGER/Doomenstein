#include "Game/Weapon.hpp"
#include "Game/Map.hpp"
#include "Game/ActorDefinition.hpp"
#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Game/Controller.hpp"
#include "Game/PlayerController.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Camera.hpp"

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
	if (m_definition.m_isReuseableProjectile) {
		if (m_activeProjectile != nullptr && !m_activeProjectile->m_isDead && !m_activeProjectile->m_needDestroy) {
			m_isRetrieving = true;
			m_fireClock->Reset();

			if (m_definition.m_sounds.find("Fire") != m_definition.m_sounds.end()) {
				g_engine->m_audio->StartSoundAt(m_definition.m_sounds.at("Fire"), m_owner->m_position, false, g_gameConfig->m_soundEffectVolume);
			}
			return true;
		}
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

			if (m_definition.m_isReuseableProjectile) {
				m_activeProjectile = projectileActor;
				m_isRetrieving = false;
				m_currentRopeLength = m_definition.m_projectileSpeed * 1.5f;
			}

			g_engine->m_audio->StartSoundAt(
				m_definition.m_sounds.at("Fire"),
				m_owner->m_position +
				Vec3(0.f, 0.f, m_owner->m_definition.m_collision.m_height * 0.5f) +
				randomDirection * m_owner->m_definition.m_collision.m_radius,
				false,
				g_gameConfig->m_soundEffectVolume
			);
		}
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
void Weapon::Update(float deltaSeconds) {
	if (m_definition.m_isReuseableProjectile && m_activeProjectile != nullptr) {
		if (m_activeProjectile->m_isDead || m_activeProjectile->m_needDestroy) {
			m_activeProjectile = nullptr;
			m_isRetrieving = false;
			return;
		}

		if (dynamic_cast<PlayerController*>(m_owner->m_controller) == nullptr) {
			return;
		}
		PlayerController* playerController = dynamic_cast<PlayerController*>(m_owner->m_controller);
		Mat44 camModelMatrix = playerController->m_playerCamera->GetCameraToWorldTransform();
		Vec3 cameraPos = playerController->m_playerCamera->GetPosition();
		Vec3 cameraForward;
		Vec3 cameraLeft;
		Vec3 cameraUp;
		playerController->m_playerCamera->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(cameraForward, cameraLeft, cameraUp);
		Vec3 weaponRenderPos = cameraPos + (cameraForward * m_owner->m_definition.m_collision.m_radius * 0.5f) - cameraUp * 0.26f;

		Vec3 ropeStartPos = weaponRenderPos + cameraForward * 0.01f + cameraUp * 0.31f;
		Vec3 ropeEndPos = m_activeProjectile->m_position;
		Vec3 displacement = ropeEndPos - ropeStartPos;
		float distance = displacement.GetLength();

		if (m_isRetrieving) {
			float reelSpeed = m_definition.m_projectileSpeed * 1.5f;
			m_currentRopeLength -= reelSpeed * deltaSeconds;

			Vec3 pullDir = displacement.GetNormalized() * -1.f + Vec3(0.f,0.f,0.5f);
			m_activeProjectile->AddForce(pullDir * 100.f);

			if (distance < m_owner->m_definition.m_collision.m_radius * 2.f || m_currentRopeLength <= 0.f) {
				m_activeProjectile->m_isDead = true;
				m_activeProjectile = nullptr;
				m_isRetrieving = false;

				playerController->m_haveFish = true;
				playerController->m_curFishSize = m_randomGenerator->RollRandomFloatInRange(0.5f, 10.f);
				playerController->m_fishIndex = m_randomGenerator->RollRandomIntInRange(0, 9);
				playerController->GetPossessedActor()->EquipWeapon(3);
				return;
			}
		}

		if (distance > m_currentRopeLength && distance > 0.f) {
			Vec3 dir = displacement.GetNormalized();

			m_activeProjectile->m_position = ropeStartPos + dir * m_currentRopeLength;

			float outwardSpeed = DotProduct3D(m_activeProjectile->m_velocity, dir);
			if (outwardSpeed > 0.f) {
				m_activeProjectile->m_velocity -= dir * outwardSpeed;
			}
		}
	}
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