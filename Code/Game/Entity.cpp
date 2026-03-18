#include "Game/Entity.hpp"
#include "GameCommon.hpp"
#include "Game/Game.hpp"

#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"

//-----------------------------------------------------------------------------------------------
Entity ::Entity(Game* game, Vec3 startPos)
	: m_game(game)
	, m_position(startPos)
	, m_velocity(Vec3(0.f,0.f, 0.f))
	, m_isGarbage(false){
}

//-----------------------------------------------------------------------------------------------
Entity::~Entity() {
}

//-----------------------------------------------------------------------------------------------
void Entity::Render() const {
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
		),
		m_color
	);
}
//-----------------------------------------------------------------------------------------------
Vec3 Entity::GetForwardVector() const {
	return m_orientation.GetForwardDir_IFwd_JLeft_KUp();
}