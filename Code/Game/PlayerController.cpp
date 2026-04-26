#include "Game/PlayerController.hpp"
#include "Game/Map.hpp"
#include "Game/Actor.hpp"
#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Clock.hpp"

//-----------------------------------------------------------------------------------------------
PlayerController::PlayerController(Map* map) :
	Controller(map) {
	m_playerClock = new Clock();
	m_playerCamera = new Camera(g_gameConfig->m_screenAspect, 60.f, 0.1f, 1000.f);

	SpawnInfo playerSpawnInfo;
	playerSpawnInfo.m_actorName = "Marine";
	Actor* spawnPoint = m_map->GetRandomSpwanPoint();
	playerSpawnInfo.m_spawnPosition = spawnPoint != nullptr ? spawnPoint->m_position : Vec3(0.f, 0.f, 0.f);
	playerSpawnInfo.m_spawnOrientation = spawnPoint != nullptr ? spawnPoint->m_orientation : EulerAngles(0.f, 0.f, 0.f);

	m_map->SpawnPlayerActor(playerSpawnInfo, this);
}

//-----------------------------------------------------------------------------------------------
PlayerController::~PlayerController() {
	delete m_playerCamera;
	m_playerCamera = nullptr;

	delete m_playerClock;
	m_playerClock = nullptr;
}

//-----------------------------------------------------------------------------------------------
void PlayerController::UpdateInput() {
	Actor* possessedActor;
	EulerAngles newOrientation;
	Vec3 viewForwardDir;
	Vec3 viewLeftDir;
	Vec3 viewUpDir;

	if (m_possessedActorHandle != nullptr && m_possessedActorHandle->IsValid()) {
		possessedActor = GetPossessedActor();
		if (possessedActor != nullptr) {
			m_playerStates.isGrounded = GetPossessedActor()->m_isGrounded;
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
		possessedActor->MoveInDirection(
			(viewForwardDir * m_inputActions.moveInput.x + viewLeftDir * m_inputActions.moveInput.y).GetNormalized(),
			m_inputActions.isRun ? possessedActor->m_definition.m_physics.m_runSpeed : possessedActor->m_definition.m_physics.m_walkSpeed
		);

		if (m_inputActions.isAttack) {
			possessedActor->Attack(m_orientation.GetForwardDir_IFwd_JLeft_KUp());
		}

		if (m_inputActions.isJump) {
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
		m_playerCamera->SerPerspFOV(60.f);
		break;
	default:
		break;
	}
}


