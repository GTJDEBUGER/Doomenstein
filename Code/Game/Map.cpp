#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/Tile.hpp"
#include "Game/Actor.hpp"
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
			m_tiles.emplace_back(IntVec2(x, y), m_definition.m_tileSize, *tileDef);
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
			AABB2 const& wallUV = tile->m_definition.m_wallUV;

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

			if (tile->m_definition.m_floorUV != AABB2::ZERO_TO_ONE)   AddGeometryForFloor(worldMesh, tile->m_definition.m_floorUV);
			if (tile->m_definition.m_ceilingUV != AABB2::ZERO_TO_ONE) AddGeometryForCeiling(worldMesh, tile->m_definition.m_ceilingUV);
			if (tile->m_definition.m_topUV != AABB2::ZERO_TO_ONE)     AddGeometryForTop(worldMesh, tile->m_definition.m_topUV);
			if (tile->m_definition.m_bottomUV != AABB2::ZERO_TO_ONE)  AddGeometryForBottom(worldMesh, tile->m_definition.m_bottomUV);
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
bool Map::ShouldRenderFaceAgainstNeighbor(int x, int y) const {
	Tile const* neighbor = GetTile(x, y);
	return neighbor == nullptr || !neighbor->m_definition.m_isSolid;
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
	for (Actor* actor : m_actors) {
		if (actor != nullptr) {
			actor->Update((float)m_game->m_gameClock->GetDeltaSeconds());
		}
	}

	float deltaTime = (float)m_game->m_gameClock->GetDeltaSeconds();
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
	UpdatePointLights();
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors() {
	for (size_t i = 0; i < m_actors.size(); ++i) {
		if (
			m_actors[i]==nullptr || 
			m_actors[i]->m_isDead ||
			!m_actors[i]->m_definition.m_collision.m_collisionWithActors
		) {
			continue;
		}
		for (size_t j = i + 1; j < m_actors.size(); ++j) {
			if (
				m_actors[j] == nullptr || 
				m_actors[j]->m_isDead ||
				!m_actors[j]->m_definition.m_collision.m_collisionWithActors ||
				(m_actors[i]->m_owner != nullptr && m_actors[i]->m_owner == m_actors[j]) ||
				(m_actors[j]->m_owner != nullptr && m_actors[j]->m_owner == m_actors[i])
			) {
				continue;
			}
			CollideActors(m_actors[i], m_actors[j]);
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

	isPushed |= PushActorOutOfTileIfSolid(actor, tx, ty);

	isPushed |= PushActorOutOfTileIfSolid(actor, tx + 1, ty); // North
	isPushed |=PushActorOutOfTileIfSolid(actor, tx - 1, ty); // South
	isPushed |=PushActorOutOfTileIfSolid(actor, tx, ty + 1); // West
	isPushed |=PushActorOutOfTileIfSolid(actor, tx, ty - 1); // East

	isPushed |=PushActorOutOfTileIfSolid(actor, tx - 1, ty + 1); // SW
	isPushed |=PushActorOutOfTileIfSolid(actor, tx - 1, ty - 1); // SE
	isPushed |=PushActorOutOfTileIfSolid(actor, tx + 1, ty + 1); // NW
	isPushed |=PushActorOutOfTileIfSolid(actor, tx + 1, ty - 1); // NE

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
	if (!tile || tile->m_definition.m_floorUV == AABB2::ZERO_TO_ONE ) {
		return isPushed;
	}

	if (actor->m_position.z < 0.f) {
		actor->m_position.z = 0.f;
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

	Vec3 idealSunPos = GetMapWorldCenter() - (shadowSunDir * 200.f);
	EulerAngles sunOrientation = EulerAngles::MakeLookDirectionEulerAngles(shadowSunDir);

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
	g_engine->m_renderer->BindTexture(m_definition.m_mapRoughnessTexture, TextureSlot::ROUGHNESS);
	g_engine->m_renderer->BindTexture(m_definition.m_mapMetallicTexture, TextureSlot::METALLIC);
	g_engine->m_renderer->BindTexture(nullptr, TextureSlot::EMISSIVE);
	g_engine->m_renderer->BindTexture(nullptr, TextureSlot::SHADOWMAP);
	g_engine->m_renderer->DrawVertexArray(m_mapVerts, m_mapIndexs, m_definition.m_mapShader);
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

	Vec3 cameraPos = viewCamera.GetPosition();

	int actorCount = (int)m_actors.size();
	if (actorCount <= 0) return;

	Actor* tempActors[1000];
	int canRenderActorNum = actorCount > 1000 ? 1000 : actorCount;
	int actualActorNum = 0;
	for (int i = 0; i < canRenderActorNum; ++i) {
		if (m_actors[i] != nullptr) {
			tempActors[actualActorNum] = m_actors[i];
			actualActorNum++;
		}
	}

	for (int i = 0; i < actualActorNum - 1; ++i) {
		for (int j = 0; j < actualActorNum - i - 1; ++j) {
			Vec3 posA = tempActors[j]->m_position;
			Vec3 posB = tempActors[j + 1]->m_position;

			float d2A = (posA.x - cameraPos.x) * (posA.x - cameraPos.x) +
				(posA.y - cameraPos.y) * (posA.y - cameraPos.y) +
				(posA.z - cameraPos.z) * (posA.z - cameraPos.z);

			float d2B = (posB.x - cameraPos.x) * (posB.x - cameraPos.x) +
				(posB.y - cameraPos.y) * (posB.y - cameraPos.y) +
				(posB.z - cameraPos.z) * (posB.z - cameraPos.z);

			if (d2A < d2B) {
				Actor* temp = tempActors[j];
				tempActors[j] = tempActors[j + 1];
				tempActors[j + 1] = temp;
			}
		}
	}

	for (int i = 0; i < actualActorNum; ++i) {
		tempActors[i]->Render(viewCamera);
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
		return (currentZ >= 0.f && currentZ <= m_definition.m_tileSize);
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

	float tDeltaX = (direction.x != 0.f) ? abs(m_definition.m_tileSize / direction.x) : FLT_MAX;
	float tDeltaY = (direction.y != 0.f) ? abs(m_definition.m_tileSize / direction.y) : FLT_MAX;

	float nextBoundaryX = (curTile.x + (stepX > 0 ? 1.f : 0.f)) * m_definition.m_tileSize;
	float nextBoundaryY = (curTile.y + (stepY > 0 ? 1.f : 0.f)) * m_definition.m_tileSize;

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

	float tileSize = m_definition.m_tileSize;

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
			const TileDefinition& tileDef = tile->m_definition;
			bool hasSurface = false;

			if (direction.z < 0.f) {
				hasSurface = (i == 0) ? (tileDef.m_topUV != AABB2::ZERO_TO_ONE)
					: (tileDef.m_floorUV != AABB2::ZERO_TO_ONE);
			}
			else {
				hasSurface = (i == 0) ? (tileDef.m_bottomUV != AABB2::ZERO_TO_ONE)
					: (tileDef.m_ceilingUV != AABB2::ZERO_TO_ONE);
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
bool Map::SectorDetectWorldActors(Vec3 const& center, Vec3 const& forward, float radius, float angleDegrees, Actor* owner, std::vector<Actor *>* out_actors) const {
	bool foundAny = false;
	float cosThreshold = CosDegrees(angleDegrees * 0.5f);
	for (Actor* actor : m_actors) {
		if (actor == nullptr || actor == owner || actor->m_definition.m_name=="SpawnPoint" || actor->m_isDead) {
			continue;
		}
		if (center.z <= actor->m_position.z || center.z >= actor->m_position.z + actor->m_definition.m_collision.m_height) {
			continue;
		}
		Vec3 toActor = actor->m_position - center;
		toActor.z = 0.f;
		float distanceToActor = toActor.GetLength() - actor->m_definition.m_collision.m_radius;
		toActor /= distanceToActor;
		if (distanceToActor < radius && DotProduct3D(toActor, forward) > cosThreshold) {
			out_actors->push_back(actor);
			foundAny = true;
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
				m_game->m_playerKeyboardController->m_deadCount += 1;
			}
			else if (m_game->m_playerGamepadController != nullptr &&
				m_game->m_playerGamepadController->m_possessedActorHandle == oldHandle) {
				m_game->m_playerGamepadController->Possess(playerActor->m_handle);
				m_game->m_playerGamepadController->m_deadCount += 1;
			}
			else {
				ERROR_AND_DIE("Player respawn failed due to invalid controller possessed handle!");
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
Actor* Map::SpawnActor(SpawnInfo const& spawnInfo){
	Actor* newActor = nullptr;
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

	if (emptySlotIndex == -1 && m_nextActorUID <= ActorHandle::MAX_ACTOR_UID) {
		emptySlotIndex = (int)m_actors.size();
		newActor = new Actor(
			ActorDefinition::s_definitions.at(spawnInfo.m_actorName),
			this,
			new ActorHandle(m_nextActorUID, (unsigned int)emptySlotIndex),
			spawnInfo.m_spawnPosition,
			spawnInfo.m_spawnOrientation,
			spawnInfo.m_spawnScale
		);
		m_actors.push_back(newActor);
	}else if(emptySlotIndex > -1 && m_nextActorUID <= ActorHandle::MAX_ACTOR_UID){
		newActor = new Actor(
			ActorDefinition::s_definitions.at(spawnInfo.m_actorName),
			this,
			new ActorHandle(m_nextActorUID, (unsigned int)emptySlotIndex),
			spawnInfo.m_spawnPosition,
			spawnInfo.m_spawnOrientation,
			spawnInfo.m_spawnScale
		);
		m_actors[emptySlotIndex] = newActor;
	}
	else {
		ERROR_AND_DIE("Exceeded maximum number of actor UID limit!");
	}
	++m_nextActorUID;

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
			if (m_pointLights.size() >= MAX_POINT_LIGHTS){
				break;
			}
		}
	}
}