#include "Game/CubeProp.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Rgba8.hpp"

//-----------------------------------------------------------------------------------------------
CubeProp::~CubeProp(){
}

//-----------------------------------------------------------------------------------------------
CubeProp::CubeProp(
	Game* game, Vec3 startPos, float length, bool isFlash, 
	float yawSpeed, float pitchSpeed, float rollSpeed
) : Prop(game, startPos){
	//+X
	AddVertexForQuad3D(
		m_vertexs, 
		Vec3(length * 0.5f, -length * 0.5f, length * 0.5f),
		Vec3(length * 0.5f, -length * 0.5f, -length * 0.5f),
		Vec3(length * 0.5f, length * 0.5f, -length * 0.5f),
		Vec3(length * 0.5f, length * 0.5f, length * 0.5f),
		Rgba8::RED
	);
	//-X
	AddVertexForQuad3D(
		m_vertexs,
		Vec3(-length * 0.5f, -length * 0.5f, -length * 0.5f),
		Vec3(-length * 0.5f, -length * 0.5f, length * 0.5f),
		Vec3(-length * 0.5f, length * 0.5f, length * 0.5f),
		Vec3(-length * 0.5f, length * 0.5f, -length * 0.5f),
		Rgba8::CYAN
	);
	//+Y
	AddVertexForQuad3D(
		m_vertexs,
		Vec3(-length * 0.5f, length * 0.5f, length * 0.5f),
		Vec3(length * 0.5f, length * 0.5f, length * 0.5f),
		Vec3(length * 0.5f, length * 0.5f, -length * 0.5f),
		Vec3(-length * 0.5f, length * 0.5f, -length * 0.5f),
		Rgba8::GREEN
	);
	//-Y
	AddVertexForQuad3D(
		m_vertexs,
		Vec3(-length * 0.5f, -length * 0.5f, -length * 0.5f),
		Vec3(length * 0.5f, -length * 0.5f, -length * 0.5f),
		Vec3(length * 0.5f, -length * 0.5f, length * 0.5f),
		Vec3(-length * 0.5f, -length * 0.5f, length * 0.5f),
		Rgba8::MEGENTA
	);
	//+Z
	AddVertexForQuad3D(
		m_vertexs,
		Vec3(-length * 0.5f, -length * 0.5f, length * 0.5f),
		Vec3(length * 0.5f, -length * 0.5f, length * 0.5f),
		Vec3(length * 0.5f, length * 0.5f, length * 0.5f),
		Vec3(-length * 0.5f, length * 0.5f, length * 0.5f),
		Rgba8::BLUE
	);
	//-Z
	AddVertexForQuad3D(
		m_vertexs,
		Vec3(length * 0.5f, -length * 0.5f, -length * 0.5f),
		Vec3(-length * 0.5f, -length * 0.5f, -length * 0.5f),
		Vec3(-length * 0.5f, length * 0.5f,-length * 0.5f),
		Vec3(length * 0.5f, length * 0.5f, -length * 0.5f),
		Rgba8::YELLOW
	);

	m_length = length;
	m_isFlash = isFlash;
	m_angularVelocity.m_yawDegrees = yawSpeed;
	m_angularVelocity.m_pitchDegrees = pitchSpeed;
	m_angularVelocity.m_rollDegrees = rollSpeed;
}

//-----------------------------------------------------------------------------------------------
void CubeProp::Update(float deltaSeconds) {
	if (m_isFlash) {
		m_color = Interpolate(
			Rgba8::WHITE, 
			Rgba8::BLACK, 
			(SinDegrees((float)m_game->m_gameClock->GetTotalSeconds() * 180.f)+1.f)*0.5f
		);
	}

	m_position += m_velocity * deltaSeconds;
	m_orientation += m_angularVelocity * deltaSeconds;
}

//-----------------------------------------------------------------------------------------------
void CubeProp::Render() const {
	Entity::Render();

	Prop::Render();
}