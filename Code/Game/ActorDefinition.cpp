#include "Game/ActorDefinition.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

//-----------------------------------------------------------------------------------------------
std::map<std::string, ActorDefinition> ActorDefinition::s_definitions;

//-----------------------------------------------------------------------------------------------
Actor2DAnimationGroup::~Actor2DAnimationGroup() {
	for (auto& pair : m_animations) {
		if (pair.second != nullptr) {
			delete pair.second;
			pair.second = nullptr;
		}
	}
	m_animations.clear();
}

//-----------------------------------------------------------------------------------------------
Actor2DRenderInfo::~Actor2DRenderInfo() {
	for (auto& pair : m_animationGroups) {
		if (pair.second != nullptr) {
			delete pair.second;
			pair.second = nullptr;
		}
	}
	m_animationGroups.clear();
}

//-----------------------------------------------------------------------------------------------
ActorDefinition::ActorDefinition(
	std::string name,
	std::string faction,
	float health,
	bool canBePossessed,
	float corpseLifetime,
	bool visible,
	bool dieOnSpawn
) : 
	m_name(name),
	m_faction(faction),
	m_health(health),
	m_canBePossessed(canBePossessed),
	m_corpseLifetime(corpseLifetime),
	m_visible(visible),
	m_dieOnSpawn(dieOnSpawn) {
}

//-----------------------------------------------------------------------------------------------
ActorDefinition::~ActorDefinition() {
}

