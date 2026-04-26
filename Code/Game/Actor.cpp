#include "Game/Actor.hpp"
#include "Game/ActorHandle.hpp"
#include "Game/Map.hpp"
#include "Game/Controller.hpp"
#include "Game/PlayerController.hpp"
#include "Game/AIController.hpp"
#include "Game/Weapon.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/MathUtils.hpp"

//---------------------------------------------------------------------------------------------------
Actor::Actor(ActorDefinition const& definition, Map* map, ActorHandle* handle, Vec3 pos, EulerAngles orien, Vec3 scale) :
	m_definition(definition),
	m_map(map),
	m_handle(handle),
	m_position(pos),
	m_orientation(orien),
	m_scale(scale){

	if (m_definition.m_faction._Equal("Marine")) {
		m_debugColor = Rgba8::GREEN;
	}
	else if (m_definition.m_faction._Equal("Demon")) {
		m_debugColor = Rgba8::RED;
	}
	m_debugColor.a = 100;

	m_curEyeHeight = m_definition.m_actorCamera.m_eyeHeight;
	m_curHealth = m_definition.m_health;

	for(unsigned int i = 0; i < m_definition.m_inventory.size(); i++) {
		std::string weaponName = m_definition.m_inventory[i];
		if (WeaponDefinition::s_definitions.find(weaponName) != WeaponDefinition::s_definitions.end()) {
			m_inventory.push_back(new Weapon(WeaponDefinition::s_definitions.at(weaponName), this));
		}
	}
	if(!m_inventory.empty()) EquipWeapon(0);

	if (m_definition.m_actorAI.m_aiEnabled) {
		m_controller = new AIController(m_map);
		m_controller->Possess(m_handle);
	}

	if (m_definition.m_dieOnSpawn) {
		m_isDead = true;
		m_debugColor = m_debugDeadColor;
		m_deadTimer = m_definition.m_corpseLifetime;
		m_curAnimName = "Death";
		m_animTimer = 0.f;
	}

	// Debug geometry
	AddVertexForArrow3D(
		m_debugVerts,
		Vec3(0.f, 0.f, m_definition.m_collision.m_height * 0.5f),
		Vec3(0.5f, 0.f, m_definition.m_collision.m_height * 0.5f),
		0.05f,
		Rgba8::RED,
		16
	);

	AddVertexForArrow3D(
		m_debugVerts,
		Vec3(0.f, 0.f, m_definition.m_collision.m_height * 0.5f),
		Vec3(0.f, 0.5f, m_definition.m_collision.m_height * 0.5f),
		0.05f,
		Rgba8::GREEN,
		16
	);

	AddVertexForArrow3D(
		m_debugVerts,
		Vec3(0.f, 0.f, m_definition.m_collision.m_height * 0.5f),
		Vec3(0.f, 0.f, m_definition.m_collision.m_height * 0.5f) + Vec3(0.f, 0.f, 0.5f),
		0.05f,
		Rgba8::BLUE,
		16
	);

	AddVertexForSphere3D(
		m_debugVerts,
		Vec3(0.f, 0.f, m_definition.m_collision.m_height * 0.5f),
		0.06f,
		Rgba8::WHITE,
		AABB2::ZERO_TO_ONE,
		16,
		8
	);

	AddVertexForCylinder3D(
		m_debugVerts,
		Vec3(0.f, 0.f, 0.f),
		Vec3(0.f, 0.f, m_definition.m_collision.m_height),
		m_definition.m_collision.m_radius,
		m_debugColor,
		AABB2::ZERO_TO_ONE,
		16
	);

	if (m_definition.m_actorCamera.m_cameraFOV > 0.f) {
		AddVertexForCone3D(
			m_debugVerts,
			Vec3(m_definition.m_collision.m_radius, 0.f, m_definition.m_actorCamera.m_eyeHeight),
			Vec3(m_definition.m_collision.m_radius + 0.4f, 0.f, m_definition.m_actorCamera.m_eyeHeight),
			0.25f,
			m_debugColor,
			AABB2::ZERO_TO_ONE,
			8
		);
	}

	AddVertexForWireframeCylinder3D(
		m_debugVerts,
		Vec3(0.f, 0.f, 0.f),
		Vec3(0.f, 0.f, m_definition.m_collision.m_height),
		m_definition.m_collision.m_radius,
		0.02f,
		Rgba8(
			(unsigned int)m_debugColor.r,
			(unsigned int)m_debugColor.g,
			(unsigned int)m_debugColor.b
		),
		AABB2::ZERO_TO_ONE,
		16
	);

	if (m_definition.m_actorCamera.m_cameraFOV > 0.f) {
		AddVertexForWireframeCone3D(
			m_debugVerts,
			Vec3(m_definition.m_collision.m_radius, 0.f, m_definition.m_actorCamera.m_eyeHeight),
			Vec3(m_definition.m_collision.m_radius + 0.4f, 0.f, m_definition.m_actorCamera.m_eyeHeight),
			0.25f,
			0.02f,
			Rgba8(
				(unsigned int)m_debugColor.r,
				(unsigned int)m_debugColor.g,
				(unsigned int)m_debugColor.b
			),
			AABB2::ZERO_TO_ONE,
			8
		);
	}

	// Actor geometry
	Vec2 spriteSheetSize = m_definition.m_actor2DRenderInfo.m_size;
	if (!m_definition.m_actor2DRenderInfo.m_renderLit) {
		AddVertexForQuad3D(
			m_actorUnlitVerts,
			Vec3(0.f, spriteSheetSize.x * (m_definition.m_actor2DRenderInfo.m_pivot.x - 1.f), spriteSheetSize.y * -m_definition.m_actor2DRenderInfo.m_pivot.y),
			Vec3(0.f, spriteSheetSize.x * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.x), spriteSheetSize.y * -m_definition.m_actor2DRenderInfo.m_pivot.y),
			Vec3(0.f, spriteSheetSize.x * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.x), spriteSheetSize.y * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.y)),
			Vec3(0.f, spriteSheetSize.x * (m_definition.m_actor2DRenderInfo.m_pivot.x - 1.f), spriteSheetSize.y * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.y))
		);
	}
	else {
		AddVertexForQuad3D(
			m_actorLitVerts,
			Vec3(0.f, spriteSheetSize.x * (m_definition.m_actor2DRenderInfo.m_pivot.x - 1.f), spriteSheetSize.y * -m_definition.m_actor2DRenderInfo.m_pivot.y),
			Vec3(0.f, spriteSheetSize.x * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.x), spriteSheetSize.y * -m_definition.m_actor2DRenderInfo.m_pivot.y),
			Vec3(0.f, spriteSheetSize.x * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.x), spriteSheetSize.y * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.y)),
			Vec3(0.f, spriteSheetSize.x * (m_definition.m_actor2DRenderInfo.m_pivot.x - 1.f), spriteSheetSize.y * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.y))
		);

		AddVertexForQuad3D(
			m_actorShadowmapVerts,
			Vec3(0.f, spriteSheetSize.x * (m_definition.m_actor2DRenderInfo.m_pivot.x - 1.f), spriteSheetSize.y * -m_definition.m_actor2DRenderInfo.m_pivot.y),
			Vec3(0.f, spriteSheetSize.x * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.x), spriteSheetSize.y * -m_definition.m_actor2DRenderInfo.m_pivot.y),
			Vec3(0.f, spriteSheetSize.x * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.x), spriteSheetSize.y * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.y)),
			Vec3(0.f, spriteSheetSize.x * (m_definition.m_actor2DRenderInfo.m_pivot.x - 1.f), spriteSheetSize.y * (1.f - m_definition.m_actor2DRenderInfo.m_pivot.y))
		);
	}
}

