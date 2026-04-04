#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/Tile.hpp"
#include "Game/Actor.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/Player.hpp"
#include "Game/Demon.hpp"
#include "Game/Projectile.hpp"
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
//-----------------------------------------------------------------------------------------------
Map::Map(Game* game, MapDefinition const* definition)
	: m_game(game)
	, m_definition(definition)
{
	m_shadowMapShader = g_engine->m_renderer->CreateShader("ShadowMap", VertexType::PCUTBN);
	CreateCameras();
	CreateTiles();
	CreateGeometry();
	CreateBuffers();
	SpawnActors();
}

//-----------------------------------------------------------------------------------------------
Map::~Map() {
	for (Actor* actor : m_actors) {
		delete actor;
	}
	delete m_sunShadowCamera;
	m_sunShadowCamera = nullptr;
	delete m_mapVertexBuffer;
	m_mapVertexBuffer = nullptr;
	delete m_mapIndexBuffer;
	m_mapIndexBuffer = nullptr;
}

//-----------------------------------------------------------------------------------------------
void Map::CreateCameras() {
	m_sunShadowCamera = new Camera(Vec2(-150.f, -150.f), Vec2(150.f, 150.f), 0.001f, 350.f);
	UpdateSunShadowCamera();
}

//-----------------------------------------------------------------------------------------------
void Map::CreateTiles() {
	m_dimensions = IntVec2(m_definition->m_mapImage.GetDimentions().x, m_definition->m_mapImage.GetDimentions().y);
	m_tiles.reserve(m_dimensions.x * m_dimensions.y);
	for (int y = 0; y < m_dimensions.y; y++) {
		for (int x = 0; x < m_dimensions.x; x++) {
			Rgba8 pixelColor = m_definition->m_mapImage.GetTexelColor(IntVec2(x, y));
			TileDefinition* tileDef = nullptr;
			for (auto& def : TileDefinition::s_definitions) {
				if (def.second.m_mapImagePixelColor == pixelColor) {
					tileDef = &def.second;
					break;
				}
			}
			if (tileDef == nullptr) {
				ERROR_AND_DIE(
					Stringf(
						"No tile definition found for map image pixel color (%d, %d, %d, %d) at position (%d, %d)",
						pixelColor.r,
						pixelColor.g,
						pixelColor.b,
						pixelColor.a,
						x,
						y
					)
				);
			}
			m_tiles.emplace_back(IntVec2(x, y), m_definition->m_tileSize, tileDef);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::CreateGeometry() {
	for (int y = 0; y < m_dimensions.y; ++y) {
		for (int x = 0; x < m_dimensions.x; ++x) {
			const Tile* tile = GetTile(x, y);
			if (!tile) continue;

			AABB3 worldMesh = tile->GetWorldMesh();
			AABB2 const& wallUV = tile->m_definition->m_wallUV;

			if (wallUV != AABB2::ZERO_TO_ONE) {
				if (ShouldRenderFaceAgainstNeighbor(x + 1, y)) {
					AddGeometryForFrontWall(worldMesh, wallUV);
				}
				if (ShouldRenderFaceAgainstNeighbor(x - 1, y)) {
					AddGeometryForBackWall(worldMesh, wallUV);
				}
				if (ShouldRenderFaceAgainstNeighbor(x, y + 1)) {
					AddGeometryForLeftWall(worldMesh, wallUV);
				}
				if (ShouldRenderFaceAgainstNeighbor(x, y - 1)) {
					AddGeometryForRightWall(worldMesh, wallUV);
				}
			}

			if (tile->m_definition->m_floorUV != AABB2::ZERO_TO_ONE)   AddGeometryForFloor(worldMesh, tile->m_definition->m_floorUV);
			if (tile->m_definition->m_ceilingUV != AABB2::ZERO_TO_ONE) AddGeometryForCeiling(worldMesh, tile->m_definition->m_ceilingUV);
			if (tile->m_definition->m_topUV != AABB2::ZERO_TO_ONE)     AddGeometryForTop(worldMesh, tile->m_definition->m_topUV);
			if (tile->m_definition->m_bottomUV != AABB2::ZERO_TO_ONE)  AddGeometryForBottom(worldMesh, tile->m_definition->m_bottomUV);
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
void Map::CreateBuffers() {
	m_mapVertexBuffer = g_engine->m_renderer->CreateVertexBuffer(static_cast<unsigned int>(m_mapVerts.size()) * sizeof(Vertex_TBN), sizeof(Vertex_TBN));
	m_mapIndexBuffer = g_engine->m_renderer->CreateIndexBuffer(static_cast<unsigned int>(m_mapIndexs.size()) * sizeof(unsigned int));
}

//-----------------------------------------------------------------------------------------------
bool Map::ShouldRenderFaceAgainstNeighbor(int x, int y) const {
	Tile const* neighbor = GetTile(x, y);
	return neighbor == nullptr || !neighbor->m_definition->m_isSolid;
}

//-----------------------------------------------------------------------------------------------
bool Map::IsTileSolid(int x, int y) const {
	Tile const* tile = GetTile(x, y);
	return tile != nullptr && tile->m_definition->m_isSolid;
}

//-----------------------------------------------------------------------------------------------
bool Map::IsPositionInBounds(Vec3 const& position) const {
	return position.x >= 0.f && position.x < static_cast<float>(m_dimensions.x) * m_definition->m_tileSize 
		&& position.y >= 0.f && position.y < static_cast<float>(m_dimensions.y) * m_definition->m_tileSize
		&& position.z >= 0.f && position.z < m_definition->m_tileSize;
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
		static_cast<float>(m_dimensions.x) * m_definition->m_tileSize * 0.5f,
		static_cast<float>(m_dimensions.y) * m_definition->m_tileSize * 0.5f,
		m_definition->m_tileSize * 0.5f
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
	for (Actor* actor : m_actors) {
		actor->Update((float)m_game->m_gameClock->GetDeltaSeconds());
	}
	UpdateSunShadowCamera();

	CollideActorsWithMap();
	CollideActors();
	CollideActorsWithMap();
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors() {
	for (size_t i = 0; i < m_actors.size(); ++i) {
		for (size_t j = i + 1; j < m_actors.size(); ++j) {
			CollideActors(m_actors[i], m_actors[j]);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors(Actor* actorA, Actor* actorB) {
	bool isZPush = false;
	if (actorA->m_isStatic && actorB->m_isStatic) {
		return;
	}
	else if (!actorA->m_isStatic && actorB->m_isStatic) {
		PushZCylinderOutOfFixedZCylinder3D(
			actorA->m_position,
			actorA->m_physicsRadius,
			FloatRange(actorA->m_position.z, actorA->m_position.z + actorA->m_physicsHeight),
			actorB->m_position,
			actorB->m_physicsRadius,
			FloatRange(actorB->m_position.z, actorB->m_position.z + actorB->m_physicsHeight),
			&isZPush
		);
		if (isZPush) {
			actorA->m_velocity.z = 0.f;
		}
	}
	else if (actorA->m_isStatic && !actorB->m_isStatic) {
		PushZCylinderOutOfFixedZCylinder3D(
			actorB->m_position,
			actorB->m_physicsRadius,
			FloatRange(actorB->m_position.z, actorB->m_position.z + actorB->m_physicsHeight),
			actorA->m_position,
			actorA->m_physicsRadius,
			FloatRange(actorA->m_position.z, actorA->m_position.z + actorA->m_physicsHeight),
			&isZPush
		);
		if (isZPush) {
			actorB->m_velocity.z = 0.f;
		}
	}
	else {
		PushZCylindersOutOfEachOther3D(
			actorA->m_position,
			actorA->m_physicsRadius,
			FloatRange(actorA->m_position.z, actorA->m_position.z + actorA->m_physicsHeight),
			actorB->m_position,
			actorB->m_physicsRadius,
			FloatRange(actorB->m_position.z, actorB->m_position.z + actorB->m_physicsHeight),
			&isZPush
		);
		if (isZPush) {
			actorA->m_velocity.z = 0.f;
			actorB->m_velocity.z = 0.f;
		}
	}
}

IntVec2 Map::GetTileCoordsForPosition(Vec3 const& position) const {
	return IntVec2(
		(int)floorf(position.x / m_definition->m_tileSize),
		(int)floorf(position.y / m_definition->m_tileSize)
	);
}

//-----------------------------------------------------------------------------------------------	
void Map::CollideActorsWithMap() {
	for (Actor* actor : m_actors) {
		if (actor->m_isStatic) {
			continue;
		}
		if (actor->m_position.z < 0.f) {
			actor->m_position.z = 0.f;
		}
		CollideActorWithMap(actor);
	}
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActorWithMap(Actor* actor) {
	if (!actor) return;

	IntVec2 tileCoords = GetTileCoordsForPosition(actor->m_position);
	int tx = tileCoords.x;
	int ty = tileCoords.y;

	PushActorOutOfTileIfSolid(actor, tx, ty);

	PushActorOutOfTileIfSolid(actor, tx + 1, ty); // North
	PushActorOutOfTileIfSolid(actor, tx - 1, ty); // South
	PushActorOutOfTileIfSolid(actor, tx, ty + 1); // West
	PushActorOutOfTileIfSolid(actor, tx, ty - 1); // East

	PushActorOutOfTileIfSolid(actor, tx - 1, ty + 1); // SW
	PushActorOutOfTileIfSolid(actor, tx - 1, ty - 1); // SE
	PushActorOutOfTileIfSolid(actor, tx + 1, ty + 1); // NW
	PushActorOutOfTileIfSolid(actor, tx + 1, ty - 1); // NE
}

//-----------------------------------------------------------------------------------------------
void Map::PushActorOutOfTileIfSolid(Actor* actor, int tileX, int tileY) {
	const Tile* tile = GetTile(tileX, tileY);
	if (!tile || !tile->m_definition->m_isSolid) {
		return;
	}

	FloatRange currentZRange(actor->m_position.z, actor->m_position.z + actor->m_physicsHeight);
	PushZCylinderOutOfFixedAABB3D(actor->m_position, actor->m_physicsRadius, currentZRange, tile->GetWorldMesh());
}

//-----------------------------------------------------------------------------------------------
void Map::UpdateSunShadowCamera() {
	Vec3 sunDir = m_sunDirection.GetNormalized();

	Vec3 idealSunPos = GetMapWorldCenter() - (sunDir * 160.f);
	EulerAngles sunOrientation = EulerAngles::MakeLookDirectionEulerAngles(sunDir);

	float orthoWidth = 300.f;
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
}

//-----------------------------------------------------------------------------------------------
void Map::Render() const {
	g_engine->m_renderer->SetLightConstants(
		m_sunDirection.GetNormalized(),
		m_sunIntensity,
		m_ambientIntensity,
		GetSunShadowCameraViewProjMatrix()
	);

	g_engine->m_renderer->BindShader(m_definition->m_mapShader);

	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP, SamplerSlot::SLOT0);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::ANISOTROPIC_WARP, SamplerSlot::SLOT1);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::SHADOWMAP, SamplerSlot::SLOT2);

	g_engine->m_renderer->BindTexture(m_definition->m_mapTexture, TextureSlot::DIFFUSE_SCREEN);
	g_engine->m_renderer->BindTexture(m_definition->m_mapNormalTexture, TextureSlot::NORMAL_ORIGINALSCREEN);
	g_engine->m_renderer->BindTexture(m_definition->m_mapAOTexture, TextureSlot::AO_SCREENDEPTH);
	g_engine->m_renderer->BindTexture(m_definition->m_mapRoughnessTexture, TextureSlot::ROUGHNESS);
	g_engine->m_renderer->BindTexture(m_definition->m_mapMetallicTexture, TextureSlot::METALLIC);
	g_engine->m_renderer->BindTexture(nullptr, TextureSlot::SHADOWMAP);
	g_engine->m_renderer->DrawIndexedVertexBuffer(m_mapVertexBuffer, m_mapIndexBuffer, static_cast<unsigned int>(m_mapIndexs.size()));
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

	for (Actor* actor : m_actors) {
		actor->Render();
	}
}

//-----------------------------------------------------------------------------------------------
void Map::RenderShadowmap() const {
	g_engine->m_renderer->BindShader(m_shadowMapShader, true, false);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_engine->m_renderer->CopyCPUToGPU(
		m_mapVerts.data(),
		static_cast<unsigned int>(m_mapVerts.size() * sizeof(Vertex_TBN)),
		m_mapVertexBuffer
	);
	g_engine->m_renderer->BindVertexBuffer(m_mapVertexBuffer);
	g_engine->m_renderer->CopyCPUToGPU(
		m_mapIndexs.data(),
		static_cast<unsigned int>(m_mapIndexs.size() * sizeof(unsigned int)),
		m_mapIndexBuffer
	);
	g_engine->m_renderer->BindIndexBuffer(m_mapIndexBuffer);
	g_engine->m_renderer->DrawIndexedVertexBuffer(m_mapVertexBuffer, m_mapIndexBuffer, static_cast<unsigned int>(m_mapIndexs.size()));
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner) const {
	RaycastResult3D worldXYResult = RaycastWorldXY(start, direction, distance);
	RaycastResult3D worldZResult = RaycastWorldZ(start, direction, distance);
	RaycastResult3D worldActorsResult = RaycastWorldActors(start, direction, distance, owner);
	RaycastResult3D nearestResult = worldXYResult;

	if (worldZResult.m_didImpact) {
		if (!nearestResult.m_didImpact || worldZResult.m_impactDist < nearestResult.m_impactDist) {
			nearestResult = worldZResult;
		}
	}

	if (worldActorsResult.m_didImpact) {
		if (!nearestResult.m_didImpact || worldActorsResult.m_impactDist < nearestResult.m_impactDist) {
			nearestResult = worldActorsResult;
		}
	}

	return nearestResult;
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const {
	RaycastResult3D result;
	result.m_rayStartPos = start;
	result.m_rayFwdNormal = direction;
	result.m_rayMaxLength = distance;

	auto IsZInTileRange = [&](float t) {
		float currentZ = start.z + (direction.z * t);
		return (currentZ >= 0.f && currentZ <= m_definition->m_tileSize);
		};

	IntVec2 curTile = GetTileCoordsForPosition(start);

	if (IsTileSolid(curTile.x, curTile.y) && IsZInTileRange(0.f)) {
		result.m_didImpact = true;
		result.m_impactDist = 0.f;
		result.m_impactPos = start;
		result.m_impactNormal = -direction;
		return result;
	}

	int stepX = (direction.x > 0) ? 1 : -1;
	int stepY = (direction.y > 0) ? 1 : -1;

	float tDeltaX = (direction.x != 0.f) ? abs(m_definition->m_tileSize / direction.x) : FLT_MAX;
	float tDeltaY = (direction.y != 0.f) ? abs(m_definition->m_tileSize / direction.y) : FLT_MAX;

	float nextBoundaryX = (curTile.x + (stepX > 0 ? 1.f : 0.f)) * m_definition->m_tileSize;
	float nextBoundaryY = (curTile.y + (stepY > 0 ? 1.f : 0.f)) * m_definition->m_tileSize;

	float tMaxX = (direction.x != 0.f) ? (nextBoundaryX - start.x) / direction.x : FLT_MAX;
	float tMaxY = (direction.y != 0.f) ? (nextBoundaryY - start.y) / direction.y : FLT_MAX;

	while (true) {
		if (tMaxX < tMaxY) {
			if (tMaxX > distance) break;
			curTile.x += stepX;
			if (IsTileSolid(curTile.x, curTile.y) && IsZInTileRange(tMaxX)) {
				result.m_didImpact = true;
				result.m_impactDist = tMaxX;
				result.m_impactPos = start + direction * tMaxX;
				result.m_impactNormal = (stepX > 0) ? Vec3(-1.f, 0.f, 0.f) : Vec3(1.f, 0.f, 0.f);
				return result;
			}
			tMaxX += tDeltaX;
		}
		else {
			if (tMaxY > distance) break;
			curTile.y += stepY;
			if (IsTileSolid(curTile.x, curTile.y) && IsZInTileRange(tMaxY)) {
				result.m_didImpact = true;
				result.m_impactDist = tMaxY;
				result.m_impactPos = start + direction * tMaxY;
				result.m_impactNormal = (stepY > 0) ? Vec3(0.f, -1.f, 0.f) : Vec3(0.f, 1.f, 0.f);
				return result;
			}
			tMaxY += tDeltaY;
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

	float tileSize = m_definition->m_tileSize;

	float tValues[2];
	float zValues[2];
	Vec3 normals[2];

	if (direction.z < 0.f) {
		tValues[0] = (tileSize - start.z) / direction.z;
		zValues[0] = tileSize;
		normals[0] = Vec3(0.f, 0.f, 1.f);

		tValues[1] = (0.f - start.z) / direction.z;
		zValues[1] = 0.f;
		normals[1] = Vec3(0.f, 0.f, 1.f);
	}
	else {
		tValues[0] = (0.f - start.z) / direction.z;
		zValues[0] = 0.f;
		normals[0] = Vec3(0.f, 0.f, -1.f);

		tValues[1] = (tileSize - start.z) / direction.z;
		zValues[1] = tileSize;
		normals[1] = Vec3(0.f, 0.f, -1.f);
	}

	for (int i = 0; i < 2; ++i) {
		float t = tValues[i];
		if (t < 0.f || t > distance) continue;

		Vec3 impactPos = start + (direction * t);
		IntVec2 tileCoords = GetTileCoordsForPosition(impactPos);

		if (AreCoordsInBounds(tileCoords.x, tileCoords.y)) {
			const Tile* tile = GetTile(tileCoords.x, tileCoords.y);
			const TileDefinition* tileDef = tile->m_definition;
			bool hasSurface = false;

			if (direction.z < 0.f) {
				hasSurface = (i == 0) ? (tileDef->m_topUV != AABB2::ZERO_TO_ONE)
					: (tileDef->m_floorUV != AABB2::ZERO_TO_ONE);
			}
			else {
				hasSurface = (i == 0) ? (tileDef->m_bottomUV != AABB2::ZERO_TO_ONE)
					: (tileDef->m_ceilingUV != AABB2::ZERO_TO_ONE);
			}

			if (hasSurface) {
				result.m_didImpact = true;
				result.m_impactDist = t;
				result.m_impactPos = impactPos;
				result.m_impactNormal = normals[i];
				return result;
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner) const {
	float nearestImpactDist = distance;
	RaycastResult3D nearestResult;
	nearestResult.m_rayStartPos = start;
	nearestResult.m_rayFwdNormal = direction;
	nearestResult.m_rayMaxLength = distance;

	for (Actor* actor : m_actors) {
		if (actor == owner) {
			continue;
		}
		RaycastResult3D result = RaycastVsCylinderZ3D(
			start, 
			direction, 
			distance, 
			actor->m_position.GetXY(), 
			actor->m_physicsRadius, 
			FloatRange(actor->m_position.z, actor->m_position.z + actor->m_physicsHeight)
		);
		if (result.m_didImpact && result.m_impactDist < nearestImpactDist) {
			nearestResult = result;
			nearestImpactDist = result.m_impactDist;
		}
	}
	return nearestResult;
}

//-----------------------------------------------------------------------------------------------
ActorType Map::StringToActorType(std::string const& str) {
	if (str == "Demon") {
		return ActorType::DEMON;
	}else if (str == "Projectile") {
		return ActorType::PROJECTILE;
	}
	ERROR_AND_DIE(Stringf("Unknown actor type string: %s", str.c_str()));
	return ActorType::DEMON;
}

//-----------------------------------------------------------------------------------------------
void Map::SpawnActors() {
	for (auto i : m_definition->m_spawnInfos) {
		SpawnActor(
			StringToActorType(i.m_actorType),
			i.m_spawnPosition,
			i.m_spawnOrientation,
			i.m_spawnScale,
			i.m_isPhysicsSimul
		);
	}
}

//-----------------------------------------------------------------------------------------------
void Map::SpawnActor(ActorType type, Vec3 const& pos, EulerAngles const& ori, Vec3 const& scale, bool isPhysicsSimul) {
	switch (type)
	{
	case ActorType::DEMON:
		m_actors.push_back(new Demon(pos, ori, scale));
		break;
	case ActorType::PROJECTILE:
		m_actors.push_back(new Projectile(pos, ori, scale, isPhysicsSimul));
		break;
	default:
		ERROR_AND_DIE("Spawn unknown actor type!");
		break;
	}
}