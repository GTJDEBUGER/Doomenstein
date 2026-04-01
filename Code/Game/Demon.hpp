#pragma once
#include "Game/Actor.hpp"
#include "Engine/Core/Vertex.hpp"
#include <vector>
//-----------------------------------------------------------------------------------------------
class Demon : public Actor {
public:
	Demon(Vec3 pos, EulerAngles orien, Vec3 scale);
	~Demon() override;

	void Update(float deltaSeconds) override;
	void Render() const override;

public:
	std::vector<Vertex> m_solidMesh;
	std::vector<Vertex> m_wireframeMesh;
};