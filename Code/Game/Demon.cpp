#include "Game/Demon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"

//-----------------------------------------------------------------------------------------------
Demon::Demon(Vec3 pos, EulerAngles orien, Vec3 scale) :
	Actor(pos, orien, scale) {
	m_color = Rgba8(255, 0, 0, 255);
	m_physicsHeight = 3.75f;
	m_physicsRadius = 1.75f;
	m_isStatic = true;

	AddVertexForCylinder3D(
		m_solidMesh,
		m_position,
		m_position + Vec3(0, 0, m_physicsHeight),
		m_physicsRadius,
		m_color,
		AABB2::ZERO_TO_ONE,
		16
	);
	AddVertexForCylinder3D(
		m_wireframeMesh,
		m_position,
		m_position + Vec3(0, 0, m_physicsHeight),
		m_physicsRadius,
		Rgba8::WHITE,
		AABB2::ZERO_TO_ONE,
		16
	);
}

//-----------------------------------------------------------------------------------------------
Demon::~Demon() {
}

//-----------------------------------------------------------------------------------------------
void Demon::Update(float deltaSeconds) {
}

//-----------------------------------------------------------------------------------------------
void Demon::Render() const {
	g_engine->m_renderer->BindTexture(nullptr);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_engine->m_renderer->DrawVertexArray(m_solidMesh);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
	g_engine->m_renderer->DrawVertexArray(m_wireframeMesh);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}