//---------------------------------------------------------------------------------------------------
Actor::~Actor() {
	if (m_handle) {
		delete m_handle;
		m_handle = nullptr;
	}

	if (m_controller != nullptr && dynamic_cast<AIController*>(m_controller)) {
		delete m_controller;
		m_controller = nullptr;
	}

	for (Weapon* weapon : m_inventory) {
		delete weapon;
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::Update(float deltaSeconds) {
	// Handle actor death
	if (m_isDead) {
		if (m_needDestroy) {
			return;
		}

		m_deadTimer -= deltaSeconds;
		if (dynamic_cast<PlayerController*>(m_controller)) {
			m_curEyeHeight = Interpolate(
				0.1f,
				m_definition.m_actorCamera.m_eyeHeight,
				GetClamped(m_deadTimer / m_definition.m_corpseLifetime * 0.5f, 0.f, 1.f)
			);
		}
		if (m_definition.m_actor2DRenderInfo.m_animationGroups.size() > 0) {
			m_animTimer += deltaSeconds;
			UpdateActorAnimation(m_curAnimName, m_animTimer);
		}

		if (m_deadTimer <= 0.f) {
			m_needDestroy = true;
		}
		return;
	}

	//Handle AI controlled behavior
	if (m_controller != nullptr && dynamic_cast<AIController*>(m_controller)) {
		dynamic_cast<AIController*>(m_controller)->Update();
	}

	// Update physics if simulated, otherwise reset velocity and acceleration
	if (m_definition.m_physics.m_isSimulated) {
		UpdatePhysics(deltaSeconds);
	}
	else {
		m_velocity = Vec3(0.f,0.f,0.f);
		m_acceleration = Vec3(0.f, 0.f, 0.f);
	}

	// Update animations
	if (m_definition.m_actor2DRenderInfo.m_animationGroups.size() > 0) {
		// Determine nearest 8 directions to player camera
		int nearestEightDirCount = GetQuantizedDirectionAnimationIndex(*m_map->m_game->m_playerController->m_playerCamera);

		// Determine nearest 8 directions to shadow map camera
		int shadowmapNearestEightDirCount = GetQuantizedDirectionAnimationIndex(*m_map->m_sunShadowCamera);

		// Determine current animation based on velocity and action state
		if (m_velocity.GetXY().GetLengthSquared() > 0.01f && 
			m_curAnimName != "Attack" && 
			m_curAnimName != "Hurt" &&
			m_curAnimName != "Death" &&
			m_curAnimName != "Walk"
		) {
			m_curAnimName = "Walk";
			m_animTimer = 0.f;
		}
		else if (m_curAnimName != "Idle" && (
				  (m_curAnimName == "Attack" && m_animTimer > m_definition.m_actor2DRenderInfo.m_animationGroups.at("Attack")->m_animations[nearestEightDirCount]->GetDuration()) ||
				  (m_curAnimName == "Hurt" && m_animTimer > m_definition.m_actor2DRenderInfo.m_animationGroups.at("Hurt")->m_animations[nearestEightDirCount]->GetDuration()) ||
                  (m_curAnimName == "Walk" && m_velocity.GetXY().GetLengthSquared() <= 0.01f)
				)
		) {
			m_curAnimName = "Idle";
		}

		// Update UVs based on current animation and direction
		if (m_curAnimName == "Idle") {
			m_animTimer = 0.f;
			UpdateActorAnimation("Walk", m_animTimer, nearestEightDirCount, shadowmapNearestEightDirCount);
		}
		else{
			if (m_definition.m_actor2DRenderInfo.m_animationGroups.at(m_curAnimName)->m_scaleBySpeed) {
				m_animTimer += deltaSeconds;
				UpdateActorAnimation(m_curAnimName, m_animTimer, nearestEightDirCount, shadowmapNearestEightDirCount);
			}
			else {
				m_animTimer += deltaSeconds;
				UpdateActorAnimation(m_curAnimName, m_animTimer, nearestEightDirCount, shadowmapNearestEightDirCount);
			}
		}
	}

	// Update world UI
	UpdateWeapon(deltaSeconds);
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdatePhysics(float deltaSeconds) {
	// Apply gravity
	if (!m_definition.m_physics.m_flying) {
		m_velocity -= Vec3(0.f, 0.f, 49.f) * deltaSeconds;
	}

	// Apply acceleration
	m_velocity += m_acceleration * deltaSeconds;

	// Apply friction
	if (!m_definition.m_physics.m_flying) {
		float frictionDivisor = 1.0f + (m_definition.m_physics.m_drag * deltaSeconds);

		m_velocity.x /= frictionDivisor;
		m_velocity.y /= frictionDivisor;

		if (m_velocity.GetXY().GetLengthSquared() < 0.0001f) {
			m_velocity.x = 0.f;
			m_velocity.y = 0.f;
		}
	}

	// Update position
	m_position += m_velocity * deltaSeconds;

	// Reset acceleration
	m_acceleration = Vec3(0.f, 0.f, 0.f);
}

//---------------------------------------------------------------------------------------------------
void Actor::Damage(float damageAmount, Actor* attacker) {
	if (m_isDead) {
		return;
	}

	m_curHealth -= damageAmount;
	m_curAnimName = "Hurt";
	m_animTimer = 0.f;

	if(dynamic_cast<PlayerController*>(m_controller)) {
		m_map->m_game->AddCameraShake(0.75f);
	}

	if (m_curHealth <= 0.f) {
		m_isDead = true;
		m_deadTimer = m_definition.m_corpseLifetime;
		m_curAnimName = "Death";
		m_animTimer = 0.f;
		m_curHealth = 0.f;
	}

	if (attacker != nullptr && 
		m_definition.m_faction != attacker->m_definition.m_faction &&
		attacker->m_definition.m_faction != "Neutral" &&
		m_definition.m_actorAI.m_aiEnabled 
		) {
		dynamic_cast<AIController*>(m_controller)->DamagedBy(attacker);
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::AddForce(Vec3 const& force) {
	m_acceleration += force / m_definition.m_physics.m_mass;
}

//---------------------------------------------------------------------------------------------------
void Actor::AddImpulse(Vec3 const& impulse) {
	m_velocity += impulse / m_definition.m_physics.m_mass;
}

//---------------------------------------------------------------------------------------------------
void Actor::OnCollide(Actor* other) {
	if (m_isDead) {
		return;
	}

	if (other!=nullptr && m_definition.m_collision.m_damageOnCollide.GetLength() > 0.f) {
		float randomDamage = m_map->m_game->m_randomGenerator->RollRandomFloatInRange(m_definition.m_collision.m_damageOnCollide.m_min, m_definition.m_collision.m_damageOnCollide.m_max);
		if (m_owner == nullptr) {
			other->Damage(randomDamage, this);
		}
		else {
			other->Damage(randomDamage, m_owner);
		}

		m_owner->m_map->SpawnActor(
			SpawnInfo{
				"BloodSplatter",
				m_position + m_velocity.GetNormalized() * 0.05f
			}
		);
	}

	if (other!=nullptr && m_definition.m_collision.m_impulseOnCollide > 0.f) {
		Vec3 impulseDirection = (other->m_position - m_position).GetNormalized();
		other->AddImpulse(impulseDirection * m_definition.m_collision.m_impulseOnCollide);
	}

	if (m_definition.m_collision.m_dieOnCollide) {
		m_isDead = true;
		m_debugColor = m_debugDeadColor;
		m_deadTimer = m_definition.m_corpseLifetime;
		m_curAnimName = "Death";
		m_animTimer = 0.f;
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::OnPossessed() {
	PlayerController* playerController = dynamic_cast<PlayerController*>(m_controller);
	if (playerController != nullptr) {
		playerController->m_orientation = m_orientation;
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::OnUnpossessed() {
	if (m_controller != nullptr && dynamic_cast<AIController*>(m_controller)) {
		delete m_controller;
		m_controller = nullptr;
	} 
	else if (m_controller != nullptr && dynamic_cast<PlayerController*>(m_controller)) {
		m_controller = nullptr;
		if (m_definition.m_actorAI.m_aiEnabled) {
			m_controller = new AIController(m_map);
			m_controller->Possess(m_handle);
		}
	}
	else {
		m_controller = nullptr;
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::MoveInDirection(Vec3 const& direction, float targetSpeed) {
	Vec3 desiredVelocity = direction.GetNormalized() * targetSpeed;
	Vec3 steering = desiredVelocity - Vec3(m_velocity.x, m_velocity.y, 0.f);
	AddForce(steering);
}

//---------------------------------------------------------------------------------------------------
void Actor::TurnInDirection(Vec3 const& direction, float maxStepAngle) {
	Vec3 currentForward = m_orientation.GetForwardDir_IFwd_JLeft_KUp();
	Vec3 targetDirection = direction.GetNormalized();

	float angleToTarget = GetTurnedTowardDegrees(
		currentForward.GetOrientationAboutZDegrees(),
		targetDirection.GetOrientationAboutZDegrees(),
		maxStepAngle
	);
	m_orientation.m_yawDegrees = angleToTarget;

	if (std::abs(targetDirection.z) < 0.99f) {
		angleToTarget = GetTurnedTowardDegrees(
			currentForward.GetOrientationAboutYDegrees(),
			targetDirection.GetOrientationAboutYDegrees(),
			maxStepAngle
		);
		m_orientation.m_pitchDegrees = angleToTarget;
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::Attack(Vec3 const& aimDirection) {
	if (m_equippedWeapon != nullptr) {
		if (m_equippedWeapon->Fire(aimDirection)) {
			m_curAnimName = "Attack";
			m_animTimer = 0.f;
			m_curWeaponAnimName = "Attack";
			m_weaponAnimTimer = 0.f;

			if (dynamic_cast<PlayerController*>(m_controller)) {
				if (m_equippedWeapon->m_definition.m_name == "Pistol") {
					m_map->m_game->AddCameraShake(1.0f);
				}
				else {
					m_map->m_game->AddCameraShake(0.5f);
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::EquipWeapon(unsigned int weaponIndex) {
	if (weaponIndex < m_inventory.size()) {
		m_equippedWeapon = m_inventory[weaponIndex];

		m_weaponLitVerts.clear();
		Vec2 weaponSize = m_equippedWeapon->m_definition.m_hud.m_spriteSize.GetVec2();
		Vec2 weaponPivot = m_equippedWeapon->m_definition.m_hud.m_spritePivot;
		AddVertexForQuad3D(
			m_weaponLitVerts,
			Vec3(0.f, -weaponSize.x * weaponPivot.x, -weaponSize.y * weaponPivot.y),
			Vec3(0.f, weaponSize.x * (1.f - weaponPivot.x), -weaponSize.y * weaponPivot.y),
			Vec3(0.f, weaponSize.x * (1.f - weaponPivot.x), weaponSize.y * (1.f - weaponPivot.y)),
			Vec3(0.f, -weaponSize.x * weaponPivot.x, weaponSize.y * (1.f - weaponPivot.y))
		);
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdateWeapon(float deltaSeconds) {
	PlayerController* playerController = dynamic_cast<PlayerController*>(m_controller);
	if ((playerController != nullptr && playerController->m_cameraMode != PlayerCameraMode::FREE_CAMERA) && m_equippedWeapon != nullptr && m_equippedWeapon->m_definition.m_hud.m_animations.size()>0) {
		m_weaponAnimTimer += deltaSeconds; 

		if (m_curWeaponAnimName != "Idle" && m_curWeaponAnimName == "Attack" && m_weaponAnimTimer > m_equippedWeapon->m_definition.m_hud.m_animations.at("Attack")->GetDuration()) {
			m_curWeaponAnimName = "Idle";
			m_weaponAnimTimer = 0.f;
		} 
		else if (m_curWeaponAnimName == "Idle") {
			m_weaponAnimTimer = 0.f;
		}

		UpdateWeaponAnimation(m_curWeaponAnimName, m_weaponAnimTimer);
	}
}

//---------------------------------------------------------------------------------------------------
int Actor::GetQuantizedDirectionAnimationIndex(Camera const& viewCamera) const {
	Vec3 cameraPos = viewCamera.GetPosition();
	Vec3 dispToCamera = cameraPos - m_position;
	float angleToCamera = dispToCamera.GetOrientationAboutZDegrees();
	float actorYaw = m_orientation.GetForwardDir_IFwd_JLeft_KUp().GetOrientationAboutZDegrees();
	float relativeAngle = angleToCamera - actorYaw;
	while (relativeAngle < 0.f) {
		relativeAngle += 360.f;
	}
	while (relativeAngle >= 360.f) {
		relativeAngle -= 360.f;
	}
	int nearestEightDirCount = RoundDownToInt((relativeAngle + 22.5f) / 45.f) % 8;
	return nearestEightDirCount;
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdateActorVertsUVs(AABB2 const& newUVs) {
	Vec2 bottomLeftUV = Vec2(newUVs.m_mins.x, newUVs.m_mins.y);
	Vec2 bottomRightUV = Vec2(newUVs.m_maxs.x, newUVs.m_mins.y);
	Vec2 topRightUV = Vec2(newUVs.m_maxs.x, newUVs.m_maxs.y);
	Vec2 topLeftUV = Vec2(newUVs.m_mins.x, newUVs.m_maxs.y);

	if (!m_definition.m_actor2DRenderInfo.m_renderLit) {
		m_actorUnlitVerts[0].m_uvTexCoords = bottomLeftUV;
		m_actorUnlitVerts[1].m_uvTexCoords = topRightUV;
		m_actorUnlitVerts[2].m_uvTexCoords = bottomRightUV;

		m_actorUnlitVerts[3].m_uvTexCoords = bottomLeftUV;
		m_actorUnlitVerts[4].m_uvTexCoords = topLeftUV;
		m_actorUnlitVerts[5].m_uvTexCoords = topRightUV;
	}
	else {
		m_actorLitVerts[0].m_uvTexCoords = bottomLeftUV;
		m_actorLitVerts[1].m_uvTexCoords = topRightUV;
		m_actorLitVerts[2].m_uvTexCoords = bottomRightUV;

		m_actorLitVerts[3].m_uvTexCoords = bottomLeftUV;
		m_actorLitVerts[4].m_uvTexCoords = topLeftUV;
		m_actorLitVerts[5].m_uvTexCoords = topRightUV;
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdateActorShadowmapVertsUVs(AABB2 const& newUVs) {
	Vec2 bottomLeftUV = Vec2(newUVs.m_mins.x, newUVs.m_mins.y);
	Vec2 bottomRightUV = Vec2(newUVs.m_maxs.x, newUVs.m_mins.y);
	Vec2 topRightUV = Vec2(newUVs.m_maxs.x, newUVs.m_maxs.y);
	Vec2 topLeftUV = Vec2(newUVs.m_mins.x, newUVs.m_maxs.y);
	m_actorShadowmapVerts[0].m_uvTexCoords = bottomLeftUV;
	m_actorShadowmapVerts[1].m_uvTexCoords = topRightUV;
	m_actorShadowmapVerts[2].m_uvTexCoords = bottomRightUV;

	m_actorShadowmapVerts[3].m_uvTexCoords = bottomLeftUV;
	m_actorShadowmapVerts[4].m_uvTexCoords = topLeftUV;
	m_actorShadowmapVerts[5].m_uvTexCoords = topRightUV;
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdateActorAnimation(std::string animationGroupName, float playbackTime, int animationIndex, int shadowmapIndex) {
	if (m_definition.m_actor2DRenderInfo.m_renderLit) {
		if (animationIndex >= 0 && shadowmapIndex >= 0) {
			AABB2 currentSpriteUV = m_definition.m_actor2DRenderInfo.m_animationGroups.at(animationGroupName)->
				m_animations[animationIndex]->GetSpriteDefAtTime(playbackTime).GetUvs();
			UpdateActorVertsUVs(currentSpriteUV);

			AABB2 currentShadowmapSpriteUV = m_definition.m_actor2DRenderInfo.m_animationGroups.at(animationGroupName)->
				m_animations[shadowmapIndex]->GetSpriteDefAtTime(playbackTime).GetUvs();
			UpdateActorShadowmapVertsUVs(currentShadowmapSpriteUV);
		}
		else {
			AABB2 currentSpriteUV = m_definition.m_actor2DRenderInfo.m_animationGroups.at(animationGroupName)->
				m_animations.begin()->second->GetSpriteDefAtTime(playbackTime).GetUvs();
			UpdateActorVertsUVs(currentSpriteUV);
			AABB2 currentShadowmapSpriteUV = m_definition.m_actor2DRenderInfo.m_animationGroups.at(animationGroupName)->
				m_animations.begin()->second->GetSpriteDefAtTime(playbackTime).GetUvs();
			UpdateActorShadowmapVertsUVs(currentShadowmapSpriteUV);
		}
	}
	else {
		if (animationIndex >= 0) {
			AABB2 currentSpriteUV = m_definition.m_actor2DRenderInfo.m_animationGroups.at(animationGroupName)->
				m_animations[animationIndex]->GetSpriteDefAtTime(playbackTime).GetUvs();
			UpdateActorVertsUVs(currentSpriteUV);
		}
		else {
			AABB2 currentSpriteUV = m_definition.m_actor2DRenderInfo.m_animationGroups.at(animationGroupName)->
				m_animations.begin()->second->GetSpriteDefAtTime(playbackTime).GetUvs();
			UpdateActorVertsUVs(currentSpriteUV);
		}
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdateWeaponVertsUVs(AABB2 const& newUVs) {
	Vec2 bottomLeftUV = Vec2(newUVs.m_mins.x, newUVs.m_mins.y);
	Vec2 bottomRightUV = Vec2(newUVs.m_maxs.x, newUVs.m_mins.y);
	Vec2 topRightUV = Vec2(newUVs.m_maxs.x, newUVs.m_maxs.y);
	Vec2 topLeftUV = Vec2(newUVs.m_mins.x, newUVs.m_maxs.y);
	m_weaponLitVerts[0].m_uvTexCoords = bottomLeftUV;
	m_weaponLitVerts[1].m_uvTexCoords = topRightUV;
	m_weaponLitVerts[2].m_uvTexCoords = bottomRightUV;

	m_weaponLitVerts[3].m_uvTexCoords = bottomLeftUV;
	m_weaponLitVerts[4].m_uvTexCoords = topLeftUV;
	m_weaponLitVerts[5].m_uvTexCoords = topRightUV;
}

//---------------------------------------------------------------------------------------------------
void Actor::UpdateWeaponAnimation(std::string animationName, float playbackTime) {
	if (m_equippedWeapon == nullptr) {
		return;
	}
	AABB2 currentSpriteUV = m_equippedWeapon->m_definition.m_hud.m_animations.at(animationName)->GetSpriteDefAtTime(playbackTime).GetUvs();
	UpdateWeaponVertsUVs(currentSpriteUV);
}

//---------------------------------------------------------------------------------------------------
void Actor::Render() const {
	PlayerController* playerController = dynamic_cast<PlayerController*>(m_controller);
	if (playerController == nullptr || playerController->m_cameraMode == PlayerCameraMode::FREE_CAMERA) {
		if (m_definition.m_visible) {
			g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP, SamplerSlot::SLOT0);
			g_engine->m_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP, SamplerSlot::SLOT1);
			g_engine->m_renderer->SetSamplerMode(SamplerMode::SHADOWMAP, SamplerSlot::SLOT2);
			g_engine->m_renderer->BindTexture(m_definition.m_actor2DRenderInfo.m_spriteSheetTexture, TextureSlot::DIFFUSE_SCREEN);
			g_engine->m_renderer->BindTexture(m_definition.m_actor2DRenderInfo.m_spriteSheetNormalTexture, TextureSlot::NORMAL_ORIGINALSCREEN); 
			g_engine->m_renderer->BindTexture(m_definition.m_actor2DRenderInfo.m_spriteSheetAOTexture, TextureSlot::AO_SCREENDEPTH);
			g_engine->m_renderer->BindTexture(nullptr, TextureSlot::PARALLAX_SCREENNORMAL);
			g_engine->m_renderer->BindTexture(m_definition.m_actor2DRenderInfo.m_spriteSheetRoughnessTexture, TextureSlot::ROUGHNESS);
			g_engine->m_renderer->BindTexture(m_definition.m_actor2DRenderInfo.m_spriteSheetMetallicTexture, TextureSlot::METALLIC);
			g_engine->m_renderer->BindTexture(m_definition.m_actor2DRenderInfo.m_spriteSheetEmissiveTexture, TextureSlot::EMISSIVE);

			Mat44 camModelMatrix = m_map->m_game->m_playerController->m_playerCamera->GetCameraToWorldTransform();
			Mat44 modelMatrix;
			if (m_definition.m_actor2DRenderInfo.m_billboardType != BillboardType::NONE) {
				modelMatrix = GetBillboardTransform(
					m_definition.m_actor2DRenderInfo.m_billboardType,
					camModelMatrix,
					m_position,
					Vec2(m_scale.x, m_scale.y)
				);
			}
			else {
				modelMatrix = Mat44::MakeTransform3D(m_position, m_orientation, m_scale);
			}
			g_engine->m_renderer->SetModelConstants(modelMatrix);
			g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);

			if (!m_definition.m_actor2DRenderInfo.m_renderLit) {
				g_engine->m_renderer->DrawVertexArray(m_actorUnlitVerts);
			}
			else {
				g_engine->m_renderer->DrawVertexArray(m_actorLitVerts, m_definition.m_actor2DRenderInfo.m_shader);
			}
		}

		if (m_map->m_game->m_isDrawDebug) {
			if (m_isDead && !m_needDestroy) {
				g_engine->m_renderer->BindTexture(nullptr);

				g_engine->m_renderer->SetModelConstants(Mat44::MakeTransform3D(m_position, m_orientation, m_scale), m_debugDeadColor);
				g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
				g_engine->m_renderer->DrawVertexArray(m_debugVerts);
			}
			else if (!m_needDestroy) {
				g_engine->m_renderer->BindTexture(nullptr);

				g_engine->m_renderer->SetModelConstants(Mat44::MakeTransform3D(m_position, m_orientation, m_scale));
				g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
				g_engine->m_renderer->DrawVertexArray(m_debugVerts);
			}
		}
	}
	else if (playerController != nullptr && playerController->m_cameraMode != PlayerCameraMode::FREE_CAMERA && m_equippedWeapon != nullptr && m_equippedWeapon->m_definition.m_hud.m_animations.size()>0 && !m_isDead) {
		Mat44 camModelMatrix = playerController->m_playerCamera->GetCameraToWorldTransform();
		Vec3 cameraPos = playerController->m_playerCamera->GetPosition();
		Vec3 cameraForward;
		Vec3 cameraLeft;
		Vec3 cameraUp;
		playerController->m_playerCamera->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(cameraForward, cameraLeft, cameraUp);
		Vec3 weaponRenderPos = cameraPos + (cameraForward * m_definition.m_collision.m_radius * 0.5f) - cameraUp * 0.26f;

		Mat44 modelMatrix = GetBillboardTransform(
			BillboardType::FULL_FACING,
			camModelMatrix,
			weaponRenderPos,
			Vec2(0.001f, 0.001f)
		);

		g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
		g_engine->m_renderer->SetModelConstants(modelMatrix);
		g_engine->m_renderer->BindTexture(&m_equippedWeapon->m_definition.m_hud.m_spriteSheet->GetTexture(), TextureSlot::DIFFUSE_SCREEN);
		g_engine->m_renderer->BindTexture(m_equippedWeapon->m_definition.m_hud.m_spriteSheetNormalTexture, TextureSlot::NORMAL_ORIGINALSCREEN);
		g_engine->m_renderer->BindTexture(nullptr, TextureSlot::AO_SCREENDEPTH);
		g_engine->m_renderer->BindTexture(nullptr, TextureSlot::PARALLAX_SCREENNORMAL);
		g_engine->m_renderer->BindTexture(nullptr, TextureSlot::ROUGHNESS);
		g_engine->m_renderer->BindTexture(nullptr, TextureSlot::METALLIC);
		g_engine->m_renderer->BindTexture(m_equippedWeapon->m_definition.m_hud.m_spriteSheetEmissiveTexture, TextureSlot::EMISSIVE);
		g_engine->m_renderer->DrawVertexArray(m_weaponLitVerts, m_equippedWeapon->m_definition.m_hud.m_shader);
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::RenderShadowmap() const {
	if (m_definition.m_visible) {
		if (m_definition.m_actor2DRenderInfo.m_renderLit) {
			Mat44 camModelMatrix = m_map->m_sunShadowCamera->GetCameraToWorldTransform();
			Mat44 modelMatrix = GetBillboardTransform(
				m_definition.m_actor2DRenderInfo.m_billboardType,
				camModelMatrix,
				m_position,
				Vec2(m_scale.x, m_scale.y)
			);

			g_engine->m_renderer->BindTexture(&(m_definition.m_actor2DRenderInfo.m_spriteSheet->GetTexture()));
			g_engine->m_renderer->SetModelConstants(modelMatrix);
			g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
			g_engine->m_renderer->DrawVertexArray(m_actorShadowmapVerts, m_definition.m_actor2DRenderInfo.m_shader);
		}
	}
}