#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Core/Rgba8.hpp"

//-----------------------------------------------------------------------------------------------
class Actor {
public:
	Actor(Vec3 pos, EulerAngles orien, Vec3 scale);
	virtual ~Actor() = 0;

	virtual void Update(float deltaSeconds) = 0;
	virtual void Render() const = 0;
	Mat44        GetModelMatrix();

public:
	Vec3        m_position = Vec3(0.f, 0.f, 0.f);
	EulerAngles m_orientation = EulerAngles(0.f, 0.f, 0.f);
	Vec3        m_scale = Vec3(1.f, 1.f, 1.f);
	Rgba8	    m_color = Rgba8::WHITE;
	float       m_physicsHeight = 1.f;
	float       m_physicsRadius = 0.5f;
	bool        m_isStatic = false;
	Vec3        m_velocity = Vec3(0.f, 0.f, 0.f);
};