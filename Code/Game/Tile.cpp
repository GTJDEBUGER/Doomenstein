#include "Game/Tile.hpp"

//-----------------------------------------------------------------------------------------------
Tile::Tile(IntVec2 position, int minZ, int maxZ, float size, TileDefinition const& def)
	: m_tileCoords(position)
	, m_minZ(minZ)
	, m_maxZ(maxZ)
	, m_size(size)
	, m_definition(def)
{
}

//-----------------------------------------------------------------------------------------------
AABB3 Tile::GetWorldMesh() const {
	Vec3 mins = Vec3(
		static_cast<float>(m_tileCoords.x) * m_size,
		static_cast<float>(m_tileCoords.y) * m_size,
		static_cast<float>(m_minZ) * m_size
	);
	Vec3 maxs = mins + Vec3(m_size, m_size, static_cast<float>(m_maxZ - m_minZ + 1) * m_size);
	return AABB3(mins, maxs);
}

//-----------------------------------------------------------------------------------------------
Vec3 Tile::GetCenterWorldPosition() const {
	AABB3 worldMesh = GetWorldMesh();
	return (worldMesh.m_mins + worldMesh.m_maxs) * 0.5f;
}