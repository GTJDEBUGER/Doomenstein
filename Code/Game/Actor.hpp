#pragma once
#include "Game/ActorDefinition.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
struct ActorHandle;
class Map;
class Controller;
class Weapon;
class Camera;

//-----------------------------------------------------------------------------------------------
class Actor {
public:
	Actor(ActorDefinition const& definition, Map* m_map, ActorHandle* handle, Vec3 pos, EulerAngles orien, Vec3 scale);
	~Actor();

	void    Update(float deltaSeconds);
	void    Render(Camera const& viewCamera) const;

	void    UpdatePhysics(float deltaSeconds);
	void    Damage(float damageAmount, Actor* attacker);
	void    AddForce(Vec3 const& force);
	void    AddImpulse(Vec3 const& impulse);
	void    OnCollide(Actor* other);
	void    OnPossessed();
	void    OnUnpossessed();
	void    MoveInDirection(Vec3 const& direction, float targetSpeed);
	void    TurnInDirection(Vec3 const& direction, float maxStepAngle = -1);
	void    Attack(Vec3 const& aimDirection);
	void    EquipWeapon(unsigned int weaponIndex=0);
	void    UpdateWeapon(float deltaSeconds);

	int     GetQuantizedDirectionAnimationIndex(Camera const& viewCamera) const;
	void    UpdateWeaponVertsUVs(AABB2 const& newUVs);
	void    UpdateWeaponAnimation(std::string animationName, float playbackTime = 0.f);
	void    GetCurAnimationMeshVerts(std::vector<Vertex_TBN>& out_actorVerts, Camera const& viewCamera) const;
	void    GetCurAnimationMeshVerts(std::vector<Vertex>& out_actorVerts, Camera const& viewCamera) const;
	
	void    RenderShadowmap() const;

public:
	bool                      m_isFrozenPhysics = false;
	bool                      m_isSubActor   = false;
	ActorHandle*              m_handle       = nullptr;
	ActorDefinition const&    m_definition;
	Map*                      m_map;
	Actor*                    m_owner        = nullptr;
						      
	Vec3                      m_position       = Vec3(0.f, 0.f, 0.f);
	EulerAngles               m_orientation    = EulerAngles(0.f, 0.f, 0.f);
	Vec3                      m_scale          = Vec3(1.f, 1.f, 1.f);
						      
	Vec3                      m_velocity       = Vec3(0.f, 0.f, 0.f);
	Vec3                      m_acceleration   = Vec3(0.f, 0.f, 0.f);
						      
	Rgba8                     m_debugColor     = Rgba8::BLUE;
	Rgba8                     m_debugDeadColor = Rgba8(100,100,100);
	std::vector<Vertex>       m_debugVerts;
							  
	std::string               m_curAnimName = "Idle";
	float                     m_animTimer = 0.f;
	std::vector<Vertex_TBN>   m_weaponLitVerts;
	std::string               m_curWeaponAnimName = "Idle";
	float                     m_weaponAnimTimer = 0.f;
							  
	std::vector<Weapon*>      m_inventory;
	Weapon*                   m_equippedWeapon = nullptr;
						      
	bool                      m_isDead       = false;
	bool                      m_needDestroy  = false;
	float                     m_deadTimer    = 0.f;
						      
	float                     m_curEyeHeight = 0.f;
	float                     m_curHealth    = 0.f;
	Controller*               m_controller   = nullptr;
	bool                      m_isGrounded   = false;
	bool                      m_isInWater    = false;

	float                     m_lifeTimer    = -1.f;
};