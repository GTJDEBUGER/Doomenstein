#pragma once
#include "Game/Controller.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"

//-----------------------------------------------------------------------------------------------
class Camera;
class Clock;

enum class PlayerCameraMode {
	ACTOR_CAMERA,
	FREE_CAMERA
};

struct PlayerInputActions {
	Vec3 moveInput;
	Vec2 viewInput;
	bool isRun = false;
	bool isAttack = false;
	bool isJump = false;
};

struct PlayerStates {
	bool isGrounded = false;
};

//-----------------------------------------------------------------------------------------------
class PlayerController : public Controller {
public:
	PlayerController(Map* map);
	~PlayerController() override;

	void UpdateInput();
	void UpdateCamera();
	
public:
	Clock*              m_playerClock = nullptr;
	Camera*             m_playerCamera = nullptr;
	PlayerInputActions  m_inputActions;
	PlayerStates        m_playerStates;
	Vec3                m_position = Vec3(0.f, 0.f, 0.f);
	EulerAngles         m_orientation = EulerAngles(0.f, 0.f, 0.f);
	PlayerCameraMode    m_cameraMode = PlayerCameraMode::ACTOR_CAMERA;
};