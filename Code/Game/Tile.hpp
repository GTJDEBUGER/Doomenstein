#pragma once
#include "Game/TileDefinition.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Math/AABB3.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
class Tile {
public:
	Tile() = default;
	~Tile() = default;
	Tile(IntVec2 position, float size, TileDefinition* def);

	AABB3 GetWorldMesh() const;
	Vec3  GetCenterWorldPosition() const;

public:
	IntVec2         m_tileCoords = IntVec2(0, 0);
	float           m_size = 0.f;
	TileDefinition* m_definition = nullptr;
};