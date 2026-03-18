#pragma once
#include "Game/Prop.hpp"
#include "Engine/Math/AABB2.hpp"

//-----------------------------------------------------------------------------------------------
class ConeProp : public Prop {
public:
	~ConeProp() override;
	ConeProp(
		Game* game, Vec3 startPos, Vec3 endPos, float radius,
		Rgba8 color = Rgba8::WHITE, AABB2 const& UVs = AABB2::ZERO_TO_ONE, int numSlices = 32,
		float yawSpeed = 0.f, float pitchSpeed = 0, float rollSpeed = 0
	);

	void          Update(float deltaSeconds) override;
	void          Render() const override;

	Vec3  m_axis;
	float m_radius = 1.f;
	AABB2 m_UVs = AABB2::ZERO_TO_ONE;
	int   m_numSlices = 32;
};