#pragma once
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include <string>
#include <map>
#include <vector>

//-----------------------------------------------------------------------------------------------
class Shader;
class Texture;
class SpriteSheet;
class SpriteAnimDefinition;

//-----------------------------------------------------------------------------------------------
struct WeaponHUD {
	Shader*  m_shader                     = nullptr;
	Texture* m_baseTexture                = nullptr;
	Texture* m_reticleTexture             = nullptr;
	IntVec2  m_reticleSize                = IntVec2(0, 0);
	SpriteSheet* m_spriteSheet            = nullptr;
	Texture* m_spriteSheetNormalTexture   = nullptr;
	Texture* m_spriteSheetEmissiveTexture = nullptr;
	IntVec2  m_spriteSize                 = IntVec2(256, 256);
	Vec2     m_spritePivot                = Vec2(0.5f, 0.0f);
	std::map<std::string, SpriteAnimDefinition*> m_animations;
};

//-----------------------------------------------------------------------------------------------
class WeaponDefinition {
public:
	WeaponDefinition() = default;
	~WeaponDefinition() = default;
	explicit WeaponDefinition(
		std::string name,
		float refireTime,
		unsigned int rayCount,
		float rayCone,
		float rayRange,
		FloatRange rayDamage,
		float rayImpulse,
		unsigned int projectileCount,
		std::string projectileActor,
		float projectileCone,
		float projectileSpeed,
		unsigned int meleeCount,
		float meleeArc,
		float meleeRange,
		FloatRange meleeDamage,
		float meleeImpulse
	);
	static void InitializeWeaponDefs(std::string configPath);

public:
	static std::map<std::string, WeaponDefinition> s_definitions;
	std::string                       m_name = "undefinedWeapon";
	float                             m_refireTime = 0.f;

	//for raycast weapon	          
	unsigned int                      m_rayCount = 0;
	float                             m_rayCone = 0.f;
	float                             m_rayRange = 0.f;
	FloatRange                        m_rayDamage = FloatRange(0.f, 0.f);
	float                             m_rayImpulse = 0.f;

	//for projectile weapon	          
	unsigned int                      m_projectileCount = 0;
	std::string                       m_projectileActor = "undefinedProjectile";
	float                             m_projectileCone = 0.f;
	float                             m_projectileSpeed = 0.f;

	//for melee 			          
	unsigned int                      m_meleeCount = 0;
	float                             m_meleeArc = 0.f;
	float                             m_meleeRange = 0.f;
	FloatRange                        m_meleeDamage = FloatRange(0.f, 0.f);
	float                             m_meleeImpulse = 0.f;

	WeaponHUD                         m_hud;
	std::map<std::string, SoundID>    m_sounds;
};