#pragma once
#include "Game/Entity.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Renderer/Texture.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
class Prop : public Entity {
public:
	~Prop() override;
	Prop(Game* game, Vec3 startPos);

	void          Update(float deltaSeconds) override;
	void          Render() const override;

public:
	std::vector<Vertex> m_vertexs;
	Texture*            m_texture     = nullptr;
};