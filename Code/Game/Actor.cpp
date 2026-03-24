#include "Game/Actor.hpp"

//---------------------------------------------------------------------------------------------------
Actor::Actor(Vec3 pos, EulerAngles orien, Vec3 scale) :
	m_position(pos),
	m_orientation(orien),
	m_scale(scale){
}

//---------------------------------------------------------------------------------------------------
Actor::~Actor() {
}

//---------------------------------------------------------------------------------------------------
Mat44 Actor::GetModelMatrix() {
	return Mat44::MakeTransform3D(m_position, m_orientation, m_scale);
}