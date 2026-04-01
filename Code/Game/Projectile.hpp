#pragma once
#include "Game/Actor.hpp"
#include "Engine/Core/Vertex.hpp"
#include <vector>
//-----------------------------------------------------------------------------------------------
class Projectile : public Actor {
public:
	Projectile(Vec3 pos, EulerAngles orien, Vec3 scale, bool physicsSimul);
	~Projectile() override;

	void Update(float deltaSeconds) override;
	void Render() const override;

public:
	bool m_physicsSimul = true;
};