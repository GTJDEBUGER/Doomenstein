#pragma once

//-----------------------------------------------------------------------------------------------
class Map;
class Actor;
struct ActorHandle;

//-----------------------------------------------------------------------------------------------
class Controller {
public:
	Controller(Map* map);
	virtual ~Controller() = default;

	virtual void Update() = 0;
	void Possess(ActorHandle* actorHandle);
	void SwitchToNextPossessibleActor();
	Actor* GetPossessedActor();

public:
	ActorHandle* m_possessedActorHandle = nullptr;
	Map*         m_map                  = nullptr;
	int          m_handleIndex          = -1;
};