#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Core/Image.hpp"
#include <string>
#include <map>
#include <vector>

//-----------------------------------------------------------------------------------------------
class Image;
class Shader;
class Texture;

struct SpawnInfo {
	std::string m_actorType = "undefinedActor";
	Vec3 m_spawnPosition = Vec3(0.f, 0.f, 0.f);
	EulerAngles m_spawnOrientation = EulerAngles(0.f, 0.f, 0.f);
};

//-----------------------------------------------------------------------------------------------
class MapDefinition {
public:
	MapDefinition() = default;
	~MapDefinition();
	explicit MapDefinition(
		std::string name,
		Image mapImage,
		Shader* mapShader,
		Texture* mapTexture,
		Texture* mapNormalTexture,
		Texture* mapAOTexture,
		Texture* mapRoughnessTexture,
		Texture* mapMetallicTexture,
		IntVec2 mapTextureDimentions,
		float tileSize
	);
	static void InitializeMapDefs();

public:
	static std::map<std::string, MapDefinition> s_definitions;
	std::string            m_name = "undefineMap";
	Image                  m_mapImage;
	Shader*                m_mapShader = nullptr;
	Texture*               m_mapTexture = nullptr;
	Texture*			   m_mapNormalTexture = nullptr;
	Texture*               m_mapAOTexture = nullptr;
	Texture*               m_mapRoughnessTexture = nullptr;
	Texture*			   m_mapMetallicTexture = nullptr;
	IntVec2                m_mapTextureDimentions = IntVec2(0, 0);
	float                  m_tileSize = 1.f;
	std::vector<SpawnInfo> m_spawnInfos;
};