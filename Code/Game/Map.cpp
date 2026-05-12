#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/Tile.hpp"
#include "Game/Actor.hpp"
#include "Game/TempActor.hpp"
#include "Game/PlayerController.hpp"
#include "Game/ActorHandle.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

//-----------------------------------------------------------------------------------------------
Map::Map(Game* game, MapDefinition const& definition)
	: m_game(game)
	, m_definition(definition)
{
	m_shadowMapShader = g_engine->m_renderer->CreateShader("ShadowMap", VertexType::PCUTBN);
	CreateCameras();
	CreateTiles();
	CreateGeometry();
	SpawnActors();
}

//-----------------------------------------------------------------------------------------------
Map::~Map() {
	for (Actor* actor : m_actors) {
		if (actor != nullptr) {
			delete actor;
		}
	}
	for (TempActor* tempActor : m_tempActors) {
		if (tempActor != nullptr) {
			delete tempActor;
		}
	}
	delete m_sunShadowCamera;
	m_sunShadowCamera = nullptr;
}

//-----------------------------------------------------------------------------------------------
void Map::CreateCameras() {
	m_sunShadowCamera = new Camera(Vec2(-150.f, -150.f), Vec2(150.f, 150.f), 0.001f, 300.f);
	UpdateSunShadowCamera();
}

