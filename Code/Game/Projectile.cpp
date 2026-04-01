#include "Game/Projectile.hpp"
#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Game/Map.hpp"
#include "Game/MapDefinition.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"

//-----------------------------------------------------------------------------------------------
Projectile::Projectile(Vec3 pos, EulerAngles orien, Vec3 scale, bool physicsSimul) :
	Actor(pos, orien, scale),
	m_physicsSimul(physicsSimul){
	m_color = Rgba8(0, 0, 255, 255);
	m_physicsHeight = 0.625f * m_scale.z;
	m_physicsRadius = 0.3125f * m_scale.x;
	m_isStatic = false;
}

//-----------------------------------------------------------------------------------------------
Projectile::~Projectile() {
}

//-----------------------------------------------------------------------------------------------
void Projectile::Update(float deltaSeconds) {
	m_velocity += Vec3(0, 0, -9.8f) * deltaSeconds;
	m_position += m_velocity * deltaSeconds;
	if (m_physicsSimul) {
		return;
	}

	if (g_app->m_game->m_player->m_canMove) {
		return;
	}

	//Update projectile velocity and position
	Vec3 curFwd;
	Vec3 curLeft;
	Vec3 curUp;
	g_app->m_game->m_player->m_orientation.GetAsVectors_IFwd_JLeft_KUp(curFwd, curLeft, curUp);
	curFwd.z = 0.f;
	curFwd = curFwd.GetNormalized();
	curLeft.z = 0.f;
	curLeft = curLeft.GetNormalized();
	curUp = Vec3(0, 0, 1); //use world up
	if (g_app->m_game->m_player->m_isRun) {
		m_velocity = (-curFwd * g_app->m_game->m_player->m_moveInput.z - curLeft * g_app->m_game->m_player->m_moveInput.x + curUp * g_app->m_game->m_player->m_moveInput.y) * g_gameConfig->m_playerRunSpeed;
	}
	else {
		m_velocity = (-curFwd * g_app->m_game->m_player->m_moveInput.z - curLeft * g_app->m_game->m_player->m_moveInput.x + curUp * g_app->m_game->m_player->m_moveInput.y) * g_gameConfig->m_playerMoveSpeed;
	}
	m_position += m_velocity * deltaSeconds;
}

//-----------------------------------------------------------------------------------------------
void Projectile::Render() const {
	//Update projectile mesh
	std::vector<Vertex> m_solidMesh;
	std::vector<Vertex> m_wireframeMesh;

	AddVertexForCylinder3D(
		m_solidMesh,
		m_position,
		m_position + Vec3(0, 0, m_physicsHeight),
		m_physicsRadius ,
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

	g_engine->m_renderer->BindTexture(nullptr);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_engine->m_renderer->DrawVertexArray(m_solidMesh);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
	g_engine->m_renderer->DrawVertexArray(m_wireframeMesh);
	g_engine->m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}