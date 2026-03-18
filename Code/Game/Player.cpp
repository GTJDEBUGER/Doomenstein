#include "Game/Player.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Clock.hpp"

//-----------------------------------------------------------------------------------------------
Player::~Player() {
	delete m_playerCamera;
	m_playerCamera = nullptr;

	delete m_playerClock;
	m_playerClock = nullptr;
}

//-----------------------------------------------------------------------------------------------
Player::Player(Game* game, Vec3 startPos) : Entity(game, startPos) {
	m_playerClock = new Clock();
	m_playerCamera = new Camera(SCREEN_ASPECT, 60.f, 0.1f, 1000.f);
}

//-----------------------------------------------------------------------------------------------
void Player::Update(float deltaSeconds) {
	//use independent clock delta seconds 
	deltaSeconds = (float)m_playerClock->GetDeltaSeconds();

	//reset player position and orientation
	if (m_isResetTransform) {
		m_isResetTransform = false;
		m_position = Vec3(0, 0, 0);
		m_orientation = EulerAngles();
	}

	Vec3 curFwd;
	Vec3 curLeft;
	Vec3 curUp;
	m_orientation.GetAsVectors_IFwd_JLeft_KUp(curFwd, curLeft, curUp);
	curUp = Vec3(0, 0, 1); //use world up
	if (m_isRun) {
		m_velocity = (-curFwd * m_moveInput.z - curLeft * m_moveInput.x + curUp * m_moveInput.y) * PLAYER_RUN_MAX_SPEED;
	}
	else {
		m_velocity = (-curFwd * m_moveInput.z - curLeft * m_moveInput.x + curUp * m_moveInput.y) * PLAYER_MOVE_MAX_SPEED;
	}

	m_angularVelocity.m_yawDegrees = m_viewInput.x * PLAYER_VIEW_YAW_SPEED;
	m_angularVelocity.m_pitchDegrees = -m_viewInput.y * PLAYER_VIEW_PITCH_SPEED;
	m_angularVelocity.m_rollDegrees = m_viewRollInput * PLAYER_VIEW_ROLL_SPEED;

	m_position += m_velocity * deltaSeconds;
	m_orientation.m_yawDegrees += m_angularVelocity.m_yawDegrees * deltaSeconds;
	if (m_orientation.m_pitchDegrees + m_angularVelocity.m_pitchDegrees * deltaSeconds >= -85.f &&
		m_orientation.m_pitchDegrees + m_angularVelocity.m_pitchDegrees * deltaSeconds <= 85.f) {
		m_orientation.m_pitchDegrees += m_angularVelocity.m_pitchDegrees * deltaSeconds;
	}
	if (m_orientation.m_rollDegrees + m_angularVelocity.m_rollDegrees * deltaSeconds >= -45.f &&
		m_orientation.m_rollDegrees + m_angularVelocity.m_rollDegrees * deltaSeconds <= 45.f) {
		m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
	}

	m_playerCamera->SetPosition(m_position);
	m_playerCamera->SetOrientation(m_orientation);
}
//-----------------------------------------------------------------------------------------------
void Player::Render() const {
	Entity::Render();
}