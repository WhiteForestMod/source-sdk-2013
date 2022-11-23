//================================ WHITE FOREST ===============================//
//
// Purpose:	A nail gun that shoots nails, nails can be used to conduct electricity
//
//=============================================================================//


#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon_shared.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "beam_shared.h"
#include "rumble_shared.h"
#include "gamestats.h"
#include "decals.h"
#include "engine/ivdebugoverlay.h"
#include "physics_prop_ragdoll.h"
#include "RagdollBoogie.h"
#include "soundenvelope.h"
#include "point_electric_nail.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_plr_dmg_nailgun;
extern ConVar sk_npc_dmg_nailgun;

#define NAIL_MODEL "models/props_mining/railroad_spike01.mdl"
#define NAIL_AIR_VELOCITY	2500

#define NAIL_SPRITE			"sprites/bluelight1.vmt"
#define NAIL_GLOW_SPRITE	"sprites/blueglow1.vmt"

class CNail : public CBaseCombatCharacter
{
	DECLARE_CLASS(CNail, CBaseCombatCharacter);

public:
	CNail() {};
	~CNail() {};

public:
	Class_T Classify(void) { return CLASS_NONE; }

	void Spawn(void);
	void Precache(void);
	void NailTouch(CBaseEntity* pOther);
	void NailThink(void);
	bool CreateVPhysics(void);
	unsigned int PhysicsSolidMaskForEntity() const;

	static CNail* NailCreate(const Vector& vecOrigin, const QAngle& angAngles, CBasePlayer* pentOwner = NULL);

protected:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};
LINK_ENTITY_TO_CLASS(nailgun_nail, CNail);

BEGIN_DATADESC(CNail)
DEFINE_FUNCTION(NailThink),
DEFINE_FUNCTION(NailTouch),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNail, DT_Nail)
END_SEND_TABLE()

void CNail::Precache(void)
{
	PrecacheModel(NAIL_MODEL);
}

void CNail::Spawn(void)
{
	Precache();

	SetModel(NAIL_MODEL);
	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
	UTIL_SetSize(this, -Vector(0.3f, 0.3f, 0.3f), Vector(0.3f, 0.3f, 0.3f));
	SetSolid(SOLID_BBOX);
	SetGravity(0.05f);

	SetThink(&CNail::NailThink);
	SetTouch(&CNail::NailTouch);

	SetNextThink(gpGlobals->curtime + 0.1f);
}

CNail* CNail::NailCreate(const Vector& vecOrigin, const QAngle& angAngles, CBasePlayer* pentOwner)
{
	CNail* pNail = (CNail*)CreateEntityByName("nailgun_nail");

	UTIL_SetOrigin(pNail, vecOrigin);
	pNail->SetAbsAngles(angAngles);
	pNail->Spawn();
	pNail->SetOwnerEntity(pentOwner);

	return pNail;
}

