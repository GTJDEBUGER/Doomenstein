#pragma once
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
class Game;
class MapDefinition;
class Tile;
class Actor;
class Texture;
class Shader;
class VertexBuffer;
class IndexBuffer;

//-----------------------------------------------------------------------------------------------
class Map {
public:
	Map(Game* game, MapDefinition const* definition);
	~Map();

	void CreateTiles();
	void CreateGeometry();
	void AddGeometryForFrontWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForBackWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForLeftWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForRightWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForFloor(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForCeiling(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForTop(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForBottom(AABB3 const& bounds, AABB2 const& UVs);
	void CreateBuffers();

	bool ShouldRenderFaceAgainstNeighbor(int x, int y) const;
	bool IsPositionInBounds(Vec3 const& position) const;
	bool AreCoordsInBounds(int x, int y) const;
	Tile const* GetTile(int x, int y) const;
	Vec3 GetMapWorldCenter() const;

	void Update();
	void CollideActors();
	void CollideActors(Actor* actorA, Actor* actorB);
	void CollideActorsWithMap();
	void CollideActorWithMap(Actor* actor);

	void Render() const;
	RaycastResult3D RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;
	RaycastResult3D RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;

public:
	Game* m_game = nullptr;
	Vec3  m_sunDirection = Vec3(-1.f, 1.f, -1.f);
	float m_sunIntensity = 0.85f;
	float m_ambientIntensity = 0.35f;

protected:
	MapDefinition const* m_definition = nullptr;
	std::vector<Tile> m_tiles;
	IntVec2 m_dimensions = IntVec2(0, 0);

	std::vector<Vertex_TBN> m_mapVertices;
	std::vector<unsigned int> m_mapIndices;
	VertexBuffer* m_mapVertexBuffer = nullptr;
	IndexBuffer* m_mapIndexBuffer = nullptr;
};