//-----------------------------------------------------------------------------------------------
void Map::CreateTiles() {
	m_dimensions = IntVec2(m_definition.m_mapImage.GetDimentions().x, m_definition.m_mapImage.GetDimentions().y);
	m_tiles.reserve(m_dimensions.x * m_dimensions.y);
	for (int y = 0; y < m_dimensions.y; y++) {
		for (int x = 0; x < m_dimensions.x; x++) {
			Rgba8 pixelColor = m_definition.m_mapImage.GetTexelColor(IntVec2(x, y));
			TileDefinition* tileDef = nullptr;
			for (auto& def : TileDefinition::s_definitions) {
				if (def.second.m_mapImagePixelColor.r == pixelColor.r &&
					def.second.m_mapImagePixelColor.g == pixelColor.g &&
					def.second.m_mapImagePixelColor.b == pixelColor.b) {
					tileDef = &def.second;
					break;
				}
			}
			if (tileDef == nullptr) {
				ERROR_AND_DIE(Stringf("No tile definition found for map image pixel color..."));
			}

			int a = pixelColor.a;
			if (a == 255) {
				a = 127;
			}
			int minZ = 0;
			int maxZ = 0;
			if (a < 127) {
				minZ = a - 127;
			}
			else if (a > 127) {
				maxZ = a - 127;
			}

			m_tiles.emplace_back(IntVec2(x, y), minZ, maxZ, m_definition.m_tileSize, *tileDef);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::CreateGeometry() {
	for (int y = 0; y < m_dimensions.y; ++y) {
		for (int x = 0; x < m_dimensions.x; ++x) {
			const Tile* tile = GetTile(x, y);
			if (!tile) continue;

			for (int z = tile->m_minZ; z <= tile->m_maxZ; ++z) {
				AABB3 bounds(
					Vec3(x * m_definition.m_tileSize, y * m_definition.m_tileSize, z * m_definition.m_tileSize),
					Vec3((x + 1) * m_definition.m_tileSize, (y + 1) * m_definition.m_tileSize, (z + 1) * m_definition.m_tileSize)
				);

				if (tile->m_definition.m_wallUV != AABB2::ZERO_TO_ONE) {
					if (!HasSolidBlock(x + 1, y, z)) AddGeometryForFrontWall(bounds, tile->m_definition.m_wallUV);
					if (!HasSolidBlock(x - 1, y, z)) AddGeometryForBackWall(bounds, tile->m_definition.m_wallUV);
					if (!HasSolidBlock(x, y + 1, z)) AddGeometryForLeftWall(bounds, tile->m_definition.m_wallUV);
					if (!HasSolidBlock(x, y - 1, z)) AddGeometryForRightWall(bounds, tile->m_definition.m_wallUV);
				}

				if (z == tile->m_maxZ && tile->m_definition.m_topUV != AABB2::ZERO_TO_ONE) {
					if (!HasSolidBlock(x, y, z + 1)) AddGeometryForTop(bounds, tile->m_definition.m_topUV);
				}

				if (z == tile->m_minZ && tile->m_definition.m_bottomUV != AABB2::ZERO_TO_ONE) {
					if (!HasSolidBlock(x, y, z - 1)) AddGeometryForBottom(bounds, tile->m_definition.m_bottomUV);
				}

				if (tile->m_definition.m_insideWallUV != AABB2::ZERO_TO_ONE) {
					if (!HasRoomBlock(x + 1, y, z)) AddGeometryForInsideFrontWall(bounds, tile->m_definition.m_insideWallUV);
					if (!HasRoomBlock(x - 1, y, z)) AddGeometryForInsideBackWall(bounds, tile->m_definition.m_insideWallUV);
					if (!HasRoomBlock(x, y + 1, z)) AddGeometryForInsideLeftWall(bounds, tile->m_definition.m_insideWallUV);
					if (!HasRoomBlock(x, y - 1, z)) AddGeometryForInsideRightWall(bounds, tile->m_definition.m_insideWallUV);
				}

				if (z == tile->m_minZ && tile->m_definition.m_floorUV != AABB2::ZERO_TO_ONE) {
					if (!HasRoomBlock(x, y, z - 1)) AddGeometryForFloor(bounds, tile->m_definition.m_floorUV);
				}

				if (z == tile->m_maxZ && tile->m_definition.m_ceilingUV != AABB2::ZERO_TO_ONE) {
					if (!HasRoomBlock(x, y, z + 1)) AddGeometryForCeiling(bounds, tile->m_definition.m_ceilingUV);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForFrontWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForBackWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForLeftWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForRightWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForFloor(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForCeiling(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForTop(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForBottom(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z),
		Rgba8::WHITE,
		UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForInsideFrontWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Rgba8::WHITE, UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForInsideBackWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z),
		Rgba8::WHITE, UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForInsideLeftWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z),
		Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z),
		Rgba8::WHITE, UVs
	);
}

//-----------------------------------------------------------------------------------------------
void Map::AddGeometryForInsideRightWall(AABB3 const& bounds, AABB2 const& UVs) {
	AddVertexForQuad3D(m_mapVerts, m_mapIndexs,
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z),
		Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z),
		Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z),
		Rgba8::WHITE, UVs
	);
}

//-----------------------------------------------------------------------------------------------
bool Map::HasSolidBlock(int x, int y, int z) const {
	Tile const* tile = GetTile(x, y);
	if (!tile || tile->m_definition.m_wallUV == AABB2::ZERO_TO_ONE) return false;
	return (z >= tile->m_minZ && z <= tile->m_maxZ);
}

//-----------------------------------------------------------------------------------------------
bool Map::HasRoomBlock(int x, int y, int z) const {
	Tile const* tile = GetTile(x, y);
	if (!tile || tile->m_definition.m_isSolid) return false;
	return (z >= tile->m_minZ && z <= tile->m_maxZ);
}

//-----------------------------------------------------------------------------------------------
bool Map::IsTileSolid(int x, int y) const {
	Tile const* tile = GetTile(x, y);
	return tile != nullptr && tile->m_definition.m_isSolid;
}

//-----------------------------------------------------------------------------------------------
bool Map::IsPositionInBounds(Vec3 const& position) const {
	return position.x >= 0.f && position.x < static_cast<float>(m_dimensions.x) * m_definition.m_tileSize 
		&& position.y >= 0.f && position.y < static_cast<float>(m_dimensions.y) * m_definition.m_tileSize
		&& position.z >= 0.f && position.z < m_definition.m_tileSize;
}

//-----------------------------------------------------------------------------------------------
bool Map::AreCoordsInBounds(int x, int y) const {
	return x >= 0 && x < m_dimensions.x && y >= 0 && y < m_dimensions.y;
}

//-----------------------------------------------------------------------------------------------
Tile const* Map::GetTile(int x, int y) const {
	if (!AreCoordsInBounds(x, y)) {
		return nullptr;
	}
	return &m_tiles[y * m_dimensions.x + x];
}

//-----------------------------------------------------------------------------------------------
Vec3 Map::GetMapWorldCenter() const {
	return Vec3(
		static_cast<float>(m_dimensions.x) * m_definition.m_tileSize * 0.5f,
		static_cast<float>(m_dimensions.y) * m_definition.m_tileSize * 0.5f,
		m_definition.m_tileSize * 0.5f
	);
}

//-----------------------------------------------------------------------------------------------
Mat44 Map::GetSunShadowCameraViewProjMatrix() const {
	Mat44 viewProj = m_sunShadowCamera->GetRendererToClipTransform();
	viewProj.Append(m_sunShadowCamera->GetCameraToRendererTransform());
	viewProj.Append(m_sunShadowCamera->GetWorldToCameraTransform());
	return viewProj;
}

//-----------------------------------------------------------------------------------------------
void Map::Update() {
	float deltaTime = (float)m_game->m_gameClock->GetDeltaSeconds();

	for (Actor* actor : m_actors) {
		if (actor != nullptr) {
			actor->Update(deltaTime);
		}
	}

	for (TempActor* tempActor : m_tempActors) {
		if (tempActor != nullptr) {
			tempActor->Update(deltaTime);
		}
	}
	m_timeOfDay += deltaTime * m_simulatTimeScale;

	if (m_timeOfDay >= 24.0f) {
		m_timeOfDay -= 24.0f;
	}

	float hourAngle = (m_timeOfDay - 12.0f) * 15.0f;

	float sinLat = SinDegrees(m_simulatLatitude);
	float cosLat = CosDegrees(m_simulatLatitude);
	float sinDec = SinDegrees(m_simulatDeclination);
	float cosDec = CosDegrees(m_simulatDeclination);
	float sinHour = SinDegrees(hourAngle);
	float cosHour = CosDegrees(hourAngle);

	float sunX = -cosDec * sinHour;
	float sunY = sinLat * cosDec * cosHour - cosLat * sinDec;
	float sunZ = cosLat * cosDec * cosHour + sinLat * sinDec;

	m_sunDirection = -Vec3(sunX, sunY, sunZ).GetNormalized();

	if (m_game->m_isDrawDebug) {
		DebugAddWorldArrow(
			GetMapWorldCenter() + Vec3(0.f, 0.f, 20.f),
			GetMapWorldCenter() + Vec3(0.f, 0.f, 20.f) + m_sunDirection * 7.5f,
			0.5f,
			0.f,
			Rgba8::YELLOW,
			Rgba8::YELLOW
		);
	}

	UpdateSunShadowCamera();

	CollideActorsWithMap();
	CollideActors();
	CollideActorsWithMap();

	ClearDeadActors();
	RespawnPlayers();
	UpdateSortedActors();
	UpdatePointLights();
}

//-----------------------------------------------------------------------------------------------
int Map::GetSpatialHashIndex(int cx, int cy, int cz) const {
	unsigned int h = (cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791);
	return h % 4096;
}

//-----------------------------------------------------------------------------------------------
void Map::BuildSpatialGrid() {
	for (int i = 0; i < 4096; ++i) {
		m_spatialHash[i].clear();
	}

	for (Actor* actor : m_actors) {
		if (actor == nullptr || actor->m_isDead || !actor->m_definition.m_collision.m_collisionWithActors) {
			continue;
		}

		int cx = (int)floorf(actor->m_position.x / m_spatialCellSize);
		int cy = (int)floorf(actor->m_position.y / m_spatialCellSize);

		float centerZ = actor->m_position.z + (actor->m_definition.m_collision.m_height * 0.5f);
		int cz = (int)floorf(centerZ / m_spatialCellSize);

		int hashIdx = GetSpatialHashIndex(cx, cy, cz);
		m_spatialHash[hashIdx].push_back(actor);
	}
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors() {
	BuildSpatialGrid();

	for (Actor* actorA : m_actors) {
		if (actorA == nullptr || actorA->m_isDead || !actorA->m_definition.m_collision.m_collisionWithActors) {
			continue;
		}

		int cx = (int)floorf(actorA->m_position.x / m_spatialCellSize);
		int cy = (int)floorf(actorA->m_position.y / m_spatialCellSize);
		float centerZ = actorA->m_position.z + (actorA->m_definition.m_collision.m_height * 0.5f);
		int cz = (int)floorf(centerZ / m_spatialCellSize);

		for (int dz = -1; dz <= 1; ++dz) {
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					int hashIdx = GetSpatialHashIndex(cx + dx, cy + dy, cz + dz);

					for (Actor* actorB : m_spatialHash[hashIdx]) {
						if (actorA >= actorB) {
							continue;
						}

						if (actorA->m_owner == actorB ||
							actorB->m_owner == actorA ||
							(actorA->m_owner != nullptr && actorA->m_owner == actorB->m_owner)) {
							continue;
						}

						CollideActors(actorA, actorB);
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors(Actor* actorA, Actor* actorB) {
	bool isZPush = false;
	if (!actorA->m_definition.m_physics.m_isSimulated && !actorB->m_definition.m_physics.m_isSimulated) {
		return;
	}
	else if (actorA->m_definition.m_physics.m_isSimulated && !actorB->m_definition.m_physics.m_isSimulated) {
		if (PushZCylinderOutOfFixedZCylinder3D(
			actorA->m_position,
			actorA->m_definition.m_collision.m_radius,
			FloatRange(actorA->m_position.z, actorA->m_position.z + actorA->m_definition.m_collision.m_height),
			actorB->m_position,
			actorB->m_definition.m_collision.m_radius,
			FloatRange(actorB->m_position.z, actorB->m_position.z + actorB->m_definition.m_collision.m_height),
			&isZPush
		)) {
			actorA->OnCollide(actorB);
		}
		if (isZPush) {
			actorA->m_velocity.z = 0.f;
		}
	}
	else if (!actorA->m_definition.m_physics.m_isSimulated && actorB->m_definition.m_physics.m_isSimulated) {
		if (PushZCylinderOutOfFixedZCylinder3D(
			actorB->m_position,
			actorB->m_definition.m_collision.m_radius,
			FloatRange(actorB->m_position.z, actorB->m_position.z + actorB->m_definition.m_collision.m_height),
			actorA->m_position,
			actorA->m_definition.m_collision.m_radius,
			FloatRange(actorA->m_position.z, actorA->m_position.z + actorA->m_definition.m_collision.m_height),
			&isZPush
		)) {
			actorB->OnCollide(actorA);
		}
		if (isZPush) {
			actorB->m_velocity.z = 0.f;
		}
	}
	else {
		if (PushZCylindersOutOfEachOther3D(
			actorA->m_position,
			actorA->m_definition.m_collision.m_radius,
			FloatRange(actorA->m_position.z, actorA->m_position.z + actorA->m_definition.m_collision.m_height),
			actorB->m_position,
			actorB->m_definition.m_collision.m_radius,
			FloatRange(actorB->m_position.z, actorB->m_position.z + actorB->m_definition.m_collision.m_height),
			&isZPush
		)) {
			actorA->OnCollide(actorB);
			actorB->OnCollide(actorA);
		}
		if (isZPush) {
			actorA->m_velocity.z = 0.f;
			actorB->m_velocity.z = 0.f;
		}
	}
}

IntVec2 Map::GetTileCoordsForPosition(Vec3 const& position) const {
	return IntVec2(
		(int)floorf(position.x / m_definition.m_tileSize),
		(int)floorf(position.y / m_definition.m_tileSize)
	);
}

//-----------------------------------------------------------------------------------------------	
void Map::CollideActorsWithMap() {
	for (Actor* actor : m_actors) {
		if (
			actor==nullptr || 
			!actor->m_definition.m_physics.m_isSimulated ||
			!actor->m_definition.m_collision.m_collisionWithWorld
		) {
			continue;
		}
		
		if (CollideActorWithMap(actor)) {
			actor->OnCollide(nullptr);
		}
	}
}

//-----------------------------------------------------------------------------------------------
bool Map::CollideActorWithMap(Actor* actor) {
	if (!actor) return false;

	IntVec2 tileCoords = GetTileCoordsForPosition(actor->m_position);
	int tx = tileCoords.x;
	int ty = tileCoords.y;

	bool isPushed = false;

	int checkRadius = (int)ceilf(actor->m_definition.m_collision.m_radius / m_definition.m_tileSize) + 1;

	for (int dy = -checkRadius; dy <= checkRadius; ++dy) {
		for (int dx = -checkRadius; dx <= checkRadius; ++dx) {
			isPushed |= PushActorOutOfTileIfSolid(actor, tx + dx, ty + dy);
		}
	}

	isPushed |= PushActorOutofFloor(actor, tx, ty);

	return isPushed;
}

//-----------------------------------------------------------------------------------------------
void Map::ClearDeadActors() {
	for (size_t i = 0; i < m_actors.size(); i++) {
		if (m_actors[i]!=nullptr && m_actors[i]->m_needDestroy) {
			delete m_actors[i];
			m_actors[i] = nullptr;
		}
	}

	for (size_t i = 0; i < m_tempActors.size(); i++) {
		if (m_tempActors[i] != nullptr && m_tempActors[i]->m_isDead) {
			delete m_tempActors[i];
			m_tempActors[i] = nullptr;
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::UpdateSortedActors() {
	m_sortedActors.clear();
	for (Actor* actor : m_actors) {
		if (actor != nullptr) {
			m_sortedActors.push_back(actor);
		}
	}
}

//-----------------------------------------------------------------------------------------------
bool Map::PushActorOutOfTileIfSolid(Actor* actor, int tileX, int tileY) {
	bool isPushed = false;
	const Tile* tile = GetTile(tileX, tileY);
	if (!tile || !tile->m_definition.m_isSolid) {
		return isPushed;
	}

	FloatRange currentZRange(actor->m_position.z, actor->m_position.z + actor->m_definition.m_collision.m_height);
	bool isZPush = false;
	isPushed = PushZCylinderOutOfFixedAABB3D(actor->m_position, actor->m_definition.m_collision.m_radius, currentZRange, tile->GetWorldMesh(), &isZPush);
	if (isZPush) {
		actor->m_velocity.z = 0.f;
		actor->m_isGrounded = true;
	}

	return isPushed;
}

//-----------------------------------------------------------------------------------------------
bool Map::PushActorOutofFloor(Actor* actor, int tileX, int tileY) {
	bool isPushed = false;
	const Tile* tile = GetTile(tileX, tileY);
	if (!tile || tile->m_definition.m_floorUV == AABB2::ZERO_TO_ONE) {
		return isPushed;
	}

	float floorZ = static_cast<float>(tile->m_minZ) * m_definition.m_tileSize;
	if (actor->m_position.z < floorZ) {
		actor->m_position.z = floorZ;
		actor->m_velocity.z = 0.f;
		actor->m_isGrounded = true;

		isPushed = true;
	}

	return isPushed;
}

//-----------------------------------------------------------------------------------------------
void Map::UpdateSunShadowCamera() {
	float shadowUpdateInterval = m_simulatTimeScale;
	float quantizedTimeOfDay = floorf(m_timeOfDay / shadowUpdateInterval) * shadowUpdateInterval;

	float hourAngle = (quantizedTimeOfDay - 12.0f) * 15.0f;
	float sinLat = SinDegrees(m_simulatLatitude);
	float cosLat = CosDegrees(m_simulatLatitude);
	float sinDec = SinDegrees(m_simulatDeclination);
	float cosDec = CosDegrees(m_simulatDeclination);
	float sinHour = SinDegrees(hourAngle);
	float cosHour = CosDegrees(hourAngle);

	float sunX = -cosDec * sinHour;
	float sunY = sinLat * cosDec * cosHour - cosLat * sinDec;
	float sunZ = cosLat * cosDec * cosHour + sinLat * sinDec;

	Vec3 shadowSunDir = -Vec3(sunX, sunY, sunZ).GetNormalized();

	float mapSizeX = static_cast<float>(m_dimensions.x) * m_definition.m_tileSize;
	float mapSizeY = static_cast<float>(m_dimensions.y) * m_definition.m_tileSize;

	float mapRadius = sqrtf((mapSizeX * 0.5f) * (mapSizeX * 0.5f) + (mapSizeY * 0.5f) * (mapSizeY * 0.5f));

	float shadowRadius = mapRadius * 0.9f;

	Vec3 idealSunPos = GetMapWorldCenter() - (shadowSunDir * shadowRadius);
	EulerAngles sunOrientation = EulerAngles::MakeLookDirectionEulerAngles(shadowSunDir);

	float orthoWidth = shadowRadius * 2.0f;
	float shadowMapRes = 2048.f;
	float unitsPerTexel = orthoWidth / shadowMapRes;

	Vec3 lightLeft;
	Vec3 lightUp;
	Vec3 lightFwd;
	sunOrientation.GetAsVectors_IFwd_JLeft_KUp(lightFwd, lightLeft, lightUp);

	float xInLightSpace = DotProduct3D(idealSunPos, -lightLeft);
	float yInLightSpace = DotProduct3D(idealSunPos, lightUp);
	float zInLightSpace = DotProduct3D(idealSunPos, lightFwd);

	xInLightSpace = floorf(xInLightSpace / unitsPerTexel) * unitsPerTexel;
	yInLightSpace = floorf(yInLightSpace / unitsPerTexel) * unitsPerTexel;

	Vec3 snappedSunPos = (-lightLeft * xInLightSpace) +
		(lightUp * yInLightSpace) +
		(lightFwd * zInLightSpace);

	m_sunShadowCamera->SetPosition(snappedSunPos);
	m_sunShadowCamera->SetOrientation(sunOrientation);

	m_sunShadowCamera->SetOrthoView(
		Vec2(-shadowRadius, -shadowRadius),
		Vec2(shadowRadius, shadowRadius),
		0.0f, 
		shadowRadius * 2.0f 
	);
}

//-----------------------------------------------------------------------------------------------
void Map::Render(Camera const& viewCamera) const {
	g_engine->m_renderer->SetLightConstants(
		m_sunDirection.GetNormalized(),
		m_sunIntensity,
		m_ambientIntensity,
		GetSunShadowCameraViewProjMatrix(),
		(unsigned int)m_pointLights.size(),
		m_pointLights.data()
	);
	g_engine->m_renderer->SetModelConstants(
		Mat44::MakeTransform3D(
			Vec3(),
			EulerAngles(),
			Vec3(1.f, 1.f, 1.f)
		)
	);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP, SamplerSlot::SLOT0);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::ANISOTROPIC_WARP, SamplerSlot::SLOT1);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::SHADOWMAP, SamplerSlot::SLOT2);

	g_engine->m_renderer->BindTexture(m_definition.m_mapTexture, TextureSlot::DIFFUSE_SCREEN);
	g_engine->m_renderer->BindTexture(m_definition.m_mapNormalTexture, TextureSlot::NORMAL_ORIGINALSCREEN);
	g_engine->m_renderer->BindTexture(m_definition.m_mapAOTexture, TextureSlot::AO_SCREENDEPTH);
	g_engine->m_renderer->BindTexture(m_definition.m_mapParallaxTexture, TextureSlot::PARALLAX_SCREENNORMAL);
	g_engine->m_renderer->BindTexture(m_definition.m_mapRoughnessTexture, TextureSlot::ROUGHNESS_SCREENDEPTHSTENCIL);
	g_engine->m_renderer->BindTexture(m_definition.m_mapMetallicTexture, TextureSlot::METALLIC);
	g_engine->m_renderer->BindTexture(nullptr, TextureSlot::EMISSIVE);
	g_engine->m_renderer->BindTexture(nullptr, TextureSlot::SHADOWMAP);
	g_engine->m_renderer->DrawVertexArray(m_mapVerts, m_mapIndexs, m_definition.m_mapShader);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

	Vec3 cameraPos = viewCamera.GetPosition();

	int actorCount = (int)m_actors.size();
	if (actorCount <= 0) return;

	if (m_sortedActors.size() > 1) {
		QuickSortActorsByDepth(m_sortedActors.data(), 0, (int)m_sortedActors.size() - 1, cameraPos);
	}

	for (int i = 0; i < m_sortedActors.size(); ++i) {
		m_sortedActors[i]->Render(viewCamera);
	}

	for (TempActor* tempActor : m_tempActors) {
		if (tempActor != nullptr) {
			tempActor->Render(viewCamera);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::RenderShadowmap() const {
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_engine->m_renderer->DrawVertexArray(m_mapVerts, m_mapIndexs, m_shadowMapShader, true, false);
	for (Actor* actor : m_actors) {
		if (actor != nullptr) {
			actor->RenderShadowmap();
		}
	}
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner) const {
	RaycastResult3D nearestResult;
	nearestResult.m_didImpact = false;
	nearestResult.m_impactDist = distance;
	nearestResult.m_rayStartPos = start;
	nearestResult.m_rayFwdNormal = direction;
	nearestResult.m_rayMaxLength = distance;

	RaycastResult3D worldXYResult = RaycastWorldXY(start, direction, nearestResult.m_impactDist);
	if (worldXYResult.m_didImpact && worldXYResult.m_impactDist < nearestResult.m_impactDist) {
		nearestResult = worldXYResult;
	}

	RaycastResult3D worldZResult = RaycastWorldZ(start, direction, nearestResult.m_impactDist);
	if (worldZResult.m_didImpact && worldZResult.m_impactDist < nearestResult.m_impactDist) {
		nearestResult = worldZResult;
	}

	Actor* dummyHitActor = nullptr;
	RaycastResult3D worldActorsResult = RaycastWorldActors(start, direction, nearestResult.m_impactDist, owner, &dummyHitActor);
	if (worldActorsResult.m_didImpact && worldActorsResult.m_impactDist < nearestResult.m_impactDist) {
		nearestResult = worldActorsResult;
	}

	return nearestResult;
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const {
	RaycastResult3D result;
	result.m_rayStartPos = start;
	result.m_rayFwdNormal = direction;
	result.m_rayMaxLength = distance;

	IntVec2 curTile = GetTileCoordsForPosition(start);

	float invTileSize = 1.0f / m_definition.m_tileSize;

	auto IsZInRangeScaled = [](float checkZScaled, const Tile* tilePtr) -> bool {
		return (checkZScaled >= static_cast<float>(tilePtr->m_minZ) &&
			checkZScaled <= static_cast<float>(tilePtr->m_maxZ + 1));
		};

	const Tile* currentTilePtr = GetTile(curTile.x, curTile.y);
	float startZScaled = start.z * invTileSize;

	if (currentTilePtr && currentTilePtr->m_definition.m_isSolid && IsZInRangeScaled(startZScaled, currentTilePtr)) {
		result.m_didImpact = true;
		result.m_impactDist = 0.f;
		result.m_impactPos = start;
		result.m_impactNormal = -direction;
		return result;
	}

	int stepX = (direction.x > 0) ? 1 : (direction.x < 0 ? -1 : 0);
	int stepY = (direction.y > 0) ? 1 : (direction.y < 0 ? -1 : 0);

	float tDeltaX = (direction.x != 0.f) ? abs(m_definition.m_tileSize / direction.x) : FLT_MAX;
	float tDeltaY = (direction.y != 0.f) ? abs(m_definition.m_tileSize / direction.y) : FLT_MAX;

	float nextBoundaryX = (curTile.x + (stepX > 0 ? 1.f : 0.f)) * m_definition.m_tileSize;
	float nextBoundaryY = (curTile.y + (stepY > 0 ? 1.f : 0.f)) * m_definition.m_tileSize;

	float tMaxX = (direction.x != 0.f) ? (nextBoundaryX - start.x) / direction.x : FLT_MAX;
	float tMaxY = (direction.y != 0.f) ? (nextBoundaryY - start.y) / direction.y : FLT_MAX;

	while (true) {
		if (tMaxX < tMaxY) {
			if (tMaxX > distance) break;

			int nextTileX = curTile.x + stepX;
			const Tile* nextTilePtr = GetTile(nextTileX, curTile.y);
			float hitZScaled = (start.z + direction.z * tMaxX) * invTileSize;

			bool hitInside = currentTilePtr && (currentTilePtr->m_definition.m_insideWallUV != AABB2::ZERO_TO_ONE) && IsZInRangeScaled(hitZScaled, currentTilePtr);
			bool hitOutside = !hitInside && nextTilePtr && (nextTilePtr->m_definition.m_wallUV != AABB2::ZERO_TO_ONE) && IsZInRangeScaled(hitZScaled, nextTilePtr);

			if (hitInside || hitOutside) {
				result.m_didImpact = true;
				result.m_impactDist = tMaxX;
				result.m_impactPos = start + direction * tMaxX;
				result.m_impactNormal = (stepX > 0) ? Vec3(-1.f, 0.f, 0.f) : Vec3(1.f, 0.f, 0.f);
				return result;
			}

			tMaxX += tDeltaX;
			curTile.x = nextTileX;
			currentTilePtr = nextTilePtr;
		}
		else {
			if (tMaxY > distance) break;

			int nextTileY = curTile.y + stepY;
			const Tile* nextTilePtr = GetTile(curTile.x, nextTileY);
			float hitZScaled = (start.z + direction.z * tMaxY) * invTileSize;

			bool hitInside = currentTilePtr && (currentTilePtr->m_definition.m_insideWallUV != AABB2::ZERO_TO_ONE) && IsZInRangeScaled(hitZScaled, currentTilePtr);
			bool hitOutside = !hitInside && nextTilePtr && (nextTilePtr->m_definition.m_wallUV != AABB2::ZERO_TO_ONE) && IsZInRangeScaled(hitZScaled, nextTilePtr);

			if (hitInside || hitOutside) {
				result.m_didImpact = true;
				result.m_impactDist = tMaxY;
				result.m_impactPos = start + direction * tMaxY;
				result.m_impactNormal = (stepY > 0) ? Vec3(0.f, -1.f, 0.f) : Vec3(0.f, 1.f, 0.f);
				return result;
			}

			tMaxY += tDeltaY;
			curTile.y = nextTileY;
			currentTilePtr = nextTilePtr;
		}
	}
	return result;
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const {
	RaycastResult3D result;
	result.m_rayStartPos = start;
	result.m_rayFwdNormal = direction;
	result.m_rayMaxLength = distance;

	if (abs(direction.z) < 0.00001f) return result;

	IntVec2 curTile = GetTileCoordsForPosition(start);

	int stepX = (direction.x > 0) ? 1 : (direction.x < 0 ? -1 : 0);
	int stepY = (direction.y > 0) ? 1 : (direction.y < 0 ? -1 : 0);

	float tDeltaX = (direction.x != 0.f) ? abs(m_definition.m_tileSize / direction.x) : FLT_MAX;
	float tDeltaY = (direction.y != 0.f) ? abs(m_definition.m_tileSize / direction.y) : FLT_MAX;

	float nextBoundaryX = (curTile.x + (stepX > 0 ? 1.f : 0.f)) * m_definition.m_tileSize;
	float nextBoundaryY = (curTile.y + (stepY > 0 ? 1.f : 0.f)) * m_definition.m_tileSize;

	float tExitX = (direction.x != 0.f) ? (nextBoundaryX - start.x) / direction.x : FLT_MAX;
	float tExitY = (direction.y != 0.f) ? (nextBoundaryY - start.y) / direction.y : FLT_MAX;

	float tEnter = 0.f;
	const float EPSILON = 0.001f;

	float invDirZ = 1.0f / direction.z;

	while (tEnter <= distance) {
		float tExit = tExitX < tExitY ? tExitX : tExitY;

		if (curTile.x >= 0 && curTile.x < m_dimensions.x && curTile.y >= 0 && curTile.y < m_dimensions.y) {
			const Tile* tile = &m_tiles[curTile.y * m_dimensions.x + curTile.x];

			float topZ = static_cast<float>(tile->m_maxZ + 1) * m_definition.m_tileSize;
			float bottomZ = static_cast<float>(tile->m_minZ) * m_definition.m_tileSize;

			float tHit = FLT_MAX;
			Vec3 normalHit;
			bool hit = false;
			float tempT;

			if (direction.z < 0.f) {
				if (tile->m_definition.m_topUV != AABB2::ZERO_TO_ONE) {
					tempT = (topZ - start.z) * invDirZ;
					if (tempT >= tEnter - EPSILON && tempT <= tExit + EPSILON && tempT <= distance && tempT >= 0.f) {
						hit = true; tHit = tempT; normalHit = Vec3(0.f, 0.f, 1.f);
					}
				}
				if (!hit && tile->m_definition.m_floorUV != AABB2::ZERO_TO_ONE) {
					tempT = (bottomZ - start.z) * invDirZ;
					if (tempT >= tEnter - EPSILON && tempT <= tExit + EPSILON && tempT <= distance && tempT >= 0.f) {
						hit = true; tHit = tempT; normalHit = Vec3(0.f, 0.f, 1.f);
					}
				}
			}
			else if (direction.z > 0.f) {
				if (tile->m_definition.m_bottomUV != AABB2::ZERO_TO_ONE) {
					tempT = (bottomZ - start.z) * invDirZ;
					if (tempT >= tEnter - EPSILON && tempT <= tExit + EPSILON && tempT <= distance && tempT >= 0.f) {
						hit = true; tHit = tempT; normalHit = Vec3(0.f, 0.f, -1.f);
					}
				}
				if (!hit && tile->m_definition.m_ceilingUV != AABB2::ZERO_TO_ONE) {
					tempT = (topZ - start.z) * invDirZ;
					if (tempT >= tEnter - EPSILON && tempT <= tExit + EPSILON && tempT <= distance && tempT >= 0.f) {
						hit = true; tHit = tempT; normalHit = Vec3(0.f, 0.f, -1.f);
					}
				}
			}

			if (hit) {
				result.m_didImpact = true;
				result.m_impactDist = tHit;
				result.m_impactPos = start + direction * tHit;
				result.m_impactNormal = normalHit;
				return result;
			}
		}

		if (tExitX < tExitY) {
			tEnter = tExitX;
			curTile.x += stepX;
			tExitX += tDeltaX;
		}
		else {
			tEnter = tExitY;
			curTile.y += stepY;
			tExitY += tDeltaY;
		}
	}
	return result;
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner, Actor** hitActorPointer) const {
	float nearestImpactDist = distance;
	RaycastResult3D nearestResult;
	nearestResult.m_rayStartPos = start;
	nearestResult.m_rayFwdNormal = direction;
	nearestResult.m_rayMaxLength = distance;

	for (Actor* actor : m_actors) {
		if (actor==nullptr || actor == owner || actor->m_definition.m_name=="SpawnPoint" || actor->m_isDead) {
			continue;
		}
		RaycastResult3D result = RaycastVsCylinderZ3D(
			start, 
			direction, 
			distance, 
			actor->m_position.GetXY(), 
			actor->m_definition.m_collision.m_radius, 
			FloatRange(actor->m_position.z, actor->m_position.z + actor->m_definition.m_collision.m_height)
		);
		if (result.m_didImpact && result.m_impactDist < nearestImpactDist) {
			nearestResult = result;
			nearestImpactDist = result.m_impactDist;
			*hitActorPointer = actor;
		}
	}
	return nearestResult;
}

//-----------------------------------------------------------------------------------------------
bool Map::SectorDetectWorldActors(Vec3 const& center, Vec3 const& forward, float radius, float angleDegrees, Actor* owner, std::vector<Actor*>* out_actors) const {
	bool foundAny = false;
	float cosThreshold = CosDegrees(angleDegrees * 0.5f);

	for (Actor* actor : m_actors) {
		if (actor == nullptr || actor == owner || actor->m_definition.m_name == "SpawnPoint" || actor->m_isDead) {
			continue;
		}

		float closestZ = GetClamped(center.z, actor->m_position.z, actor->m_position.z + actor->m_definition.m_collision.m_height);
		Vec3 closestAxisPoint = Vec3(actor->m_position.x, actor->m_position.y, closestZ);

		Vec3 toActorAxis = closestAxisPoint - center;
		float distanceToAxis = toActorAxis.GetLength();

		float distanceToSurface = distanceToAxis - actor->m_definition.m_collision.m_radius;

		if (distanceToSurface < radius) {
			if (distanceToAxis < 0.001f) {
				out_actors->push_back(actor);
				foundAny = true;
			}
			else {
				Vec3 toActorDir = toActorAxis / distanceToAxis;
				if (DotProduct3D(toActorDir, forward) > cosThreshold) {
					out_actors->push_back(actor);
					foundAny = true;
				}
			}
		}
	}
	return foundAny;
}


//-----------------------------------------------------------------------------------------------
void Map::SpawnActors() {
	for (SpawnInfo spawnInfo : m_definition.m_spawnInfos) {
		SpawnActor(spawnInfo);
	}
}

//-----------------------------------------------------------------------------------------------
void Map::RespawnPlayers() {
	for (int i = 0; i < (int)m_respawnActorsHandle.size(); i++) {
		if (!m_respawnActorsHandle[i]->IsValid()) {
			continue;
		}
		ActorHandle* oldHandle = m_respawnActorsHandle[i];
		Actor* actor = GetActorByHandle(*m_respawnActorsHandle[i]);
		if (actor == nullptr) {
			Actor* playerSpawnPoint = GetRandomSpwanPoint();
			SpawnInfo playerSpawnInfo;
			playerSpawnInfo.m_actorName = "Marine";
			playerSpawnInfo.m_spawnPosition = playerSpawnPoint == nullptr ? Vec3() : playerSpawnPoint->m_position;
			playerSpawnInfo.m_spawnOrientation = playerSpawnPoint == nullptr ? EulerAngles() : playerSpawnPoint->m_orientation;
			Actor* playerActor = SpawnActor(playerSpawnInfo);
			m_respawnActorsHandle[i] = playerActor->m_handle;
			if (m_game->m_playerKeyboardController != nullptr &&
				m_game->m_playerKeyboardController->m_possessedActorHandle == oldHandle) {
				m_game->m_playerKeyboardController->Possess(playerActor->m_handle);
			}
			else if (m_game->m_playerGamepadController != nullptr &&
				m_game->m_playerGamepadController->m_possessedActorHandle == oldHandle) {
				m_game->m_playerGamepadController->Possess(playerActor->m_handle);
			}
			else {
				ERROR_AND_DIE("Player respawn failed due to invalid controller possessed handle!");
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
Actor* Map::SpawnActor(SpawnInfo const& spawnInfo) {
	if (ActorDefinition::s_definitions.find(spawnInfo.m_actorName) == ActorDefinition::s_definitions.end()) {
		return nullptr;
	}

	int emptySlotIndex = -1;
	for (size_t i = 0; i < m_actors.size(); ++i) {
		if (m_actors[i] == nullptr && m_nextActorUID <= ActorHandle::MAX_ACTOR_UID) {
			emptySlotIndex = (int)i;
			break;
		}
	}

	if (m_nextActorUID > ActorHandle::MAX_ACTOR_UID) {
		ERROR_AND_DIE("Exceeded maximum number of actor UID limit!");
	}
	unsigned int currentUID = m_nextActorUID;
	m_nextActorUID++;

	if (emptySlotIndex == -1) {
		emptySlotIndex = (int)m_actors.size();
		m_actors.push_back(nullptr);
	}

	m_actors[emptySlotIndex] = (Actor*)1;

	Actor* newActor = new Actor(
		ActorDefinition::s_definitions.at(spawnInfo.m_actorName),
		this,
		new ActorHandle(currentUID, (unsigned int)emptySlotIndex),
		spawnInfo.m_spawnPosition,
		spawnInfo.m_spawnOrientation,
		spawnInfo.m_spawnScale
	);

	m_actors[emptySlotIndex] = newActor;

	if (newActor->m_definition.m_actorAI.m_isComplex) {
		newActor->m_controller->Possess(newActor->m_handle);
	}

	if (newActor->m_definition.m_isBoss) {
		m_bossActorHandle = newActor->m_handle;
	}

	return newActor;
}

//-----------------------------------------------------------------------------------------------
Actor* Map::SpawnPlayerActor(SpawnInfo const& spawnInfo, PlayerController* controller) {
	Actor* playerActor = SpawnActor(spawnInfo);
	controller->Possess(playerActor->m_handle);
	m_respawnActorsHandle.push_back(playerActor->m_handle);

	return playerActor;
}

//-----------------------------------------------------------------------------------------------
Actor* Map::GetActorByHandle(ActorHandle const handle) const {
	if (handle.IsValid()) {
		unsigned int index = handle.GetIndex();
		if (index < m_actors.size() && m_actors[index]!=nullptr && *(m_actors[index]->m_handle) == handle) {
			return m_actors[index];
		}
	}
	return nullptr;
}

//-----------------------------------------------------------------------------------------------
void Map::AddTempActor(TempActor* tempActor, Vec3 addPosition) {
	if (tempActor == nullptr) return;
	bool findEmptySlot = false;
	for (size_t i = 0; i < m_tempActors.size(); ++i) {
		if (m_tempActors[i] == nullptr) {
			m_tempActors[i] = tempActor;
			findEmptySlot = true;
			break;
		}
	}
	if (!findEmptySlot) {
		m_tempActors.push_back(tempActor);
	}
	tempActor->m_position = addPosition;
}

//-----------------------------------------------------------------------------------------------
Actor* Map::GetRandomSpwanPoint() const {
	std::vector<Actor*> spawnPoints;
	for (Actor* actor : m_actors) {
		if (actor!=nullptr && actor->m_definition.m_name._Equal("SpawnPoint")) {
			spawnPoints.push_back(actor);
		}
	}
	if (spawnPoints.empty()) {
		return nullptr;
	}
	int spawnPointIndex = m_game->m_randomGenerator->RollRandomIntInRange(0, (int)spawnPoints.size() - 1);
	return spawnPoints[spawnPointIndex];
}

//-----------------------------------------------------------------------------------------------
Actor* Map::GetNextValidActorLoop(ActorHandle const curHandle) const {
	if (!curHandle.IsValid()) {
		return nullptr;
	}
	unsigned int startIndex = curHandle.GetIndex();
	unsigned int avilableIndex = (startIndex + 1) % m_actors.size();
	while (avilableIndex != startIndex)
	{
		if (m_actors[avilableIndex] != nullptr) {
			return m_actors[avilableIndex];
		}
		avilableIndex = (avilableIndex + 1) % m_actors.size();
	}
	return nullptr;
}

//-----------------------------------------------------------------------------------------------
Actor* Map::GetNearestActor(Actor* source, std::string const& faction) const {
	Actor* nearestActor = nullptr;
	float nearestDistSqr = FLT_MAX;
	for (Actor* actor : m_actors) {
		if (actor == nullptr || 
			actor == source || 
			actor->m_isDead ||
			actor->m_definition.m_faction == "undefinedFaction" || 
			actor->m_definition.m_faction == faction) {
			continue;
		}

		float distSqr = (actor->m_position - source->m_position).GetLengthSquared();
		if (distSqr < nearestDistSqr) {
			nearestDistSqr = distSqr;
			nearestActor = actor;
		}
	}
	return nearestActor;
}

//-----------------------------------------------------------------------------------------------
void Map::UpdatePointLights() {
	m_pointLights.clear();

	for (Actor* actor : m_actors) {
		if (m_pointLights.size() >= MAX_POINT_LIGHTS) {
			break;
		}
		if (actor != nullptr) {
			if (actor->m_definition.m_pointLight.m_intensity > 0.f) {
				m_pointLights.push_back(
					PointLight{
						actor->m_position + Vec3(0.f,0.f,actor->m_definition.m_collision.m_height * 0.5f),
						actor->m_definition.m_pointLight.m_radius,
					    {
						(float)actor->m_definition.m_pointLight.m_color.r / 255.f,
						(float)actor->m_definition.m_pointLight.m_color.g / 255.f,
						(float)actor->m_definition.m_pointLight.m_color.b / 255.f,
						1.f
					    },
						actor->m_definition.m_pointLight.m_intensity,
						actor->m_definition.m_pointLight.m_volumetric
					}
				);
			}
		}
	}

	for (TempActor* tempActor : m_tempActors) {
		if (m_pointLights.size() >= MAX_POINT_LIGHTS){
			break;
		}
		if (tempActor != nullptr) {
			if (tempActor->m_pointLightIntensity > 0.f) {
				m_pointLights.push_back(
					PointLight{
						tempActor->m_position + tempActor->m_pointLightOffset,
						tempActor->m_pointLightRadius,
						{
						(float)tempActor->m_pointLightColor.r / 255.f,
						(float)tempActor->m_pointLightColor.g / 255.f,
						(float)tempActor->m_pointLightColor.b / 255.f,
						1.f
						},
						tempActor->m_pointLightIntensity,
						tempActor->m_pointLightVolumetric
					}
				);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::QuickSortActorsByDepth(Actor** actors, int left, int right, Vec3 const& cameraPos) const {
	int i = left;
	int j = right;
	float pivotDistSqr = (actors[(left + right) / 2]->m_position - cameraPos).GetLengthSquared();

	while (i <= j) {
		while ((actors[i]->m_position - cameraPos).GetLengthSquared() > pivotDistSqr) {
			i++;
		}
		while ((actors[j]->m_position - cameraPos).GetLengthSquared() < pivotDistSqr) {
			j--;
		}

		if (i <= j) {
			Actor* temp = actors[i];
			actors[i] = actors[j];
			actors[j] = temp;
			i++;
			j--;
		}
	}

	if (left < j) {
		QuickSortActorsByDepth(actors, left, j, cameraPos);
	}
	if (i < right) {
		QuickSortActorsByDepth(actors, i, right, cameraPos);
	}
}