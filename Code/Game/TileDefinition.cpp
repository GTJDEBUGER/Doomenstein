#include "Game/TileDefinition.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/NamedStrings.hpp"

//-----------------------------------------------------------------------------------------------
std::map<std::string, TileDefinition> TileDefinition::s_definitions;

//-----------------------------------------------------------------------------------------------
TileDefinition::TileDefinition(
	std::string name,
	bool isSolid,
	Rgba8 mapImagePixelColor,
	AABB2 wallUV,
	AABB2 floorUV,
	AABB2 ceilingUV,
	AABB2 topUV,
	AABB2 bottomUV,
	Rgba8 tintColor) :
	m_name(name),
	m_isSolid(isSolid),
	m_mapImagePixelColor(mapImagePixelColor),
	m_wallUV(wallUV),
	m_floorUV(floorUV),
	m_ceilingUV(ceilingUV),
	m_topUV(topUV),
	m_bottomUV(bottomUV),
	m_tintColor(tintColor) {
}

//-----------------------------------------------------------------------------------------------
void TileDefinition::InitializeTileDefs() {
	Texture* m_tileTexture = g_engine->m_renderer->CreateOrGetTextureFromFile("Data/Images/Terrain_8x8.png");
	SpriteSheet m_tileSpriteSheet(*m_tileTexture, IntVec2(8, 8));

	XmlDocument doc;
	doc.LoadFile("Data/Definitions/TileDefinitions.xml");
	XmlElement* rootElement = doc.FirstChildElement();
	for (XmlElement* i = rootElement->FirstChildElement(); i != nullptr; i = i->NextSiblingElement()) {
		IntVec2 wallSpriteCoords = ParseXmlAttribute(*i, "wallSpriteCoords", IntVec2(-1, -1));
		int wallSpriteIndex = wallSpriteCoords.x + wallSpriteCoords.y * 8;
		IntVec2 floorSpriteCoords = ParseXmlAttribute(*i, "floorSpriteCoords", IntVec2(-1, -1));
		int floorSpriteIndex = floorSpriteCoords.x + floorSpriteCoords.y * 8;
		IntVec2 ceilingSpriteCoords = ParseXmlAttribute(*i, "ceilingSpriteCoords", IntVec2(-1, -1));
		int ceilingSpriteIndex = ceilingSpriteCoords.x + ceilingSpriteCoords.y * 8;
		IntVec2 topSpriteCoords = ParseXmlAttribute(*i, "topSpriteCoords", IntVec2(-1, -1));
		int topSpriteIndex = topSpriteCoords.x + topSpriteCoords.y * 8;
		IntVec2 bottomSpriteCoords = ParseXmlAttribute(*i, "bottomSpriteCoords", IntVec2(-1, -1));
		int bottomSpriteIndex = bottomSpriteCoords.x + bottomSpriteCoords.y * 8;

		s_definitions[ParseXmlAttribute(*i, "name", "undefineTile")] = TileDefinition(
			ParseXmlAttribute(*i, "name", "undefineTile"),
			ParseXmlAttribute(*i, "isSolid", false),
			ParseXmlAttribute(*i, "mapImagePixelColor", Rgba8(0, 0, 0)),
			wallSpriteIndex >=0 ? m_tileSpriteSheet.GetSpriteUVs(wallSpriteIndex) : AABB2::ZERO_TO_ONE,
			floorSpriteIndex >=0 ? m_tileSpriteSheet.GetSpriteUVs(floorSpriteIndex) : AABB2::ZERO_TO_ONE,
			ceilingSpriteIndex >=0 ? m_tileSpriteSheet.GetSpriteUVs(ceilingSpriteIndex) : AABB2::ZERO_TO_ONE,
			topSpriteIndex >= 0 ? m_tileSpriteSheet.GetSpriteUVs(topSpriteIndex) : AABB2::ZERO_TO_ONE,
			bottomSpriteIndex >= 0 ? m_tileSpriteSheet.GetSpriteUVs(bottomSpriteIndex) : AABB2::ZERO_TO_ONE,
			ParseXmlAttribute(*i, "tint", Rgba8(255, 0, 255))
		);
	}
}