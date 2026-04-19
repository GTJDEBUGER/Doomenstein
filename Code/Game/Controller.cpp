#include "Game/Controller.hpp"
#include "Game/Map.hpp"
#include "Game/ActorHandle.hpp"
#include "Game/Actor.hpp"

//-----------------------------------------------------------------------------------------------
Controller::Controller(Map* map)
	: m_map(map) {
}

//-----------------------------------------------------------------------------------------------
void Controller::Possess(ActorHandle* actorHandle) {
	Actor* possessedActor = GetPossessedActor();
	if (possessedActor != nullptr) {
		possessedActor->OnUnpossessed();
	}

	m_possessedActorHandle = actorHandle;

	possessedActor = GetPossessedActor();
	if (possessedActor != nullptr) {
		possessedActor->m_controller = this;
		possessedActor->OnPossessed();
	}
}

//-----------------------------------------------------------------------------------------------
void Controller::SwitchToNextPossessibleActor() {
	if (m_possessedActorHandle == nullptr) {
		return;
	}
	
	Actor* currentActor = GetPossessedActor();
	Actor* nextActor = m_map->GetNextValidActorLoop(*m_possessedActorHandle);
	while (nextActor != currentActor) {
		if (nextActor->m_definition.m_canBePossessed) {
			Possess(nextActor->m_handle);
			return;
		}
		nextActor = m_map->GetNextValidActorLoop(*nextActor->m_handle);
	}
}

//-----------------------------------------------------------------------------------------------
Actor* Controller::GetPossessedActor() {
	if (m_possessedActorHandle != nullptr && m_possessedActorHandle->IsValid()) {
		return m_map->GetActorByHandle(*m_possessedActorHandle);
	}
	return nullptr;
}