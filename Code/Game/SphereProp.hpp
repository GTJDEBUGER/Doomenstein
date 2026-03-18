#pragma once
#pragma once
#include "Game/Prop.hpp"

//-----------------------------------------------------------------------------------------------
class SphereProp : public Prop {
public:
	~SphereProp() override;
	SphereProp(
		Game* game, Vec3 startPos, float radius, int numSlices = 32, int numStacks = 16, 
		float yawSpeed = 0.f, float pitchSpeed = 0, float rollSpeed = 0
	);

	void          Update(float deltaSeconds) override;
	void          Render() const override;

	float m_radius = 1.f;
	int m_numSlices = 32;
	int m_numStacks = 16;
};