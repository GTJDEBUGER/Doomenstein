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

	void Possess(ActorHandle* actorHandle);
	void SwitchToNextPossessibleActor();
	Actor* GetPossessedActor();

public:
	ActorHandle* m_possessedActorHandle = nullptr;
	Map*         m_map                  = nullptr;
};