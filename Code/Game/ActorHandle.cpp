#include "Game/ActorHandle.hpp"

//-----------------------------------------------------------------------------------------------
ActorHandle::ActorHandle()
	: m_data(0)
{
}

//-----------------------------------------------------------------------------------------------
ActorHandle::ActorHandle(unsigned int uid, unsigned int index) {
	m_data = (uid << 16) | (index & MAX_ACTOR_INDEX);
}

//-----------------------------------------------------------------------------------------------
bool ActorHandle::IsValid() const {
	return m_data != 0;
}

//-----------------------------------------------------------------------------------------------
unsigned int ActorHandle::GetIndex() const {
	return m_data & MAX_ACTOR_INDEX;
}

//-----------------------------------------------------------------------------------------------
unsigned int ActorHandle::GetUID() const {
	return m_data >> 16;
}

//-----------------------------------------------------------------------------------------------
bool ActorHandle::operator == (ActorHandle const& other) const {
	return m_data == other.m_data;
}

//-----------------------------------------------------------------------------------------------
bool ActorHandle::operator != (ActorHandle const& other) const {
	return m_data != other.m_data;
}