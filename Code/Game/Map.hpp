#pragma once
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include <vector>
#include <string>

//-----------------------------------------------------------------------------------------------
class Game;
class MapDefinition;
class Tile;
class Actor;
class Texture;
class Shader;
class VertexBuffer;
class IndexBuffer;
class Camera;

//-----------------------------------------------------------------------------------------------
enum class ActorType
{
	DEMON,
	PROJECTILE,
	COUNT
};

//-----------------------------------------------------------------------------------------------
class Map {
public:
	Map(Game* game, MapDefinition const* definition);
	~Map();

	void CreateCameras();
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
	bool IsTileSolid(int x, int y) const;
	bool AreCoordsInBounds(int x, int y) const;
	Tile const* GetTile(int x, int y) const;
	Vec3 GetMapWorldCenter() const;
	Mat44 GetSunShadowCameraViewProjMatrix() const;

	void Update();
	void CollideActors();
	void CollideActors(Actor* actorA, Actor* actorB);
	void CollideActorsWithMap();
	IntVec2 GetTileCoordsForPosition(Vec3 const& position) const;
	void CollideActorWithMap(Actor* actor);
	void PushActorOutOfTileIfSolid(Actor* actor, int tileX, int tileY);
	void UpdateSunShadowCamera();

	void Render() const;
	void RenderShadowmap() const;
	RaycastResult3D RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;
	RaycastResult3D RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;

	ActorType StringToActorType(std::string const& str);
	void SpawnActors();
	void SpawnActor(ActorType type, Vec3 const& pos, EulerAngles const& ori=EulerAngles(), Vec3 const& scale=Vec3(1.f,1.f,1.f), bool isPhysicsSimul = false);

public:
	Game* m_game = nullptr;
	Vec3  m_sunDirection = Vec3(2.f, 1.f, -1.f);
	float m_sunIntensity = 0.85f;
	float m_ambientIntensity = 0.35f;
	Camera* m_sunShadowCamera = nullptr;

protected:
	MapDefinition const* m_definition = nullptr;
	std::vector<Tile> m_tiles;
	IntVec2 m_dimensions = IntVec2(0, 0);

	std::vector<Vertex_TBN> m_mapVerts;
	std::vector<unsigned int> m_mapIndexs;
	VertexBuffer* m_mapVertexBuffer = nullptr;
	IndexBuffer* m_mapIndexBuffer = nullptr;
	Shader* m_shadowMapShader = nullptr;

	std::vector<Actor*> m_actors;
};