void CNail::NailThink(void)
{
	QAngle angNewAngles;

	VectorAngles(GetAbsVelocity(), angNewAngles);
	SetAbsAngles(angNewAngles);

	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CNail::NailTouch(CBaseEntity* pOther)
{
	// This is pretty much copy/pasted from the crossbow

	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
			return;
	}

	if (pOther->m_takedamage != DAMAGE_NO)
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);

		if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC())
		{
			CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), sk_plr_dmg_nailgun.GetFloat(), DMG_NEVERGIB);
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);

			CBasePlayer* pPlayer = ToBasePlayer(GetOwnerEntity());
			if (pPlayer)
			{
				gamestats->Event_WeaponHit(pPlayer, true, "weapon_nailgun", dmgInfo);
			}

		}
		else
		{
			CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), sk_plr_dmg_nailgun.GetFloat(), DMG_BULLET | DMG_NEVERGIB);
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
		}

		ApplyMultiDamage();

		//Keep going through the glass.
		if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			return;

		if (!pOther->IsAlive())
		{
			// We killed it! 
			const surfacedata_t* pdata = physprops->GetSurfaceData(tr.surface.surfaceProps);
			if (pdata->game.material == CHAR_TEX_GLASS)
			{
				return;
			}
		}

		SetAbsVelocity(Vector(0, 0, 0));

		// play body "thwack" sound
		EmitSound("Weapon_Crossbow.BoltHitBody");

		Vector vForward;

		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize(vForward);

		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2);

		if (tr2.fraction != 1.0f)
		{
			//			NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
			//			NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if (tr2.m_pEnt == NULL || (tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE))
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;

				DispatchEffect("BoltImpact", data);
			}
		}

		SetTouch(NULL);
		SetThink(NULL);

		if (!g_pGameRules->IsMultiplayer())
		{
			UTIL_Remove(this);
		}
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if (pOther->GetMoveType() == MOVETYPE_NONE && !(tr.surface.flags & SURF_SKY))
		{
			EmitSound("Weapon_Crossbow.BoltHitWorld");

			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();

			// We have hit a solid brush
			SetThink(&CNail::SUB_Remove);
			SetNextThink(gpGlobals->curtime + 2.0f);

			//FIXME: We actually want to stick (with hierarchy) to what we've hit
			SetMoveType(MOVETYPE_NONE);

			Vector vForward;

			AngleVectors(GetAbsAngles(), &vForward);
			VectorNormalize(vForward);

			CEffectData	data;

			data.m_vOrigin = tr.endpos;
			data.m_vNormal = vForward;
			data.m_nEntIndex = 0;

			//DispatchEffect("BoltImpact", data);

			UTIL_ImpactTrace(&tr, DMG_BULLET);

			// Create a point_electric_nail
			CPointElectricNail* nail = (CPointElectricNail*)CreateEntityByName("point_electric_nail");
			//nail->SpawnAtPosition(GetAbsOrigin());
			nail->SpawnAtPosition(tr.endpos - vForward * 4);
			nail->SetAbsAngles(GetAbsAngles());

			AddEffects(EF_NODRAW);
			SetTouch(NULL);
			SetThink(&CNail::SUB_Remove);
			SetNextThink(gpGlobals->curtime + 2.0f);

			// Shoot some sparks
			if (UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER)
			{
				g_pEffects->Sparks(GetAbsOrigin());
			}
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ((tr.surface.flags & SURF_SKY) == false)
			{
				UTIL_ImpactTrace(&tr, DMG_BULLET);
			}

			UTIL_Remove(this);
		}
	}
}

bool CNail::CreateVPhysics(void)
{
	VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);

	return true;
}

unsigned int CNail::PhysicsSolidMaskForEntity() const
{
	return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX) & ~CONTENTS_GRATE;
}


//=============================================================================//
//
// Nailgun
//
//=============================================================================//
class CWeaponNailgun : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS(CWeaponNailgun, CBaseHLCombatWeapon);

	CWeaponNailgun(void);

	DECLARE_SERVERCLASS();

	void	Precache(void);
	void	PrimaryAttack(void);
	void	FireNail(void);

	// Implemented overrides
	virtual int	GetMinBurst() { return 1; }

	virtual int	GetMaxBurst() { return 1; }

	virtual float GetFireRate(void) { return 0.25f; }

	virtual const Vector& GetBulletSpread(void) { return VECTOR_CONE_PRECALCULATED; }

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponNailgun, DT_WeaponNailgun)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_nailgun, CWeaponNailgun);
PRECACHE_WEAPON_REGISTER(weapon_nailgun);

BEGIN_DATADESC(CWeaponNailgun)

DEFINE_FIELD(m_flSoonestPrimaryAttack, FIELD_TIME),
DEFINE_FIELD(m_flLastAttackTime, FIELD_TIME),

END_DATADESC()

acttable_t	CWeaponNailgun::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};


IMPLEMENT_ACTTABLE(CWeaponNailgun);

CWeaponNailgun::CWeaponNailgun(void)
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;

	m_fMinRange1 = 24;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 24;
	m_fMaxRange2 = 200;

	m_bFiresUnderwater = false;
}

void CWeaponNailgun::Precache(void)
{
	PrecacheModel(NAIL_SPRITE);
	PrecacheModel(NAIL_GLOW_SPRITE);
	PrecacheModel(NAIL_MODEL);

	BaseClass::Precache();
}

void CWeaponNailgun::PrimaryAttack(void)
{
	// DIRTY HACK!!
	//
	// By calling the base class we are technically firing two things here; the nail created by FireNail()
	// and a regular raytrace bullet, if we wanted to avoid this then we would have to essentially reimplement
	// the logic of the base class here and I really can't be bothered to do that as calling the base class
	// gives us a bunch of stuff for free like animations and ammo management.

	FireNail();
	BaseClass::PrimaryAttack();
}

void CWeaponNailgun::FireNail()
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	Vector vecAiming = pOwner->GetAutoaimVector(0);
	Vector vecSrc = pOwner->Weapon_ShootPosition();

	QAngle angAiming;
	VectorAngles(vecAiming, angAiming);

	CNail* nail = CNail::NailCreate(vecSrc, angAiming, pOwner);
	nail->SetAbsVelocity(vecAiming * NAIL_AIR_VELOCITY);
}