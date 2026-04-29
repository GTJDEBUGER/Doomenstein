#include "Game/AIController.hpp"
#include "Game/Actor.hpp"
#include "Game/Map.hpp"
#include "Game/Weapon.hpp"
#include "Game/Game.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Clock.hpp"

//-----------------------------------------------------------------------------------------------
AIController::AIController(Map* map)
	: Controller(map)
{
}

//-----------------------------------------------------------------------------------------------
AIController::~AIController() {
}

//-----------------------------------------------------------------------------------------------
void AIController::DamagedBy(Actor* attacker) {
	m_targetActor = attacker;
}

//-----------------------------------------------------------------------------------------------
void AIController::Update() {
	Actor* possessedActor = GetPossessedActor();
	if (possessedActor != nullptr && !possessedActor->m_isDead) {
		if (m_targetActor == nullptr) {
			Actor* nearestEnemyInSight = m_map->GetNearestActor(possessedActor, possessedActor->m_definition.m_faction);
			if (nearestEnemyInSight != nullptr) {
				float squrDistanceToEnemy = (nearestEnemyInSight->m_position - possessedActor->m_position).GetLengthSquared();
				float angleToEnemy = GetAngleDegreesBetweenVectors3D(
					possessedActor->m_orientation.GetForwardDir_IFwd_JLeft_KUp(), 
					nearestEnemyInSight->m_position - possessedActor->m_position
				);
				float squrSightRadius = possessedActor->m_definition.m_actorAI.m_sightRadius * possessedActor->m_definition.m_actorAI.m_sightRadius;
				if (angleToEnemy < possessedActor->m_definition.m_actorAI.m_sightAngle * 0.5f && squrDistanceToEnemy < squrSightRadius) {
					RaycastResult3D result = m_map->RaycastWorldXY(
						possessedActor->m_position + Vec3(0.f, 0.f, possessedActor->m_definition.m_actorCamera.m_eyeHeight),
						(nearestEnemyInSight->m_position - possessedActor->m_position).GetNormalized(),
						(possessedActor->m_position - nearestEnemyInSight->m_position).GetLength()
					);
					if (!result.m_didImpact) {
						m_targetActor = nearestEnemyInSight;
					}
				}
			}
		}
		else if(!m_targetActor->m_isDead){
			float squrDistanceToTarget = (m_targetActor->m_position - possessedActor->m_position).GetLengthSquared();

			float maxAttackDistance = 0.f;
			if (possessedActor->m_equippedWeapon != nullptr) {
				maxAttackDistance = possessedActor->m_equippedWeapon->m_definition.m_meleeRange;
				maxAttackDistance = maxAttackDistance < possessedActor->m_equippedWeapon->m_definition.m_rayRange ? possessedActor->m_equippedWeapon->m_definition.m_rayRange : maxAttackDistance;
				if (possessedActor->m_equippedWeapon->m_definition.m_projectileCount > 0) {
					maxAttackDistance = 20.f * possessedActor->m_definition.m_collision.m_radius;
				}
			}
			maxAttackDistance = maxAttackDistance + m_targetActor->m_definition.m_collision.m_radius;
			maxAttackDistance = maxAttackDistance * maxAttackDistance;

			Vec3 targetDirection = m_targetActor->m_position - possessedActor->m_position;
			targetDirection.z = 0.f;
			targetDirection = targetDirection.GetNormalized();
			Vec3 currentForward = possessedActor->m_orientation.GetForwardDir_IFwd_JLeft_KUp();
			if (currentForward != targetDirection) {
				possessedActor->TurnInDirection(
					targetDirection, 
					possessedActor->m_definition.m_physics.m_turnSpeed * 
					(float)m_map->m_game->m_gameClock->GetDeltaSeconds()
				);
			}

			currentForward = possessedActor->m_orientation.GetForwardDir_IFwd_JLeft_KUp();
			if (squrDistanceToTarget > maxAttackDistance) {
				if (possessedActor->m_curHealth > possessedActor->m_definition.m_health * 0.25f) {
					possessedActor->MoveInDirection(currentForward, possessedActor->m_definition.m_physics.m_walkSpeed);
				}
				else {
					possessedActor->MoveInDirection(currentForward, possessedActor->m_definition.m_physics.m_runSpeed);
				}

				RaycastResult3D faceBlockResult = m_map->RaycastWorldXY(
					possessedActor->m_position + Vec3(0.f, 0.f, possessedActor->m_definition.m_actorCamera.m_eyeHeight),
					currentForward,
					possessedActor->m_definition.m_collision.m_radius * 1.25f
				);
				if (faceBlockResult.m_didImpact && possessedActor->m_isGrounded) {
					possessedActor->AddImpulse(Vec3(0.f, 0.f, 25.f));
					possessedActor->m_isGrounded = false;
				}
			}
			else {
				possessedActor->Attack(targetDirection);
			}
		}
		else {
			m_targetActor = nullptr;
		}
	}
}