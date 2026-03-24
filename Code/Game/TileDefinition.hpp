#pragma once
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include <string>
#include <map>

//-----------------------------------------------------------------------------------------------
class TileDefinition {
public:
	TileDefinition() = default;
	~TileDefinition() = default;
	explicit TileDefinition(
		std::string name, 
		bool isSolid, 
		Rgba8 mapImagePixelColor, 
		AABB2 wallUV, 
		AABB2 floorUV,
		AABB2 ceilingUV,
		AABB2 topUV,
		AABB2 bottomUV,
		Rgba8 tintColor = Rgba8::WHITE
	);
	static void InitializeTileDefs();

public:
	static std::map<std::string, TileDefinition> s_definitions;
	std::string m_name = "undefineTile";
	bool        m_isSolid = false;
	Rgba8       m_mapImagePixelColor = Rgba8(0, 0, 0, 0);
	AABB2       m_wallUV    = AABB2::ZERO_TO_ONE;
	AABB2       m_floorUV   = AABB2::ZERO_TO_ONE;
	AABB2       m_ceilingUV = AABB2::ZERO_TO_ONE;
	AABB2       m_topUV     = AABB2::ZERO_TO_ONE;
	AABB2       m_bottomUV  = AABB2::ZERO_TO_ONE;
	Rgba8       m_tintColor = Rgba8(255, 255, 255, 255);
};