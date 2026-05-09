#include "Game/PlayerController.hpp"
#include "Game/Map.hpp"
#include "Game/Actor.hpp"
#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/Weapon.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Math/MathUtils.hpp"

//-----------------------------------------------------------------------------------------------
PlayerController::PlayerController(Map* map) :
	Controller(map) {
	m_playerClock = new Clock();
	m_playerCamera = new Camera(g_gameConfig->m_screenAspect, 60.f, 0.1f, 1500.f);

	std::vector<CameraKeyframe> introFrames;

	introFrames.push_back(CameraKeyframe(Vec3(17.5f, 79.5f, 9.f), EulerAngles(-0.9f, -0.9f, 0.f), 8.0f));
	introFrames.push_back(CameraKeyframe(Vec3(42.f, 79.5f, 8.6f), EulerAngles(-2.2f, -25.7f, 0.f), 4.0f));
	introFrames.push_back(CameraKeyframe(Vec3(78.3f, 130.5f, 26.5f), EulerAngles(-79.6f, 44.4f, 0.f), 4.0f));
	introFrames.push_back(CameraKeyframe(Vec3(121.5f, 67.2f, 46.8f), EulerAngles(-180.5f, 41.4f, 0.f), 4.0f));
	introFrames.push_back(CameraKeyframe(Vec3(65.4f, 59.6f, 13.5f), EulerAngles(-322.4f, -38.5f, 0.f), 6.0f));
	introFrames.push_back(CameraKeyframe(Vec3(-4.6f, 65.1f, 12.f), EulerAngles(-346.3f, -10.f, 0.f), 2.0f));
	introFrames.push_back(CameraKeyframe(Vec3(7.f, 79.9f, 6.7f), EulerAngles(-360.f, -24.6f, 0.f), 0.5f));

	PlayIntroCinematic(introFrames);
}

//-----------------------------------------------------------------------------------------------
PlayerController::~PlayerController() {
	delete m_playerCamera;
	m_playerCamera = nullptr;

	delete m_playerClock;
	m_playerClock = nullptr;
}

//-----------------------------------------------------------------------------------------------
void PlayerController::Update() {
	if (m_isPlayingCinematic) {
		UpdateCinematic((float)m_playerClock->GetDeltaSeconds());
	}
}

