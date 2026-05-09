#pragma once
#include "Game/MapDefinition.hpp"
#include "Game/ActorHandle.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Renderer/Renderer.hpp"

#include <vector>
#include <string>

//-----------------------------------------------------------------------------------------------
class Game;
class Tile;
class Actor;
class Texture;
class Shader;
class VertexBuffer;
class IndexBuffer;
class Camera;
class PlayerController;
struct PointLight;

//-----------------------------------------------------------------------------------------------
class Map {
public:
	Map(Game* game, MapDefinition const& definition);
	~Map();

	void CreateCameras();
	void CreateTiles();
	void CreateGeometry();
	void AddGeometryForFrontWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForBackWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForLeftWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForRightWall(AABB3 const& bounds, AABB2 const& UVs);

	void AddGeometryForInsideFrontWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForInsideBackWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForInsideLeftWall(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForInsideRightWall(AABB3 const& bounds, AABB2 const& UVs);

	void AddGeometryForFloor(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForCeiling(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForTop(AABB3 const& bounds, AABB2 const& UVs);
	void AddGeometryForBottom(AABB3 const& bounds, AABB2 const& UVs);

	bool HasSolidBlock(int x, int y, int z) const;
	bool HasRoomBlock(int x, int y, int z) const;
	bool IsPositionInBounds(Vec3 const& position) const;
	bool IsTileSolid(int x, int y) const;
	bool AreCoordsInBounds(int x, int y) const;
	Tile const* GetTile(int x, int y) const;
	Vec3 GetMapWorldCenter() const;
	IntVec2 GetTileCoordsForPosition(Vec3 const& position) const;
	Mat44 GetSunShadowCameraViewProjMatrix() const;

	void Update();
	int GetSpatialHashIndex(int cx, int cy, int cz) const;
	void BuildSpatialGrid();
	void CollideActors();
	void CollideActors(Actor* actorA, Actor* actorB);
	void CollideActorsWithMap();
	bool CollideActorWithMap(Actor* actor);
	void ClearDeadActors();
	void UpdateSortedActors();

	bool PushActorOutOfTileIfSolid(Actor* actor, int tileX, int tileY);
	bool PushActorOutofFloor(Actor* actor, int tileX, int tileY);
	void UpdateSunShadowCamera();

	void Render(Camera const& viewCamera) const;
	void RenderShadowmap() const;

	RaycastResult3D RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;
	RaycastResult3D RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr, Actor** hitActorPointer = nullptr) const;
	bool SectorDetectWorldActors(Vec3 const& center, Vec3 const& forward, float radius, float angleDegrees, Actor* owner = nullptr, std::vector<Actor*>* out_actors = nullptr) const;

	void SpawnActors();
	void RespawnPlayers();
	Actor* SpawnActor(SpawnInfo const& spawnInfo);
	Actor* SpawnPlayerActor(SpawnInfo const& spawnInfo, PlayerController* controller);
	Actor* GetActorByHandle(ActorHandle const handle) const;
	Actor* GetRandomSpwanPoint() const;
	Actor* GetNextValidActorLoop(ActorHandle const curHandle) const;
	Actor* GetNearestActor(Actor* source, std::string const& faction) const;

	void UpdatePointLights();

	void QuickSortActorsByDepth(Actor** actors, int left, int right, Vec3 const& cameraPos) const;
public:
	Game* m_game = nullptr;
	Vec3  m_sunDirection = Vec3(1.0f, 2.0f, -1.f);
	float m_sunIntensity = 0.85f;
	float m_ambientIntensity = 0.35f;
	float m_timeOfDay = 8.f;
	float m_simulatTimeScale = 0.1f; // 1 game second equals to 1 simulated hour
	float m_simulatLatitude = 30.659462f;
	float m_simulatDeclination = 0.f;
	Camera* m_sunShadowCamera = nullptr;
	Shader* m_shadowMapShader = nullptr;
	ActorHandle* m_bossActorHandle;

protected:
	MapDefinition const& m_definition;
	std::vector<Tile> m_tiles;
	IntVec2 m_dimensions = IntVec2(0, 0);

	std::vector<Vertex_TBN> m_mapVerts;
	std::vector<unsigned int> m_mapIndexs;

	std::vector<Actor*> m_actors;
	mutable std::vector<Actor*> m_sortedActors;
	std::vector<ActorHandle*> m_respawnActorsHandle;
	unsigned int m_nextActorUID = 1;

	std::vector<PointLight> m_pointLights;

	std::vector<Actor*> m_spatialHash[4096];
	float m_spatialCellSize = 5.0f;
};