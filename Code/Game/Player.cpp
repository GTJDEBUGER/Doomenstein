#include "Game/Player.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
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
Player::Player(Game* game, Vec3 startPos) : m_game(game), m_position(startPos) {
	m_playerClock = new Clock();
	m_playerCamera = new Camera(g_gameConfig->m_screenAspect, 60.f, 0.1f, 1000.f);
}

//-----------------------------------------------------------------------------------------------
void Player::Update() {
	//use independent clock delta seconds 
	float deltaSeconds = (float)m_playerClock->GetDeltaSeconds();

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
		m_velocity = (-curFwd * m_moveInput.z - curLeft * m_moveInput.x + curUp * m_moveInput.y) * g_gameConfig->m_playerRunSpeed;
	}
	else {
		m_velocity = (-curFwd * m_moveInput.z - curLeft * m_moveInput.x + curUp * m_moveInput.y) * g_gameConfig->m_playerMoveSpeed;
	}

	m_angularVelocity.m_yawDegrees = m_viewInput.x * g_gameConfig->m_playerViewYawSpeed;
	m_angularVelocity.m_pitchDegrees = -m_viewInput.y * g_gameConfig->m_playerViewPitchSpeed;
	m_angularVelocity.m_rollDegrees = m_viewRollInput * g_gameConfig->m_playerViewRollSpeed;

	if(m_canMove) m_position += m_velocity * deltaSeconds;
	
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
	Vec3 iBias;
	Vec3 jBias;
	Vec3 kBias;
	m_orientation.GetAsVectors_IFwd_JLeft_KUp(iBias, jBias, kBias);
	g_engine->m_renderer->SetModelConstants(
		Mat44(
			iBias,
			jBias,
			kBias,
			m_position
		)
	);
}