#include "Game/SphereProp.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"

//-----------------------------------------------------------------------------------------------
SphereProp::~SphereProp() {

}

//-----------------------------------------------------------------------------------------------
SphereProp::SphereProp(Game* game, Vec3 startPos, float radius, int numSlices, int numStacks,
	float yawSpeed, float pitchSpeed, float rollSpeed
) : Prop(game, startPos) {
	m_radius = radius;
	m_numSlices = numSlices;
	m_numStacks = numStacks;
	m_angularVelocity.m_yawDegrees = yawSpeed;
	m_angularVelocity.m_pitchDegrees = pitchSpeed;
	m_angularVelocity.m_rollDegrees = rollSpeed;

	AddVertexForSphere3D(m_vertexs, Vec3(0, 0, 0), radius, m_color, AABB2::ZERO_TO_ONE, m_numSlices, m_numStacks);
}

//-----------------------------------------------------------------------------------------------
void SphereProp::Update(float deltaSeconds) {
	m_position += m_velocity * deltaSeconds;
	m_orientation += m_angularVelocity * deltaSeconds;
}

//-----------------------------------------------------------------------------------------------
void SphereProp::Render() const {
	Entity::Render();
	g_engine->m_renderer->BindTexture(
		g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/TestUV.png")
	); 
	g_engine->m_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_renderer->DrawVertexArray(m_vertexs);
}