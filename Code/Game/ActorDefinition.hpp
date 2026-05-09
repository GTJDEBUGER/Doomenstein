#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <string>
#include <vector>
#include <map>

//-----------------------------------------------------------------------------------------------
class Shader;

//-----------------------------------------------------------------------------------------------
struct ActorCollision {
	float m_radius             = 0.f;
	float m_height             = 0.f;
	bool m_collisionWithWorld  = false;
	bool m_collisionWithActors = false;

	FloatRange m_damageOnCollide = FloatRange(0.f, 0.f);
	float m_impulseOnCollide = 0.f;
	bool m_dieOnCollide = false;
};

struct ActorPhysics
{
	bool m_isSimulated = false;
	float m_walkSpeed = 0.f;
	float m_runSpeed = 0.f;
	float m_turnSpeed = 0.f;
	bool m_flying = false;
	float m_drag = 0.f;
	float m_mass = -1.f;
};

struct ActorCamera
{
	float m_eyeHeight = 0.f;
	float m_cameraFOV = 0.f;
};

struct ActorAI
{
	bool m_aiEnabled = false;
	float m_sightRadius = 0.f;
	float m_sightAngle = 0.f;
	bool m_isComplex = false;

	std::vector<std::string> m_complexSubActors;
};

struct Actor2DAnimationGroup
{
public:
	std::string m_name                    = "undefinedAnimationGroup";
	bool m_scaleBySpeed                   = false;

	std::map<int, SpriteAnimDefinition*> m_animations;

public:
	Actor2DAnimationGroup() = default;
	~Actor2DAnimationGroup();
};

struct Actor2DRenderInfo
{
public:
	Vec2 m_size                            = Vec2(1.f, 1.f);
	Vec2 m_pivot                           = Vec2(0.5f, 0.5f);
	BillboardType m_billboardType          = BillboardType::WORLD_UP_FACING;
	bool m_renderLit                       = true;
	bool m_renderRounded                   = true;
	Shader* m_shader                       = nullptr;
	Texture* m_spriteSheetTexture          = nullptr;
	Texture* m_spriteSheetNormalTexture    = nullptr;
	Texture* m_spriteSheetAOTexture        = nullptr;
	Texture* m_spriteSheetRoughnessTexture = nullptr;
	Texture* m_spriteSheetMetallicTexture = nullptr;
	Texture* m_spriteSheetEmissiveTexture  = nullptr;
	SpriteSheet* m_spriteSheet             = nullptr;

	std::map<std::string, Actor2DAnimationGroup*> m_animationGroups;

public:
	Actor2DRenderInfo() = default;
	~Actor2DRenderInfo();
};

struct ActorPointLight
{
	float m_radius = 0.f;
	Rgba8 m_color = Rgba8(255,255,255);
	float m_intensity = 0.f;
	bool  m_volumetric = false;
};

//-----------------------------------------------------------------------------------------------
class ActorDefinition
{
public:
	ActorDefinition() = default;
	~ActorDefinition();
	explicit ActorDefinition(
		std::string name,
		std::string faction,
		float health,
		bool canBePossessed,
		float corpseLifetime,
		bool visible,
		bool isBoss,
		bool dieOnSpawn,
		float lifetime
	);
	static void InitializeActorDefs(std::string configPath);

public:
	static std::map<std::string, ActorDefinition> s_definitions;

	std::string m_name = "undefinedActor";
	std::string m_faction = "undefinedFaction";
	float m_health = 0.f;
	bool m_canBePossessed = false;
	float m_corpseLifetime = 99.f;
	bool m_visible = true;
	bool m_isBoss = false;
	bool m_dieOnSpawn = false;
	float m_lifetime = -1.f;
	ActorCollision m_collision;
	ActorPhysics m_physics;
	ActorCamera m_actorCamera;
	ActorAI m_actorAI;
	Actor2DRenderInfo m_actor2DRenderInfo;
	std::map<std::string, SoundID> m_sounds;
	std::vector<std::string> m_inventory;
	ActorPointLight m_pointLight;
};