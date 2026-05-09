#include "Game/ComplexAIController.hpp"
#include "Game/Actor.hpp"
#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

//-----------------------------------------------------------------------------------------------
ComplexAIController::ComplexAIController(Map* map)
	: Controller(map)
{
	map->m_game->m_nextMusic = g_gameConfig->m_gameMusic0;
}

//-----------------------------------------------------------------------------------------------
ComplexAIController::~ComplexAIController() {
}

//-----------------------------------------------------------------------------------------------
void ComplexAIController::Update() {
	float deltaSeconds = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();
	if (deltaSeconds <= 0.0001f) return;

	float physicsDelta = deltaSeconds > 0.0833f ? 0.0833f : deltaSeconds;

	Actor* head = GetPossessedActor();
	if (head == nullptr || head->m_isDead) {
		return;
	}

	if (m_isInitialize) {
		m_spawnPosition = head->m_position;
		m_isInitialize = false;
	}

	if (m_targetActor == nullptr || m_targetActor->m_isDead) {
		m_targetActor = m_map->GetNearestActor(head, head->m_definition.m_faction);
	}

	if (m_targetActor != nullptr && !m_targetActor->m_isDead) {
		Vec3 targetPos = m_targetActor->m_position;
		targetPos.z += m_targetActor->m_definition.m_actorCamera.m_eyeHeight * 0.5f;

		if (m_lastHealth < 0.f) m_lastHealth = head->m_curHealth;
		float damageTaken = m_lastHealth - head->m_curHealth;
		if (damageTaken > 0.f) {
			m_damageAccumulated += damageTaken;
			m_lastHealth = head->m_curHealth;
		}

		// ---------------------------------------------------------
		// State Machine
		// ---------------------------------------------------------
		if (!m_hasDoneStageTransition && head->m_curHealth <= (head->m_definition.m_health * 0.5f)
			&& m_bossState != BossState::INTRO && m_bossState != BossState::STAGE_TRANSITION) {

			m_bossState = BossState::STAGE_TRANSITION;
			m_stageTransitionPhase = StageTransitionPhase::MOVE_TO_CENTER;
			m_hasDoneStageTransition = true;
			m_stateTimer = 15.f;
			m_map->m_game->m_nextMusic = g_gameConfig->m_gameMusic2;
		}

		if (m_damageAccumulated > 50.f
			&& m_bossState != BossState::INTRO
			&& m_bossState != BossState::STAGE_TRANSITION) {

			m_damageAccumulated = 0.f;
			bool isBelowHalfHealth = head->m_curHealth <= (head->m_definition.m_health * 0.5f);
			float randomAngle = m_map->m_game->m_randomGenerator->RollRandomFloatInRange(0.f, 360.f);

			if (isBelowHalfHealth && m_map->m_game->m_randomGenerator->RollRandomFloatZeroToOne() > 0.25f) {
				m_bossState = BossState::CIRCLE_SHOOT;
				m_divePhase = DivePhase::FLY_UP;
				m_stateTimer = 10.f;
				float offsetDistance = 15.f;
				m_circleCenterOffset = Vec3(CosDegrees(randomAngle) * offsetDistance, SinDegrees(randomAngle) * offsetDistance, 0.f);
			}
			else {
				m_bossState = BossState::DIVE;
				m_divePhase = DivePhase::FLY_UP;
				m_stateTimer = 5.f;
				m_circleAngle = randomAngle;
			}
		}

		if (m_bossState == BossState::CIRCLE_SHOOT) {
			if (m_divePhase == DivePhase::HOVER_CIRCLE) {
				m_stateTimer -= deltaSeconds;
				if (m_stateTimer <= 0.f) m_bossState = BossState::CHASE;
			}
		}
		else if (m_bossState == BossState::DIVE) {
			if (m_divePhase == DivePhase::HOVER_CIRCLE) {
				m_stateTimer -= deltaSeconds;
				if (m_stateTimer <= 0.f) {
					m_divePhase = DivePhase::DASH;
					m_dashDirection = (targetPos - head->m_position).GetNormalized();
					m_stateTimer = 2.0f;
				}
			}
		}

		// ==========================================
		// INTRO 
		// ==========================================
		if (m_bossState == BossState::INTRO) {
			if (m_introPhase == IntroPhase::WAIT) {
				m_stateTimer -= deltaSeconds;
				if (m_stateTimer <= 0.f) {
					m_introPhase = IntroPhase::SPIRAL_UP;
					m_circleAngle = 0.f;
				}
			}
			else if (m_introPhase == IntroPhase::SPIRAL_UP) {
				m_circleAngle -= 90.f * deltaSeconds;

				float spiralRadius = 40.f;

				float targetZ = m_spawnPosition.z + 125.f;

				float verticalLead = 7.5f;

				Vec3 idealPos = Vec3(
					m_spawnPosition.x + CosDegrees(m_circleAngle) * spiralRadius,
					m_spawnPosition.y + SinDegrees(m_circleAngle) * spiralRadius,
					head->m_position.z + verticalLead
				);

				Vec3 toIdeal = idealPos - head->m_position;
				Vec3 moveDir = toIdeal.GetNormalized();

				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta * 2.0f);
				head->AddForce((moveDir * head->m_definition.m_physics.m_runSpeed * 1.5f - head->m_velocity) * head->m_definition.m_physics.m_mass * 15.f);

				if (head->m_position.z >= targetZ) {
					m_introPhase = IntroPhase::RAGE_AT_TOP;
					m_stateTimer = 1.f;
				}
			}
			else if (m_introPhase == IntroPhase::RAGE_AT_TOP) {
				m_stateTimer -= deltaSeconds;

				Vec3 toTarget = targetPos - head->m_position;
				Vec3 toTargetXY = Vec3(toTarget.x, toTarget.y, 0.f);
				if (toTargetXY.GetLengthSquared() > 0.001f) {
					toTargetXY = toTargetXY.GetNormalized();
				}
				else {
					toTargetXY = Vec3(1.f, 0.f, 0.f);
				}

				if (m_stateTimer > 0.25f) {
					Vec3 dipDir = Vec3(toTargetXY.x, toTargetXY.y, -0.8f).GetNormalized();
					head->TurnInDirection(dipDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta * 2.f);

					head->AddForce(dipDir * head->m_definition.m_physics.m_mass * 100.f);
				}
				else {
					Vec3 brakeForce = -head->m_velocity * head->m_definition.m_physics.m_mass * 12.f;
					Vec3 pullUpForce = Vec3(0.f, 0.f, 1.f) * head->m_definition.m_physics.m_mass * 30.f;
					head->AddForce(brakeForce + pullUpForce);

					float time = (float)m_map->m_game->m_gameClock->GetTotalSeconds();
					float tremor = sinf(time * 60.f) * 0.05f;

					Vec3 roarDir = Vec3(toTargetXY.x, toTargetXY.y, 2.0f + tremor).GetNormalized();

					head->TurnInDirection(roarDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta * 5.f);

					head->EquipWeapon(0);
					head->Attack(roarDir);
				}

				if (m_stateTimer <= 0.f) {
					m_bossState = BossState::CHASE;
					m_map->m_game->m_nextMusic = g_gameConfig->m_gameMusic1;
				}
			}
		}
		// ==========================================
		// Stage Transition (50% Health)
		// ==========================================
		else if (m_bossState == BossState::STAGE_TRANSITION) {
			head->EquipWeapon(1);

			if (m_stageTransitionPhase == StageTransitionPhase::MOVE_TO_CENTER) {
				Vec3 toIdeal = m_mapCenterHigh - head->m_position;
				if (toIdeal.GetLength() < 10.f) {
					m_stageTransitionPhase = StageTransitionPhase::SPHERE_ROLL;
					m_sphereTheta = 0.f;
					m_spherePhi = 0.f;
				}

				Vec3 moveDir = toIdeal.GetNormalized();
				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);
				head->AddForce((moveDir * head->m_definition.m_physics.m_runSpeed * 2.f - head->m_velocity) * head->m_definition.m_physics.m_mass * 20.f);
			}
			else if (m_stageTransitionPhase == StageTransitionPhase::SPHERE_ROLL) {
				m_stateTimer -= deltaSeconds;
				if (m_stateTimer <= 0.f) {
					m_bossState = BossState::CHASE;
				}

				m_sphereTheta += 270.f * deltaSeconds;
				m_spherePhi += 140.f * deltaSeconds;

				Vec3 idealPos = m_mapCenterHigh + Vec3(
					CosDegrees(m_sphereTheta) * SinDegrees(m_spherePhi) * m_transitionSphereRadius,
					SinDegrees(m_sphereTheta) * SinDegrees(m_spherePhi) * m_transitionSphereRadius,
					CosDegrees(m_spherePhi) * m_transitionSphereRadius
				);

				Vec3 toIdeal = idealPos - head->m_position;
				Vec3 moveDir = toIdeal.GetNormalized();

				head->AddForce((moveDir * head->m_definition.m_physics.m_runSpeed * 3.f - head->m_velocity) * head->m_definition.m_physics.m_mass * 25.f);

				Vec3 aimDir = (targetPos - head->m_position).GetNormalized();
				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta * 2.f);
				head->Attack(aimDir);
			}
		}

		// ==========================================
		// CHASE
		// ==========================================
		if (m_bossState == BossState::CHASE) {
			head->EquipWeapon(0);

			Vec3 toTarget = targetPos - head->m_position;
			float distToTarget = toTarget.GetLength();

			if (distToTarget > 0.001f) {
				Vec3 moveDir = toTarget / distToTarget;

				float checkDist = head->m_definition.m_collision.m_radius + 2.5f;
				RaycastResult3D forwardRay = m_map->RaycastWorldXY(head->m_position, moveDir, checkDist);

				if (forwardRay.m_didImpact) {
					Vec3 retreatDir = forwardRay.m_impactNormal;
					retreatDir.z = 1.5f;
					retreatDir = retreatDir.GetNormalized();

					head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);

					float targetSpeed = head->m_definition.m_physics.m_runSpeed * 1.5f;
					Vec3 desiredVelocity = retreatDir * targetSpeed;
					Vec3 velocityDiff = desiredVelocity - head->m_velocity;

					head->AddForce(velocityDiff * head->m_definition.m_physics.m_mass * 20.f);
				}
				else {
					head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);

					if (distToTarget > 9.f) {
						float targetSpeed = head->m_definition.m_physics.m_runSpeed;
						Vec3 desiredVelocity = moveDir * targetSpeed;
						Vec3 velocityDiff = desiredVelocity - head->m_velocity;
						head->AddForce(velocityDiff * head->m_definition.m_physics.m_mass * 15.f);
					}
					else {
						head->Attack(moveDir);
					}
				}
			}
		}
		// ==========================================
		// CIRCLE_SHOOT
		// ==========================================
		else if (m_bossState == BossState::CIRCLE_SHOOT) {
			head->EquipWeapon(1);

			float time = (float)m_map->m_game->m_gameClock->GetTotalSeconds();
			float circleRadius = 150.f;
			float heightOffset = 30.f + sinf(time * 3.f) * 4.f;

			Vec3 circleCenter = targetPos + m_circleCenterOffset;
			Vec3 idealPos = circleCenter + Vec3(CosDegrees(m_circleAngle) * circleRadius, SinDegrees(m_circleAngle) * circleRadius, heightOffset);
			Vec3 toIdeal = idealPos - head->m_position;

			if (m_divePhase == DivePhase::FLY_UP) {
				if (toIdeal.z < 5.f && head->m_position.z > targetPos.z + 10.f) {
					m_divePhase = DivePhase::HOVER_CIRCLE;
				}

				Vec3 moveDir = toIdeal.GetNormalized();
				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);
				head->AddForce((moveDir * head->m_definition.m_physics.m_runSpeed * 1.5f - head->m_velocity) * head->m_definition.m_physics.m_mass * 15.f);
			}
			else if (m_divePhase == DivePhase::HOVER_CIRCLE) {
				m_circleAngle += 60.f * deltaSeconds;

				float distToIdeal = toIdeal.GetLength();
				Vec3 aimDir = (targetPos - head->m_position).GetNormalized();

				head->TurnInDirection(aimDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);

				if (distToIdeal > 0.001f) {
					Vec3 moveDir = toIdeal / distToIdeal;
					float targetSpeed = head->m_definition.m_physics.m_runSpeed * 1.2f;
					Vec3 desiredVelocity = moveDir * targetSpeed;
					Vec3 velocityDiff = desiredVelocity - head->m_velocity;
					head->AddForce(velocityDiff * head->m_definition.m_physics.m_mass * 15.f);
				}

				head->Attack(aimDir);
			}
		}
		// ==========================================
		// DIVE
		// ==========================================
		else if (m_bossState == BossState::DIVE) {
			head->EquipWeapon(0);

			if (m_divePhase == DivePhase::FLY_UP) {
				Vec3 idealPos = targetPos + Vec3(0.f, 0.f, 25.f);
				Vec3 toIdeal = idealPos - head->m_position;

				if (toIdeal.z < 5.f && head->m_position.z > targetPos.z + 15.f) {
					m_divePhase = DivePhase::HOVER_CIRCLE;
				}

				Vec3 moveDir = toIdeal.GetNormalized();
				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);
				head->AddForce((moveDir * head->m_definition.m_physics.m_runSpeed * 1.5f - head->m_velocity) * head->m_definition.m_physics.m_mass * 15.f);
			}
			else if (m_divePhase == DivePhase::HOVER_CIRCLE) {
				float time = (float)m_map->m_game->m_gameClock->GetTotalSeconds();
				m_circleAngle += 90.f * deltaSeconds;
				float circleRadius = 200.f;
				float heightOffset = 25.f + sinf(time * 5.f) * 6.f;

				Vec3 idealPos = targetPos + Vec3(CosDegrees(m_circleAngle) * circleRadius, SinDegrees(m_circleAngle) * circleRadius, heightOffset);
				Vec3 toIdeal = idealPos - head->m_position;
				float distToIdeal = toIdeal.GetLength();

				Vec3 aimDir = (targetPos - head->m_position).GetNormalized();
				head->TurnInDirection(aimDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta * 1.5f);

				if (distToIdeal > 0.001f) {
					Vec3 moveDir = toIdeal / distToIdeal;
					float targetSpeed = head->m_definition.m_physics.m_runSpeed * 1.5f;
					Vec3 desiredVelocity = moveDir * targetSpeed;
					Vec3 velocityDiff = desiredVelocity - head->m_velocity;
					head->AddForce(velocityDiff * head->m_definition.m_physics.m_mass * 15.f);
				}
			}
			else if (m_divePhase == DivePhase::DASH) {
				Vec3 moveDir = m_dashDirection;

				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta * 2.f);
				head->AddForce((moveDir * head->m_definition.m_physics.m_runSpeed * 3.f - head->m_velocity) * head->m_definition.m_physics.m_mass * 25.f);

				m_stateTimer -= deltaSeconds;

				if (head->m_position.z < targetPos.z + 2.f || m_stateTimer <= 0.f) {
					head->Attack(moveDir);

					m_divePhase = DivePhase::DASH_RECOVERY;
					m_stateTimer = 1.5f;
				}
			}
			else if (m_divePhase == DivePhase::DASH_RECOVERY) {
				m_stateTimer -= deltaSeconds;

				Vec3 moveDir = m_dashDirection;
				head->TurnInDirection(moveDir, head->m_definition.m_physics.m_turnSpeed * physicsDelta);
				head->AddForce(moveDir * head->m_definition.m_physics.m_mass * 200.f);

				if (m_stateTimer <= 0.f) {
					m_bossState = BossState::CHASE;
				}
			}
		}
	}

	if (head->m_definition.m_physics.m_flying) {
		head->AddForce(-head->m_velocity * head->m_definition.m_physics.m_mass * 3.f);
	}

	// ----------------------------------------------------------------------------
	// Spring chain physics for sub-actors (body segments)
	// ----------------------------------------------------------------------------
	Actor* leader = head;
	float time = (float)m_map->m_game->m_gameClock->GetTotalSeconds();

	for (size_t i = 0; i < m_subActors.size(); ++i) {
		Actor* follower = m_subActors[i];
		if (follower == nullptr || follower->m_isDead) continue;

		if (follower->m_definition.m_physics.m_flying) {
			follower->AddForce(-follower->m_velocity * follower->m_definition.m_physics.m_mass * m_friction);
		}

		float restLength = (leader->m_definition.m_collision.m_radius + follower->m_definition.m_collision.m_radius) * 1.1f;

		Vec3 displacement = leader->m_position - follower->m_position;
		float currentDist = displacement.GetLength();

		if (currentDist > 0.001f) {
			Vec3 dirToLeader = displacement / currentDist;

			float chainFactor = 1.0f - ((float)i / (float)m_subActors.size()) * 0.6f;
			float curSpringK = m_baseSpringK * chainFactor;
			float curAlignK = m_baseAlignK * chainFactor;

			float stretch = currentDist - restLength;

			if (stretch > restLength * 0.8f) {
				stretch = restLength * 0.8f;
				follower->AddForce(dirToLeader * (m_baseSpringK * 2.0f * stretch));
			}

			RaycastResult3D losResult = m_map->RaycastWorldXY(follower->m_position, dirToLeader, currentDist);
			if (losResult.m_didImpact) {
				curSpringK *= 0.15f;
				Vec3 wallNormal = losResult.m_impactNormal;
				Vec3 tangentDir = CrossProduct3D(CrossProduct3D(wallNormal, dirToLeader), wallNormal).GetNormalized();
				follower->AddForce(tangentDir * m_baseSpringK * stretch * 0.5f);
			}

			Vec3 springForce = dirToLeader * (curSpringK * stretch);

			Vec3 relVel = follower->m_velocity - leader->m_velocity;
			float relVelLongMag = DotProduct3D(relVel, dirToLeader);
			Vec3 relVelLong = dirToLeader * relVelLongMag;
			Vec3 relVelLat = relVel - relVelLong;

			relVelLat.z *= 0.1f;

			Vec3 dampingForce = (-m_dampLong * relVelLong) + (-m_dampLat * relVelLat);

			Vec3 leaderForward, leaderLeft, leaderUp;
			leader->m_orientation.GetAsVectors_IFwd_JLeft_KUp(leaderForward, leaderLeft, leaderUp);

			float curAmplitude = m_bodyWaveAmplitude * (1.0f + (float)i * 0.2f);
			float phase = time * m_bodyWaveSpeed - (float)i * m_bodyWavePhase;
			Vec3 waveOffset = leaderUp * (sinf(phase) * curAmplitude);

			Vec3 idealFollowerPos = leader->m_position - leaderForward * restLength + waveOffset;
			Vec3 alignDisplacement = idealFollowerPos - follower->m_position;

			alignDisplacement.z *= 0.15f;

			Vec3 alignForce = alignDisplacement * curAlignK;

			Vec3 totalForce = springForce + dampingForce + alignForce;

			if (totalForce.GetLengthSquared() > maxForce * maxForce) {
				totalForce = totalForce.GetNormalized() * maxForce;
			}

			follower->AddForce(totalForce);
			follower->TurnInDirection(dirToLeader, follower->m_definition.m_physics.m_turnSpeed * physicsDelta);
		}
		leader = follower;
	}
}