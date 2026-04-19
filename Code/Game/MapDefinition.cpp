#include "Game/MapDefinition.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Engine.hpp"

//-----------------------------------------------------------------------------------------------
std::map<std::string, MapDefinition> MapDefinition::s_definitions;

//-----------------------------------------------------------------------------------------------
MapDefinition::MapDefinition(
	std::string name,
	Image mapImage,
	Shader* mapShader,
	Texture* mapTexture,
	Texture* mapNormalTexture,
	Texture* mapAOTexture,
	Texture* mapParallaxTexture,
	Texture* mapRoughnessTexture,
	Texture* mapMetallicTexture,
	IntVec2 mapTextureDimentions,
	float tileSize
	) :
	m_name(name),
	m_mapImage(mapImage),
	m_mapShader(mapShader),
	m_mapTexture(mapTexture),
	m_mapNormalTexture(mapNormalTexture),
	m_mapTextureDimentions(mapTextureDimentions),
	m_mapAOTexture(mapAOTexture),
	m_mapParallaxTexture(mapParallaxTexture),
	m_mapRoughnessTexture(mapRoughnessTexture),
	m_mapMetallicTexture(mapMetallicTexture),
	m_tileSize(tileSize){
}

//-----------------------------------------------------------------------------------------------
MapDefinition::~MapDefinition() {
}

//-----------------------------------------------------------------------------------------------
void MapDefinition::InitializeMapDefs(std::string configPath) {
	XmlDocument doc;
	doc.LoadFile(configPath.data());
	XmlElement* rootElement = doc.FirstChildElement();
	for (XmlElement* i = rootElement->FirstChildElement(); i != nullptr; i = i->NextSiblingElement()) {
		s_definitions[ParseXmlAttribute(*i, "name", "undefineMap")] = MapDefinition(
			ParseXmlAttribute(*i, "name", "undefineMap"),
			Image(ParseXmlAttribute(*i, "image", ParseXmlAttribute(*i, "image", "Data/Maps/TestMap.png")).data()),
			g_engine->m_renderer->CreateShader(ParseXmlAttribute(*i, "shader", "Error").data(), VertexType::PCUTBN),
			g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*i, "spriteSheetTexture", "Error").data()),
			g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*i, "spriteSheetNormalTexture", "Error").data()),
			g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*i, "spriteSheetAOTexture", "Error").data()),
			g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*i, "spriteSheetParallaxTexture", "Error").data()),
			g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*i, "spriteSheetRoughnessTexture", "Error").data()),
			g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*i, "spriteSheetMetallicTexture", "Error").data()),
			ParseXmlAttribute(*i, "spriteSheetTextureDimentions", IntVec2(0, 0)),
			ParseXmlAttribute(*i, "tileSize", 1.f)
		);

		XmlElement* spawnInfos = i->FirstChildElement("SpawnInfos");
		for (XmlElement* j = spawnInfos->FirstChildElement(); j !=nullptr ; j = j->NextSiblingElement())
		{
			s_definitions[ParseXmlAttribute(*i, "name", "undefineMap")].m_spawnInfos.push_back(
				SpawnInfo{
					ParseXmlAttribute(*j, "actor", "undefineActor"),
					ParseXmlAttribute(*j, "position", Vec3(0.f, 0.f, 0.f)),
					ParseXmlAttribute(*j, "orientation", EulerAngles(0.f, 0.f, 0.f)),
					ParseXmlAttribute(*j, "scale", Vec3(1.f,1.f,1.f))
				}
			);
		}
	}
}