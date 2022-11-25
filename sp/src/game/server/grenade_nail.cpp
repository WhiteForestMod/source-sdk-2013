//========================  WHITE FOREST  ========================//
//
// Purpose:	A crude nailbomb that discharges electricity to impacted nails
//
//================================================================//


#include "cbase.h"
#include "gamerules.h"
#include "basegrenade_shared.h"
#include "grenade_nail.h"
#include "beam_shared.h"
#include "grenade_nail_impact.h"
#include "point_electric_nail.h"
#include "RagdollBoogie.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Begin Macros

#define GRENADE_MODEL				"models/wf/debugpyramid.mdl"
#define GRENADE_BEAM_SPRITE			"sprites/bluelight1.vmt"
#define GRENADE_FLOOR_THRESHOLD		40.0f

#define GRENADE_IMPACT_MODEL		"models/props_mining/railroad_spike01.mdl"

// Begin ConVars
ConVar wf_nail_grenade_nail_count("wf_nail_grenade_nail_count", "7", FCVAR_ARCHIVE, "How many nails should a nail grenade discharge");
ConVar wf_nail_grenade_radius("wf_nail_grenade_radius", "55", FCVAR_ARCHIVE, "Nail grenade explosion radius, NPC in this radius will be ragdolled");

// Begin method definitions
class CGrenadeNail : public CBaseGrenade
{
	DECLARE_CLASS(CGrenadeNail, CBaseGrenade);

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

public:
	CBaseGrenade* NailGrenade_Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner);

	void	Spawn(void);
	void	Precache(void);
	void	StartTimer(void);
	void	CountdownThink(void);
	void	Detonate(void);
	void	Explode(trace_t* pTrace, int bitsDamageType);
	void	PostDetonation(void);
	void	SetVelocity(const Vector& velocity, const AngularImpulse& angVelocity);
	void	VPhysicsUpdate(IPhysicsObject* pPhysics);
	virtual void VPhysicsCollision(int index, gamevcollisionevent_t* pEvent);

protected:
	float	m_fShutdownTime;

	bool	m_bHasDetonated;
	bool	m_bTouchedWorld;
	bool	m_bBeamsDrawn;
	Vector	m_vLastTouch;

	CUtlVector<Vector> m_vNailPositions;
	CUtlVector<CBeam*> m_vNailBeams;
	CUtlVector<CGrenadeNailImpact*> m_vNailImpacts;

	short		g_sModelIndexFireball;		// holds the index for the fireball
	short		g_sModelIndexWExplosion;	// holds the index for the underwater explosion
};

LINK_ENTITY_TO_CLASS(npc_grenade_nail, CGrenadeNail);

BEGIN_DATADESC(CGrenadeNail)

DEFINE_THINKFUNC(CountdownThink),
DEFINE_THINKFUNC(PostDetonation),

END_DATADESC()


// Begin method implementations

CBaseGrenade* NailGrenade_Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner)
{
	CGrenadeNail* pGrenade = (CGrenadeNail*)CBaseEntity::Create("npc_grenade_nail", position, angles, pOwner);

	pGrenade->StartTimer();
	pGrenade->SetVelocity(velocity, angVelocity);
	pGrenade->SetThrower(ToBaseCombatCharacter(pOwner));
	pGrenade->m_takedamage = DAMAGE_EVENTS_ONLY;

	return pGrenade;
}

void CGrenadeNail::Spawn(void)
{
	Precache();

	SetModel(GRENADE_MODEL);

	SetCollisionGroup(COLLISION_GROUP_WEAPON);

	AddSolidFlags(FSOLID_NOT_STANDABLE);

	VPhysicsInitNormal(SOLID_BBOX, 0, false);

	m_bTouchedWorld = false;

	BaseClass::Spawn();
}

void CGrenadeNail::Precache(void)
{
	// call base precache
	PrecacheModel(GRENADE_MODEL);
	PrecacheModel(GRENADE_BEAM_SPRITE);
	PrecacheScriptSound("NPC_Hunter.FlechetteExplode");

	g_sModelIndexFireball = CBaseEntity::PrecacheModel("sprites/zerogxplode.vmt");// fireball
	g_sModelIndexWExplosion = CBaseEntity::PrecacheModel("sprites/WXplo1.vmt");// underwater fireball

	BaseClass::Precache();
}

