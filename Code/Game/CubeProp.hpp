#pragma once
#include "Game/Prop.hpp"

//-----------------------------------------------------------------------------------------------
class CubeProp : public Prop {
public:
	~CubeProp() override;
	CubeProp(
		Game* game, Vec3 startPos, float length, bool isFlash = false, 
		float yawSpeed = 0.f, float pitchSpeed = 0, float rollSpeed = 0
	);

	void          Update(float deltaSeconds) override;
	void          Render() const override;

	float m_length = 1.f;
};