#pragma once
#include "Game/Controller.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
class Camera;
class Clock;

//-----------------------------------------------------------------------------------------------
struct CameraKeyframe {
	Vec3 m_position;
	EulerAngles m_orientation;
	float m_durationToNext;

	CameraKeyframe(Vec3 pos, EulerAngles ori, float duration)
		: m_position(pos), m_orientation(ori), m_durationToNext(duration) {
	}
};

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
	bool isInWater = false;
};

//-----------------------------------------------------------------------------------------------
class PlayerController : public Controller {
public:
	PlayerController(Map* map);
	~PlayerController() override;

	void Update() override;
	void UpdateInput();
	void UpdateCamera();
	void SetVibration(float leftMotor, float rightMotor, float duration);

	void PlayIntroCinematic(const std::vector<CameraKeyframe>& keyframes);
	void UpdateCinematic(float deltaSeconds);
	
public:
	int                         m_gamepadID = -1;
	Clock*                      m_playerClock = nullptr;
	Camera*                     m_playerCamera = nullptr;
	PlayerInputActions          m_inputActions;
	PlayerStates                m_playerStates;
	Vec3                        m_position = Vec3(0.f, 0.f, 0.f);
	EulerAngles                 m_orientation = EulerAngles(0.f, 0.f, 0.f);
	PlayerCameraMode            m_cameraMode = PlayerCameraMode::ACTOR_CAMERA;
	int                         m_deadCount = 0;
	int                         m_killCount = 0;
	float                       m_leftMotorVibration = 0.f;
	float                       m_rightMotorVibration = 0.f;
	float                       m_vibrationTimer = 0.f;

	bool                        m_isPlayingCinematic = false;
	std::vector<CameraKeyframe> m_cinematicKeyframes;
	int                         m_currentKeyframeIndex = 0;
	float                       m_cinematicTimer = 0.f;

	bool                        m_isUnlockFishrod = false;
	bool                        m_haveFish = false;
	float                       m_curFishSize = 0.f;
	unsigned int                m_fishIndex = 0;
};