void CGrenadeNail::StartTimer(void)
{
	// 5 second timer, MOVE THIS TO A CONVAR
	m_flDetonateTime = gpGlobals->curtime + 5.0f;

	SetThink(&CGrenadeNail::CountdownThink);
	SetNextThink(gpGlobals->curtime);
}

void CGrenadeNail::CountdownThink(void)
{
	// The grenade should now be counting down to detonation, we should
	//		- Detonate if the timer has elapsed
	//		- Warn nearby NPCs that they are in danger
	if (gpGlobals->curtime > m_flDetonateTime)
	{
		Detonate();
		return;
	}

	SetNextThink(gpGlobals->curtime + 0.1);
}

void CGrenadeNail::Detonate(void)
{
	// Called upon detonation, we should
	//		- Check to see if we are OK to detonate (are we on a valid surface)
	//			- (If we are not on a valid surface then we should not continue as we are in the air, just explode and throw no nails)
	//		- yeet our nails upwards relative to how we landed
	//		- Create an inactive electric nail at our position
	//		- Perform explosion effects
	//		- Begin post-detonation squence

	SetSolid(SOLID_NONE);

	IPhysicsObject* physObject = VPhysicsGetObject();
	Vector currentPosition;
	QAngle currentAngles;

	physObject->GetPosition(&currentPosition, &currentAngles);
	vec_t distance = currentPosition.DistTo(m_vLastTouch);

	if (distance < GRENADE_FLOOR_THRESHOLD)
	{
		int maximumNails = wf_nail_grenade_nail_count.GetInt();

		// Shoot either 50 times or until we reach the maximum count;
		for (int i = 0; i < 50; i++)
		{
			if (m_vNailPositions.Count() >= maximumNails)
			{
				break;
			}

			float randomX = random->RandomFloat(200, 340);
			float randomY = random->RandomFloat(0, 359);

			trace_t trace;
			Vector nailForwardVector;
			QAngle nailAngle = QAngle(randomX, randomY, 0);
			AngleVectors(nailAngle, &nailForwardVector);

			UTIL_TraceLine(currentPosition, currentPosition + (nailForwardVector * 500), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace);

			if (trace.DidHitWorld())
			{
				m_vNailPositions.AddToTail(trace.endpos);

				Vector spawnPosition = (trace.endpos - nailForwardVector * 8);
				CGrenadeNailImpact* impact = GrenadeNailImpact_Create(spawnPosition, nailAngle, currentPosition);
				m_vNailImpacts.AddToTail(impact);
			}
		}

		// Create our nail entity
		CPointElectricNail* nail = (CPointElectricNail*)CreateEntityByName("point_electric_nail");
		nail->SpawnAtPositionNotInteracable(GetAbsOrigin());
		nail->Activate(false);

		// Shutdown after 10 seconds
		m_fShutdownTime = gpGlobals->curtime + 10.0f;
	}
	else
	{
		// We are NOT in a good position to throw nails so we will just explode like a regular grenade
		DevMsg("Nail Grenade was not in a good position!");
	}

	physObject->EnableMotion(false);

	// Figure out the center of the explosion (See basegrenade_shared.cpp line 293 for an explanation)
	trace_t tr;
	Vector vecSpot = GetAbsOrigin() + Vector(0, 0, 8);
	UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -32), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr);

	if (tr.startsolid)
	{
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -32), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr);
	}

	// Perform explosion effects
	Explode(&tr, DMG_BLAST);

	SetThink(&CGrenadeNail::PostDetonation);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CGrenadeNail::Explode(trace_t* pTrace, int bitsDamageType)
{
	// Called after detonation, we should
	//		- Perform any explosion effects

	EmitSound("NPC_Hunter.FlechetteExplode");

	// Essentially copied from the base class but with some custom effects
	Vector vecAbsOrigin = GetAbsOrigin();
	int explosionRadius = wf_nail_grenade_radius.GetFloat();

	//int contents = UTIL_PointContents(vecAbsOrigin);

	//if (pTrace->fraction != 1.0)
	//{
	//	Vector vecNormal = pTrace->plane.normal;
	//	surfacedata_t* pdata = physprops->GetSurfaceData(pTrace->surface.surfaceProps);
	//	CPASFilter filter(vecAbsOrigin);

	//	te->Explosion(filter, -1.0, // don't apply cl_interp delay
	//		&vecAbsOrigin,
	//		!(contents & MASK_WATER) ? g_sModelIndexFireball : g_sModelIndexWExplosion,
	//		m_DmgRadius * .03,
	//		25,
	//		TE_EXPLFLAG_NONE,
	//		m_DmgRadius,
	//		m_flDamage,
	//		&vecNormal,
	//		(char)pdata->game.material);
	//}
	//else
	//{
	//	CPASFilter filter(vecAbsOrigin);
	//	te->Explosion(filter, -1.0, // don't apply cl_interp delay
	//		&vecAbsOrigin,
	//		!(contents & MASK_WATER) ? g_sModelIndexFireball : g_sModelIndexWExplosion,
	//		m_DmgRadius * .03,
	//		25,
	//		TE_EXPLFLAG_NONE,
	//		m_DmgRadius,
	//		m_flDamage);
	//}

	/*CBaseCombatCharacter* m_hThrower = GetThrower();
	Vector vecReported = m_hThrower ? m_hThrower->GetAbsOrigin() : vec3_origin;

	CTakeDamageInfo info(this, m_hThrower, GetBlastForce(), GetAbsOrigin(), m_flDamage, bitsDamageType, 0, &vecReported);

	RadiusDamage(info, GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL);*/

	UTIL_DecalTrace(pTrace, "Scorch");

	// Find nearby enemies and ragdoll them
	NDebugOverlay::Sphere(vecAbsOrigin, explosionRadius, 255, 0, 255, true, 10.0f);

	CBaseEntity* entitiesToRagdoll[1024];
	int entityCount = UTIL_EntitiesInSphere(entitiesToRagdoll, 1024, GetAbsOrigin(), explosionRadius, FL_NPC | FL_CLIENT);

	for (int i = 0; i < entityCount; i++)
	{
		if (entitiesToRagdoll[i]->IsPlayer())
		{
			// TODO [WF] Zap the player for being stupid enough to stand next to their own grenade
		}
		else
		{
			if (entitiesToRagdoll[i]->MyCombatCharacterPointer() != NULL)
			{
				if (entitiesToRagdoll[i]->MyCombatCharacterPointer()->CanBecomeRagdoll())
				{
					entitiesToRagdoll[i]->MyCombatCharacterPointer()->BecomeRagdollBoogie(this, GetAbsOrigin(), 5.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL);
				}
			}
		}

	}


	//EmitSound("BaseGrenade.Explode");
}

