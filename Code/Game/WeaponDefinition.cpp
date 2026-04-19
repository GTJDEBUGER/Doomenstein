#include "Game/WeaponDefinition.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"

//-----------------------------------------------------------------------------------------------
std::map<std::string, WeaponDefinition> WeaponDefinition::s_definitions;

//-----------------------------------------------------------------------------------------------
WeaponDefinition::WeaponDefinition(
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
) : 
	m_name(name),
	m_refireTime(refireTime),
	m_rayCount(rayCount),
	m_rayCone(rayCone),
	m_rayRange(rayRange),
	m_rayDamage(rayDamage),
	m_rayImpulse(rayImpulse),
	m_projectileCount(projectileCount),
	m_projectileActor(projectileActor),
	m_projectileCone(projectileCone),
	m_projectileSpeed(projectileSpeed),
	m_meleeCount(meleeCount),
	m_meleeArc(meleeArc),
	m_meleeRange(meleeRange),
	m_meleeDamage(meleeDamage),
	m_meleeImpulse(meleeImpulse)
{
}

//-----------------------------------------------------------------------------------------------
void WeaponDefinition::InitializeWeaponDefs(std::string configPath) {
	XmlDocument doc;
	doc.LoadFile(configPath.data());
	XmlElement* rootElement = doc.FirstChildElement();
	for (XmlElement* i = rootElement->FirstChildElement(); i != nullptr; i = i->NextSiblingElement()) {
		s_definitions[ParseXmlAttribute(*i, "name", "undefineWeapon")] = WeaponDefinition(
			ParseXmlAttribute(*i, "name", "undefineWeapon"),
			ParseXmlAttribute(*i, "refireTime", 0.f),
			ParseXmlAttribute(*i, "rayCount", 0),
			ParseXmlAttribute(*i, "rayCone", 0.f),
			ParseXmlAttribute(*i, "rayRange", 0.f),
			ParseXmlAttribute(*i, "rayDamage", FloatRange(0.f, 0.f)),
			ParseXmlAttribute(*i, "rayImpulse", 0.f),
			ParseXmlAttribute(*i, "projectileCount", 0),
			ParseXmlAttribute(*i, "projectileActor", "undefinedActor"),
			ParseXmlAttribute(*i, "projectileCone", 0.f),
			ParseXmlAttribute(*i, "projectileSpeed", 0.f),
			ParseXmlAttribute(*i, "meleeCount", 0),
			ParseXmlAttribute(*i, "meleeArc", 0.f),
			ParseXmlAttribute(*i, "meleeRange", 0.f),
			ParseXmlAttribute(*i, "meleeDamage", FloatRange(0.f, 0.f)),
			ParseXmlAttribute(*i, "meleeImpulse", 0.f)
		);

		XmlElement* weaponHUD = i->FirstChildElement("HUD");
		if (weaponHUD != nullptr) {
			s_definitions[ParseXmlAttribute(*i, "name", "undefineWeapon")].m_hud = WeaponHUD{
				g_engine->m_renderer->CreateShader(ParseXmlAttribute(*weaponHUD, "shader", "Error").data(), VertexType::PCUTBN),
				g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*weaponHUD, "baseTexture", "Error").data()),
				g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*weaponHUD, "reticleTexture", "Error").data()),
				ParseXmlAttribute(*weaponHUD, "reticleSize", IntVec2(0, 0)),
				ParseXmlAttribute(*weaponHUD, "spriteSize", IntVec2(256, 256)),
				ParseXmlAttribute(*weaponHUD, "spritePivot", Vec2(0.5f, 0.f))
			};
			for (XmlElement* j = weaponHUD->FirstChildElement("Animation"); j != nullptr; j = j->NextSiblingElement()) {
				std::string animationName = ParseXmlAttribute(*j, "name", "undefinedAnimation");
				IntVec2 cellCount = ParseXmlAttribute(*j, "cellCount", IntVec2(0, 0));
				float secondsPerFrame = ParseXmlAttribute(*j, "secondsPerFrame", 0.f);
				int startFrame = ParseXmlAttribute(*j, "startFrame", 0);
				int endFrame = ParseXmlAttribute(*j, "endFrame", 0);
				s_definitions[ParseXmlAttribute(*i, "name", "undefineWeapon")].m_hud.m_animations.push_back(
					WeaponAnimation{
						animationName,
						g_engine->m_renderer->CreateShader(ParseXmlAttribute(*j, "shader", "Error").data(), VertexType::PCUTBN),
						new SpriteSheet(
							*(g_engine->m_renderer->CreateOrGetTextureFromFile(ParseXmlAttribute(*j, "spriteSheet", "Error").data())),
							cellCount
						),
						cellCount,
						secondsPerFrame,
						startFrame,
						endFrame
					}
				);
			}
		}

		XmlElement* weaponSounds = i->FirstChildElement("Sounds");
		if (weaponSounds != nullptr) {
			for (XmlElement* j = weaponSounds->FirstChildElement("Sound"); j != nullptr; j = j->NextSiblingElement()) {
				std::string soundName = ParseXmlAttribute(*j, "name", "undefinedSound");
				s_definitions[ParseXmlAttribute(*i, "name", "undefineWeapon")].m_sounds.push_back(
					WeaponSound{
						soundName,
						g_engine->m_audio->CreateOrGetSound(ParseXmlAttribute(*j, "filePath", "Error").data())
					}
				);
			}
		}
	}
}