//-----------------------------------------------------------------------------------------------
void PlayerController::UpdateInput() {
	if (m_isPlayingCinematic) {
		if (m_gamepadID != -1 && m_vibrationTimer > 0.f) {
			m_vibrationTimer -= (float)m_playerClock->GetDeltaSeconds();
			g_engine->m_input->GetController(m_gamepadID).SetVibration(m_leftMotorVibration, m_rightMotorVibration);
		}
		return;
	}

	Actor* possessedActor;
	EulerAngles newOrientation;
	Vec3 viewForwardDir;
	Vec3 viewLeftDir;
	Vec3 viewUpDir;

	if (m_possessedActorHandle != nullptr && m_possessedActorHandle->IsValid()) {
		possessedActor = GetPossessedActor();
		if (possessedActor != nullptr) {
			m_playerStates.isGrounded = GetPossessedActor()->m_isGrounded;
			m_playerStates.isInWater = GetPossessedActor()->m_isInWater;
		}
	}

	switch (m_cameraMode)
	{
	case PlayerCameraMode::ACTOR_CAMERA:
		if (m_possessedActorHandle == nullptr || !m_possessedActorHandle->IsValid()) {
			return;
		}
		possessedActor = GetPossessedActor();
		if (possessedActor==nullptr || possessedActor->m_isDead) {
			return;
		}

		newOrientation = m_orientation;
		m_inputActions.viewInput.x *= g_gameConfig->m_playerViewYawSpeed;
		m_inputActions.viewInput.y *= g_gameConfig->m_playerViewPitchSpeed;
		newOrientation.m_yawDegrees += m_inputActions.viewInput.x * (float)m_map->m_game->m_gameClock->GetDeltaSeconds();
		newOrientation.m_pitchDegrees -= m_inputActions.viewInput.y * (float)m_map->m_game->m_gameClock->GetDeltaSeconds();
		if (newOrientation.m_pitchDegrees > 85.f) {
			newOrientation.m_pitchDegrees = 85.f;
		}
		else if (newOrientation.m_pitchDegrees < -85.f) {
			newOrientation.m_pitchDegrees = -85.f;
		}
		m_orientation = newOrientation;


		newOrientation.m_pitchDegrees = 0.f;
		newOrientation.GetAsVectors_IFwd_JLeft_KUp(viewForwardDir, viewLeftDir, viewUpDir);

		possessedActor->TurnInDirection(viewForwardDir, -1.f);

		if (possessedActor->m_isInWater) {
			m_orientation.GetAsVectors_IFwd_JLeft_KUp(viewForwardDir, viewLeftDir, viewUpDir);
		}
		possessedActor->MoveInDirection(
			(viewForwardDir * m_inputActions.moveInput.x + viewLeftDir * m_inputActions.moveInput.y).GetNormalized(),
			m_inputActions.isRun ? possessedActor->m_definition.m_physics.m_runSpeed : possessedActor->m_definition.m_physics.m_walkSpeed
		);

		if (m_inputActions.isAttack) {
			possessedActor->Attack(m_orientation.GetForwardDir_IFwd_JLeft_KUp());
		}

		if (m_inputActions.isJump && !possessedActor->m_isInWater) {
			possessedActor->AddImpulse(Vec3(0.f, 0.f, 25.f));
			possessedActor->m_isGrounded = false;
			m_inputActions.isJump = false;
		}
		break;

	case PlayerCameraMode::FREE_CAMERA:
		newOrientation = m_orientation;
		m_inputActions.viewInput.x *= g_gameConfig->m_playerViewYawSpeed;
		m_inputActions.viewInput.y *= g_gameConfig->m_playerViewPitchSpeed;
		newOrientation.m_yawDegrees += m_inputActions.viewInput.x * (float)m_playerClock->GetDeltaSeconds();
		newOrientation.m_pitchDegrees -= m_inputActions.viewInput.y * (float)m_playerClock->GetDeltaSeconds();
		if (newOrientation.m_pitchDegrees > 85.f) {
			newOrientation.m_pitchDegrees = 85.f;
		}
		else if (newOrientation.m_pitchDegrees < -85.f) {
			newOrientation.m_pitchDegrees = -85.f;
		}
		m_orientation = newOrientation;
		
		m_orientation.GetAsVectors_IFwd_JLeft_KUp(viewForwardDir, viewLeftDir, viewUpDir);
		viewUpDir = Vec3(0.f, 0.f, 1.f);
		m_position +=  (viewForwardDir * m_inputActions.moveInput.x + viewLeftDir * m_inputActions.moveInput.y + viewUpDir * m_inputActions.moveInput.z).GetNormalized() * 
			(m_inputActions.isRun ? g_gameConfig->m_playerRunSpeed : g_gameConfig->m_playerMoveSpeed) *
			(float)m_playerClock->GetDeltaSeconds();

		break;

	default:
		break;
	}

	if (m_gamepadID!=-1) {
		if (m_vibrationTimer > 0.f) {
			m_vibrationTimer -= (float)m_playerClock->GetDeltaSeconds();
			g_engine->m_input->GetController(m_gamepadID).SetVibration(m_leftMotorVibration, m_rightMotorVibration);
		}
		else {
			m_vibrationTimer = 0.f;
			g_engine->m_input->GetController(m_gamepadID).SetVibration(0.f, 0.f);
		}

		if (m_possessedActorHandle != nullptr && m_possessedActorHandle->IsValid()) {
			possessedActor = GetPossessedActor();
			if (possessedActor != nullptr) {
				if (possessedActor->m_equippedWeapon->m_definition.m_name == "Pistol") {
					g_engine->m_input->SetFlydigiAdaptiveTrigger(
						0, 0, 0, 0, 0,
						2, 5, 5, 50, 2
					);
				}
				else if (possessedActor->m_equippedWeapon->m_definition.m_name == "PlasmaRifle") {
					g_engine->m_input->SetFlydigiAdaptiveTrigger(
						0, 0, 0, 0, 0,
						2, 5, 5, 50, 17
					);
				}
				else {
					g_engine->m_input->SetFlydigiAdaptiveTrigger();
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
void PlayerController::UpdateCamera() {
	Actor* possessedActor;
	switch (m_cameraMode)
	{
	case PlayerCameraMode::ACTOR_CAMERA:
		if (m_possessedActorHandle == nullptr || !m_possessedActorHandle->IsValid()) {
			return;
		}
		possessedActor = m_map->GetActorByHandle(*m_possessedActorHandle);
		if (possessedActor == nullptr) {
			return;
		}
		m_playerCamera->SetPosition(possessedActor->m_position + Vec3(0.f,0.f,possessedActor->m_curEyeHeight));
		m_playerCamera->SetOrientation(m_orientation);
		m_playerCamera->SerPerspFOV(possessedActor->m_definition.m_actorCamera.m_cameraFOV);
		m_position = possessedActor->m_position + Vec3(0.f, 0.f, possessedActor->m_curEyeHeight);
		break;
	case PlayerCameraMode::FREE_CAMERA:
		m_playerCamera->SetPosition(m_position);
		m_playerCamera->SetOrientation(m_orientation);
		m_playerCamera->SerPerspFOV(75.f);
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------------------------
void PlayerController::SetVibration(float leftMotor, float rightMotor, float duration) {
	m_leftMotorVibration = leftMotor;
	m_rightMotorVibration = rightMotor;
	m_vibrationTimer = duration;
}

//-----------------------------------------------------------------------------------------------
void PlayerController::PlayIntroCinematic(const std::vector<CameraKeyframe>& keyframes) {
	if (keyframes.empty()) return;

	m_cinematicKeyframes = keyframes;
	m_isPlayingCinematic = true;
	m_currentKeyframeIndex = 0;
	m_cinematicTimer = 0.f;

	m_cameraMode = PlayerCameraMode::FREE_CAMERA;

	m_position = keyframes[0].m_position;
	m_orientation = keyframes[0].m_orientation;
}

//-----------------------------------------------------------------------------------------------
void PlayerController::UpdateCinematic(float deltaSeconds) {
	if (m_currentKeyframeIndex >= (int)m_cinematicKeyframes.size() - 1) {
		m_isPlayingCinematic = false;
		m_cameraMode = PlayerCameraMode::ACTOR_CAMERA;
		return;
	}

	m_cinematicTimer += deltaSeconds;

	const CameraKeyframe& currentFrame = m_cinematicKeyframes[m_currentKeyframeIndex];

	if (m_cinematicTimer >= currentFrame.m_durationToNext) {
		m_cinematicTimer -= currentFrame.m_durationToNext;
		m_currentKeyframeIndex++;

		if (m_currentKeyframeIndex >= (int)m_cinematicKeyframes.size() - 1) {
			m_isPlayingCinematic = false;
			m_cameraMode = PlayerCameraMode::ACTOR_CAMERA;
			return;
		}
	}

	float fraction = m_cinematicTimer / m_cinematicKeyframes[m_currentKeyframeIndex].m_durationToNext;

	float easedFraction = fraction;

	const CameraKeyframe& from = m_cinematicKeyframes[m_currentKeyframeIndex];
	const CameraKeyframe& to = m_cinematicKeyframes[m_currentKeyframeIndex + 1];

	m_position.x = Interpolate(from.m_position.x, to.m_position.x, easedFraction);
	m_position.y = Interpolate(from.m_position.y, to.m_position.y, easedFraction);
	m_position.z = Interpolate(from.m_position.z, to.m_position.z, easedFraction);

	float yawDisp = GetShortestAngularDispDegrees(from.m_orientation.m_yawDegrees, to.m_orientation.m_yawDegrees);
	float pitchDisp = GetShortestAngularDispDegrees(from.m_orientation.m_pitchDegrees, to.m_orientation.m_pitchDegrees);
	float rollDisp = GetShortestAngularDispDegrees(from.m_orientation.m_rollDegrees, to.m_orientation.m_rollDegrees);

	m_orientation.m_yawDegrees = from.m_orientation.m_yawDegrees + (yawDisp * easedFraction);
	m_orientation.m_pitchDegrees = from.m_orientation.m_pitchDegrees + (pitchDisp * easedFraction);
	m_orientation.m_rollDegrees = from.m_orientation.m_rollDegrees + (rollDisp * easedFraction);
}
