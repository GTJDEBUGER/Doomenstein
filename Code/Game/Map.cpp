#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/Tile.hpp"
#include "Game/Actor.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/Player.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Camera.hpp"

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
}

//-----------------------------------------------------------------------------------------------
Map::~Map() {
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
	for (int x = 0; x < m_dimensions.x; x++) {
		for (int y = 0; y < m_dimensions.y; y++) {
			Rgba8 pixelColor = m_definition->m_mapImage.GetTexelColor(IntVec2(x,y));
			TileDefinition* tileDef = nullptr;
			for (auto& def : TileDefinition::s_definitions) {
				if (def.second.m_mapImagePixelColor == pixelColor) {
					tileDef = &def.second;
					break;
				}
			}
			if(tileDef == nullptr) {
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
				if (ShouldRenderFaceAgainstNeighbor(x, y + 1)) {
					AddGeometryForFrontWall(worldMesh, wallUV);
				}
				if (ShouldRenderFaceAgainstNeighbor(x, y - 1)) {
					AddGeometryForBackWall(worldMesh, wallUV);
				}
				if (ShouldRenderFaceAgainstNeighbor(x + 1, y)) {
					AddGeometryForLeftWall(worldMesh, wallUV);
				}
				if (ShouldRenderFaceAgainstNeighbor(x - 1, y)) {
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
	UpdateSunShadowCamera();
	CollideActorsWithMap();
	CollideActors();
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors() {
	//TODO
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActors(Actor* actorA, Actor* actorB) {
	//TODO
}

//-----------------------------------------------------------------------------------------------	
void Map::CollideActorsWithMap() {
	//TODO
}

//-----------------------------------------------------------------------------------------------
void Map::CollideActorWithMap(Actor* actor) {
	//TODO
}

//-----------------------------------------------------------------------------------------------
void Map::UpdateSunShadowCamera() {
	Vec3 sunDir = m_sunDirection.GetNormalized();
	Vec3 sunPos = GetMapWorldCenter() - (sunDir * 200.f);
	EulerAngles sunOrientation = EulerAngles::MakeLookDirectionEulerAngles(sunDir);
	m_sunShadowCamera->SetPosition(sunPos);
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

	g_engine->m_renderer->BindTexture(m_definition->m_mapTexture, TextureSlot::DIFFUSE);
	g_engine->m_renderer->BindTexture(m_definition->m_mapNormalTexture, TextureSlot::NORMAL);
	g_engine->m_renderer->BindTexture(m_definition->m_mapAOTexture, TextureSlot::AO);
	g_engine->m_renderer->BindTexture(m_definition->m_mapRoughnessTexture, TextureSlot::ROUGHNESS);
	g_engine->m_renderer->BindTexture(m_definition->m_mapMetallicTexture, TextureSlot::METALLIC);
	g_engine->m_renderer->BindTexture(nullptr, TextureSlot::SHADOWMAP);
	g_engine->m_renderer->DrawIndexedVertexBuffer(m_mapVertexBuffer, m_mapIndexBuffer, static_cast<unsigned int>(m_mapIndexs.size()));
	g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
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
	//TODO
	return RaycastResult3D();
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const {
	//TODO
	return RaycastResult3D();
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const {
	//TODO
	return RaycastResult3D();
}

//-----------------------------------------------------------------------------------------------
RaycastResult3D Map::RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner) const {
	//TODO
	return RaycastResult3D();
}