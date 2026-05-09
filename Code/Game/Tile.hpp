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
	Tile(IntVec2 position, int minZ, int maxZ, float size, TileDefinition const& def);

	AABB3 GetWorldMesh() const;
	Vec3  GetCenterWorldPosition() const;

public:
	IntVec2         m_tileCoords = IntVec2(0, 0);
	int             m_minZ = 0;
	int             m_maxZ = 0;
	float           m_size = 0.f;
	TileDefinition const& m_definition;
};