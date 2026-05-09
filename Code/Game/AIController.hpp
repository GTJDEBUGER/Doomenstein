#pragma once
#include "Game/Controller.hpp"

//-----------------------------------------------------------------------------------------------
class AIController : public Controller {
public:
	AIController(Map* map);
	~AIController() override;

	void DamagedBy(Actor* attacker);
	void Update() override;

public:
	Actor* m_targetActor = nullptr;
};