void CGrenadeNail::PostDetonation(void)
{
	// We have successfully exploded, now do our electric beams to our nails, we should
	//		- Draw our beams to nails
	//		- Perform damage if someone touches the beams
	//		- Turn off after the alloted time

	if (gpGlobals->curtime > m_fShutdownTime)
	{
		for (int i = 0; i < m_vNailImpacts.Count(); i++)
		{
			m_vNailImpacts[i]->ShutdownBeam();
		}
	}

	SetThink(&CGrenadeNail::PostDetonation);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CGrenadeNail::SetVelocity(const Vector& velocity, const AngularImpulse& angVelocity)
{
	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject)
	{
		pPhysicsObject->AddVelocity(&velocity, &angVelocity);
	}
}

void CGrenadeNail::VPhysicsUpdate(IPhysicsObject* pPhysics)
{
	BaseClass::VPhysicsUpdate(pPhysics);

	// TODO [WF] Yoink the code from grenade_frag so that we bounce of NPCs
}

void CGrenadeNail::VPhysicsCollision(int index, gamevcollisionevent_t* pEvent)
{
	// Called whenever we have hit something, we are only interested when we hit the world, we should
	//		- Set flag that we have hit the world
	//		- Update where we last hit the world

	BaseClass::VPhysicsCollision(index, pEvent);

	int otherIndex = !index;
	CBaseEntity* pHitEntity = pEvent->pEntities[otherIndex];

	if (pHitEntity)
	{
		if (pHitEntity->IsWorld())
		{
			IPhysicsObject* physObject = VPhysicsGetObject();
			Vector currentPosition;
			QAngle currentAngles;

			physObject->GetPosition(&currentPosition, &currentAngles);

			m_vLastTouch = currentPosition;
			m_bTouchedWorld = true;
		}
	}
}
