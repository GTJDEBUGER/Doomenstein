#include "Game/Prop.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"

//-----------------------------------------------------------------------------------------------
Prop::~Prop() {
}

//-----------------------------------------------------------------------------------------------
Prop::Prop(Game* game, Vec3 startPos) : Entity(game, startPos) {

}

//-----------------------------------------------------------------------------------------------
void Prop::Update(float deltaSeconds) {
	m_position += m_velocity * deltaSeconds;
	m_orientation += m_angularVelocity * deltaSeconds;
}

//-----------------------------------------------------------------------------------------------
void Prop::Render() const {
	g_engine->m_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_renderer->BindTexture(m_texture);
	g_engine->m_renderer->DrawVertexArray(m_vertexs);
}