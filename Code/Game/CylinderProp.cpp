#include "Game/CylinderProp.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"

//-----------------------------------------------------------------------------------------------
CylinderProp::~CylinderProp() {
}

//-----------------------------------------------------------------------------------------------
CylinderProp::CylinderProp(Game* game, Vec3 startPos, Vec3 endPos, float radius,
	Rgba8 color, AABB2 const& UVs, int numSlices, float yawSpeed, float pitchSpeed, float rollSpeed
) : Prop(game, startPos) {
	m_axis = endPos - startPos;
	m_position = (startPos + endPos) * 0.5f;
	m_radius = radius;
	m_color = color;
	m_UVs = UVs;
	m_numSlices = numSlices;
	m_angularVelocity.m_yawDegrees = yawSpeed;
	m_angularVelocity.m_pitchDegrees = pitchSpeed;
	m_angularVelocity.m_rollDegrees = rollSpeed;

	AddVertexForCylinder3D(m_vertexs, startPos - m_position, endPos - m_position, m_radius, m_color, m_UVs, m_numSlices);
}

//-----------------------------------------------------------------------------------------------
void CylinderProp::Update(float deltaSeconds) {
	m_position += m_velocity * deltaSeconds;
	m_orientation += m_angularVelocity * deltaSeconds;
}

//-----------------------------------------------------------------------------------------------
void CylinderProp::Render() const {
	Entity::Render();
	g_engine->m_renderer->BindTexture(
		g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/TestUV.png")
	);
	g_engine->m_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_renderer->DrawVertexArray(m_vertexs);
}