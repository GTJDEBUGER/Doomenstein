#pragma once
#include "Game/Controller.hpp"
#include "Game/Actor.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
enum class BossState {
	CHASE,
	CIRCLE_SHOOT,
	DIVE,
	INTRO,
	STAGE_TRANSITION
};

enum class DivePhase {
	FLY_UP,         
	HOVER_CIRCLE,   
	DASH,
	DASH_RECOVERY
};

enum class IntroPhase {
	WAIT,
	SPIRAL_UP,
	RAGE_AT_TOP
};

enum class StageTransitionPhase {
	MOVE_TO_CENTER,
	SPHERE_ROLL
};

//-----------------------------------------------------------------------------------------------
class ComplexAIController : public Controller
{
public:
	ComplexAIController(Map* map);
	~ComplexAIController() override;

	void Update() override;

public:
	Actor* m_targetActor = nullptr;
	std::vector<Actor*> m_subActors;

private:
	float m_baseSpringK = 180.f;
	float m_baseAlignK = 100.f;
	float m_dampLong = 22.5f;
	float m_dampLat = 12.f;
	float m_friction = 2.5f;

	float m_bodyWaveSpeed = 3.0f;
	float m_bodyWavePhase = 7.5f;
	float m_bodyWaveAmplitude = 0.5f;
	float maxForce = 2500.f;

	BossState m_bossState = BossState::INTRO;
	float m_stateTimer = 5.5f; 
	float m_lastHealth = -1.f;
	float m_damageAccumulated = 0.f;

	float m_circleAngle = 0.f;
	Vec3 m_circleCenterOffset;
	DivePhase m_divePhase = DivePhase::FLY_UP;
	Vec3 m_dashDirection = Vec3(0.f, 0.f, 1.f);

	IntroPhase m_introPhase = IntroPhase::WAIT;
	Vec3 m_spawnPosition;
	bool m_isInitialize = true;

	bool m_hasDoneStageTransition = false;
	StageTransitionPhase m_stageTransitionPhase = StageTransitionPhase::MOVE_TO_CENTER;
	float m_sphereTheta = 0.f;
	float m_spherePhi = 0.f;
	Vec3 m_mapCenterHigh = Vec3(82.5f, 82.5f, 30.f);
	float m_transitionSphereRadius = 45.f;
};