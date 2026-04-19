#include "Game/Actor.hpp"
#include "Game/ActorHandle.hpp"
#include "Game/Map.hpp"
#include "Game/Controller.hpp"
#include "Game/PlayerController.hpp"
#include "Game/AIController.hpp"
#include "Game/Weapon.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

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
	m_equippedWeapon = m_inventory.empty() ? nullptr : m_inventory[0];

	if (m_definition.m_actorAI.m_aiEnabled) {
		m_controller = new AIController(m_map);
		m_controller->Possess(m_handle);
	}

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
		m_velocity -= Vec3(m_velocity.x, m_velocity.y, 0.f) * m_definition.m_physics.m_drag * deltaSeconds;
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

	if (m_curHealth <= 0.f) {
		m_isDead = true;
		m_deadTimer = m_definition.m_corpseLifetime;
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
	if (other!=nullptr && m_definition.m_collision.m_damageOnCollide.GetLength() > 0.f) {
		float randomDamage = m_map->m_game->m_randomGenerator->RollRandomFloatInRange(m_definition.m_collision.m_damageOnCollide.m_min, m_definition.m_collision.m_damageOnCollide.m_max);
		if (m_owner == nullptr) {
			other->Damage(randomDamage, this);
		}
		else {
			other->Damage(randomDamage, m_owner);
		}
	}

	if (other!=nullptr && m_definition.m_collision.m_impulseOnCollide > 0.f) {
		Vec3 impulseDirection = (other->m_position - m_position).GetNormalized();
		other->AddImpulse(impulseDirection * m_definition.m_collision.m_impulseOnCollide);
	}

	if (m_definition.m_collision.m_dieOnCollide) {
		m_isDead = true;
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
		m_equippedWeapon->Fire(aimDirection);
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::EquipWeapon(unsigned int weaponIndex) {
	if (weaponIndex < m_inventory.size()) {
		m_equippedWeapon = m_inventory[weaponIndex];
	}
}

//---------------------------------------------------------------------------------------------------
void Actor::Render() const {
	PlayerController* playerController = dynamic_cast<PlayerController*>(m_controller);
	if (playerController == nullptr || playerController->m_cameraMode == PlayerCameraMode::FREE_CAMERA) {
		if (m_isDead && !m_needDestroy) {
			g_engine->m_renderer->BindTexture(nullptr);
			g_engine->m_renderer->SetModelConstants(Mat44::MakeTransform3D(m_position, m_orientation, m_scale), m_debugDeadColor);
			g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
			g_engine->m_renderer->DrawVertexArray((int)m_debugVerts.size(), m_debugVerts.data());
		}
		else if(!m_needDestroy){
			g_engine->m_renderer->BindTexture(nullptr);
			g_engine->m_renderer->SetModelConstants(Mat44::MakeTransform3D(m_position, m_orientation, m_scale));
			g_engine->m_renderer->SetBlendMode(BlendMode::ALPHA);
			g_engine->m_renderer->DrawVertexArray((int)m_debugVerts.size(), m_debugVerts.data());
		}
	}
}