//-----------------------------------------------------------------------------------------------
void ActorDefinition::InitializeActorDefs(std::string configPath) {
	XmlDocument doc;
	doc.LoadFile(configPath.data());
	XmlElement* rootElement = doc.FirstChildElement();
	for (XmlElement* i = rootElement->FirstChildElement(); i != nullptr; i = i->NextSiblingElement()) {
		std::string actorName = ParseXmlAttribute(*i, "name", "undefinedActor");
		s_definitions[actorName] = ActorDefinition(
			ParseXmlAttribute(*i, "name", "undefinedActor"),
			ParseXmlAttribute(*i, "faction", "undefinedFaction"),
			ParseXmlAttribute(*i, "health", 0.f),
			ParseXmlAttribute(*i, "canBePossessed", true),
			ParseXmlAttribute(*i, "corpseLifetime", 0.f),
			ParseXmlAttribute(*i, "visible", true),
			ParseXmlAttribute(*i, "dieOnSpawn", false)
		);

		XmlElement* actorCollision = i->FirstChildElement("Collision");
		if (actorCollision != nullptr) {
			s_definitions[actorName].m_collision = ActorCollision{
				ParseXmlAttribute(*actorCollision, "radius", 0.f),
				ParseXmlAttribute(*actorCollision, "height", 0.f),
				ParseXmlAttribute(*actorCollision, "collisionWithWorld", true),
				ParseXmlAttribute(*actorCollision, "collisionWithActors", true),
				ParseXmlAttribute(*actorCollision, "damageOnCollide", FloatRange(0.f,0.f)),
				ParseXmlAttribute(*actorCollision, "impulseOnCollide", 0.f),
				ParseXmlAttribute(*actorCollision, "dieOnCollide", false)
			};
		}

		XmlElement* actorPhysics = i->FirstChildElement("Physics");
		if (actorPhysics != nullptr) {
			s_definitions[actorName].m_physics = ActorPhysics{
				ParseXmlAttribute(*actorPhysics, "isSimulated", true),
				ParseXmlAttribute(*actorPhysics, "walkSpeed", 0.f),
				ParseXmlAttribute(*actorPhysics, "runSpeed", 0.f),
				ParseXmlAttribute(*actorPhysics, "turnSpeed", 0.f),
				ParseXmlAttribute(*actorPhysics, "flying", false),
				ParseXmlAttribute(*actorPhysics, "drag", 0.f),
				ParseXmlAttribute(*actorPhysics, "mass", -1.f)
			};
		}

		XmlElement* actorCamera = i->FirstChildElement("Camera");
		if (actorCamera != nullptr) {
			s_definitions[actorName].m_actorCamera = ActorCamera{
				ParseXmlAttribute(*actorCamera, "eyeHeight", 0.f),
				ParseXmlAttribute(*actorCamera, "cameraFOV", 0.f)
			};
		}

		XmlElement* actorAI = i->FirstChildElement("AI");
		if (actorAI != nullptr) {
			s_definitions[actorName].m_actorAI = ActorAI{
				ParseXmlAttribute(*actorAI, "aiEnabled", false),
				ParseXmlAttribute(*actorAI, "sightRadius", 0.f),
				ParseXmlAttribute(*actorAI, "sightAngle", 0.f)
			};
		}

		XmlElement* actor2DRenderInfo = i->FirstChildElement("Visuals");
		if (actor2DRenderInfo != nullptr) {
			std::string actorRenderBillboardTypeName = ParseXmlAttribute(*actor2DRenderInfo, "billboardType", "undefinedBillboardType");
			BillboardType actorRenderBillboardType = BillboardType::NONE;
			if (actorRenderBillboardTypeName == "FullFacing") {
				actorRenderBillboardType = BillboardType::FULL_FACING;
			}
			else if (actorRenderBillboardTypeName == "FullOpposing") {
				actorRenderBillboardType = BillboardType::FULL_OPPOSING;
			}
			else if (actorRenderBillboardTypeName == "WorldUpFacing") {
				actorRenderBillboardType = BillboardType::WORLD_UP_FACING;
			}
			else if (actorRenderBillboardTypeName == "WorldUpOpposing") {
				actorRenderBillboardType = BillboardType::WORLD_UP_OPPOSING;
			}

			bool renderLit = ParseXmlAttribute(*actor2DRenderInfo, "renderLit", true);
			std::string spriteSheetPath = ParseXmlAttribute(*actor2DRenderInfo, "spriteSheet", "Error");
			Texture* spriteSheetTexture = spriteSheetPath != "Error" ? g_engine->m_renderer->CreateOrGetTextureFromFile(spriteSheetPath.data()) : nullptr;
			std::string spriteSheetNormalPath = ParseXmlAttribute(*actor2DRenderInfo, "spriteSheetNormalTexture", "Error");
			Texture* spriteSheetNormalTexture = spriteSheetNormalPath != "Error" ? g_engine->m_renderer->CreateOrGetTextureFromFile(spriteSheetNormalPath.data()) : nullptr;
			std::string spriteSheetAOPath = ParseXmlAttribute(*actor2DRenderInfo, "spriteSheetAOTexture", "Error");
			Texture* spriteSheetAOTexture = spriteSheetAOPath != "Error" ? g_engine->m_renderer->CreateOrGetTextureFromFile(spriteSheetAOPath.data()) : nullptr;
			std::string spriteSheetRoughnessPath = ParseXmlAttribute(*actor2DRenderInfo, "spriteSheetRoughnessTexture", "Error");
			Texture* spriteSheetRoughnessTexture = spriteSheetRoughnessPath != "Error" ? g_engine->m_renderer->CreateOrGetTextureFromFile(spriteSheetRoughnessPath.data()) : nullptr;
			std::string spriteSheetMetallicPath = ParseXmlAttribute(*actor2DRenderInfo, "spriteSheetMetallicTexture", "Error");
			Texture* spriteSheetMetallicTexture = spriteSheetMetallicPath != "Error" ? g_engine->m_renderer->CreateOrGetTextureFromFile(spriteSheetMetallicPath.data()) : nullptr;
			std::string spriteSheetEmissivePath = ParseXmlAttribute(*actor2DRenderInfo, "spriteSheetEmissiveTexture", "Error");
			Texture* spriteSheetEmissiveTexture = spriteSheetEmissivePath != "Error" ? g_engine->m_renderer->CreateOrGetTextureFromFile(spriteSheetEmissivePath.data()) : nullptr;
			IntVec2 spriteSheetCellCount = ParseXmlAttribute(*actor2DRenderInfo, "cellCount", IntVec2(1, 1));
			s_definitions[actorName].m_actor2DRenderInfo = Actor2DRenderInfo{
				ParseXmlAttribute(*actor2DRenderInfo, "size", Vec2(1.f,1.f)),
				ParseXmlAttribute(*actor2DRenderInfo, "pivot", Vec2(0.5f, 0.f)),
				actorRenderBillboardType,
				renderLit,
				ParseXmlAttribute(*actor2DRenderInfo, "renderRounded", true),
				g_engine->m_renderer->CreateShader(ParseXmlAttribute(*actor2DRenderInfo, "shader", "Error").data(), renderLit ? VertexType::PCUTBN : VertexType::PCU),
				spriteSheetTexture,
				spriteSheetNormalTexture,
				spriteSheetAOTexture,
				spriteSheetRoughnessTexture,
				spriteSheetMetallicTexture,
				spriteSheetEmissiveTexture,
				new SpriteSheet(*spriteSheetTexture, spriteSheetCellCount)
			};

			for (XmlElement* j = actor2DRenderInfo->FirstChildElement(); j != nullptr; j = j->NextSiblingElement()) {
				std::string animationGroupName = ParseXmlAttribute(*j, "name", "undefinedAnimationGroup");
				bool scaleBySpeed = ParseXmlAttribute(*j, "scaleBySpeed", false);
				float secondsPerFrame = ParseXmlAttribute(*j, "secondsPerFrame", 0.25f);
				SpriteAnimPlaybackType playbackType = SpriteAnimPlaybackType::LOOP;
				std::string playbackTypeName = ParseXmlAttribute(*j, "playbackMode", "undefinedPlaybackType");
				if (playbackTypeName == "Loop") {
					playbackType = SpriteAnimPlaybackType::LOOP;
				}
				else if (playbackTypeName == "Once") {
					playbackType = SpriteAnimPlaybackType::ONCE;
				}
				else if (playbackTypeName == "PingPong") {
					playbackType = SpriteAnimPlaybackType::PINGPONG;
				}
				else {
					ERROR_AND_DIE(Stringf("Unknown playback type \"%s\" for animation group \"%s\" of actor \"%s\"", playbackTypeName.c_str(), animationGroupName.c_str(), actorName.c_str()));
				}

				s_definitions[actorName].m_actor2DRenderInfo.m_animationGroups[animationGroupName] = 
					new Actor2DAnimationGroup{
						animationGroupName,
						scaleBySpeed
					};

				for (XmlElement* k = j->FirstChildElement(); k != nullptr; k = k->NextSiblingElement()) {
					Vec3 animDirection = ParseXmlAttribute(*k, "vector", Vec3(0.f, 0.f, 0.f));
					int directionCount = RoundDownToInt((animDirection.GetOrientationAboutZDegrees() + 180.f) / 45.f) % 8;
					XmlElement* animation = k->FirstChildElement("Animation");
					int startFrame = ParseXmlAttribute(*animation, "startFrame", 0);
					int endFrame = ParseXmlAttribute(*animation, "endFrame", 0);
					s_definitions[actorName].m_actor2DRenderInfo.m_animationGroups[animationGroupName]->m_animations[directionCount] =
						new SpriteAnimDefinition(
							*s_definitions[actorName].m_actor2DRenderInfo.m_spriteSheet,
							startFrame,
							endFrame,
							1.f / secondsPerFrame,
							playbackType
						);
				}
			}
		}
	
		XmlElement* actorSounds = i->FirstChildElement("Sounds");
		if (actorSounds != nullptr) {
			for (XmlElement* j = actorSounds->FirstChildElement(); j != nullptr; j = j->NextSiblingElement()) {
				std::string soundName = ParseXmlAttribute(*j, "sound", "undefinedSound");
				std::string soundFilePath = ParseXmlAttribute(*j, "name", "undefinedFilePath");
				s_definitions[actorName].m_sounds.push_back(
					ActorSound{
						soundName,
						g_engine->m_audio->CreateOrGetSound(soundFilePath)
					}
				);
			}
		}

		XmlElement* actorInventory = i->FirstChildElement("Inventory");
		if (actorInventory != nullptr) {
			for (XmlElement* j = actorInventory->FirstChildElement(); j != nullptr; j = j->NextSiblingElement()) {
				std::string weaponName = ParseXmlAttribute(*j, "name", "undefinedWeapon");
				s_definitions[actorName].m_inventory.push_back(weaponName);
			}
		}

		XmlElement* actorPointLight = i->FirstChildElement("PointLight");
		if (actorPointLight != nullptr) {
			s_definitions[actorName].m_pointLight = ActorPointLight{
				ParseXmlAttribute(*actorPointLight, "radius", 0.f),
				ParseXmlAttribute(*actorPointLight, "color", Rgba8::WHITE),
				ParseXmlAttribute(*actorPointLight, "intensity", 0.f),
				ParseXmlAttribute(*actorPointLight, "volumetric", false)
			};
		}
	}
}