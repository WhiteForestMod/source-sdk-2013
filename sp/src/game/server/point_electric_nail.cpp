//========================  WHITE FOREST  ========================//
//
// Electric Nails - Created when a nail fired by the nailgun impacts a surface
//
//================================================================//

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

ConVar wf_nail_search_radius("wf_nail_search_radius", "100", FCVAR_ARCHIVE, "Determines how far electricity can jump between nails");

#define NAIL_MODEL "models/props_mining/railroad_spike01.mdl"
#define NAIL_AIR_VELOCITY	2500

#define NAIL_SPRITE			"sprites/bluelight1.vmt"
#define NAIL_GLOW_SPRITE	"sprites/blueglow1.vmt"


LINK_ENTITY_TO_CLASS(point_electric_nail, CPointElectricNail);

BEGIN_DATADESC(CPointElectricNail)

	// Property Definitions
	DEFINE_FIELD(m_bIsActive, FIELD_BOOLEAN),
	DEFINE_KEYFIELD(m_bIsActive, FIELD_BOOLEAN, "IsActive"),

	DEFINE_FIELD(m_bShouldNotSearchForChildren, FIELD_BOOLEAN),
	DEFINE_KEYFIELD(m_bShouldNotSearchForChildren, FIELD_BOOLEAN, "ShouldNotSearchForChild"),

	// Function Definitions
	DEFINE_FUNCTION(NailThink),
	DEFINE_FUNCTION(PickupNail),

	// Input / Output definitions
	DEFINE_INPUTFUNC(FIELD_VOID, "InputActivate", InputActivate),


	DEFINE_OUTPUT(m_OnActivated, "OnActivated"),
	DEFINE_OUTPUT(m_OnDeactivated, "OnDeactivated"),

END_DATADESC()


CPointElectricNail::CPointElectricNail() { }

void CPointElectricNail::Precache(void)
{
	PrecacheModel(NAIL_SPRITE);
	PrecacheModel(NAIL_GLOW_SPRITE);
	PrecacheModel(NAIL_MODEL);
}

void CPointElectricNail::Spawn()
{
	Precache();

	SetThink(&CPointElectricNail::NailThink);
	SetNextThink(gpGlobals->curtime + 0.5f);
}

void CPointElectricNail::SpawnAtPosition(const Vector& position)
{
	Precache();

	SetModel(NAIL_MODEL);
	SetModelScale(0.5f);

	SetSolid(SOLID_VPHYSICS);
	SetAbsOrigin(position);
	SetUse(&CPointElectricNail::PickupNail);

	m_flNextSparkTime = gpGlobals->curtime + random->RandomFloat(0.5, 1.5);

	NailThink();
}

void CPointElectricNail::SpawnAtPositionNotInteracable(const Vector& position)
{
	// Used by the nail grenade, we want to have an electricity source but the player 
	// should not be able to interact with it

	Precache();

	SetSolid(SOLID_NONE);
	SetAbsOrigin(position);

	NailThink();
}

void CPointElectricNail::InputActivate(inputdata_t& inputData)
{
	Activate(false);
	m_OnActivated.FireOutput(inputData.pActivator, this);
}

void CPointElectricNail::Activate(bool shouldFireOutput)
{
	m_bIsActive = true;

	m_CGlowSprite = CSprite::SpriteCreate(NAIL_GLOW_SPRITE, GetLocalOrigin(), false);
	m_CGlowSprite->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxPulseFast);
	m_CGlowSprite->SetScale(0.2f);

	if (shouldFireOutput)
	{
		m_OnActivated.FireOutput(this, this);
	}

	SetThink(&CPointElectricNail::NailThink);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CPointElectricNail::InputDeactivate(inputdata_t& inputData)
{
	Deactivate(false);
	m_OnDeactivated.FireOutput(inputData.pActivator, this);
}

void CPointElectricNail::Deactivate(bool shouldFireOutput)
{
	m_bIsActive = false;
	m_CFirstChild = NULL;

	if (m_CGlowSprite != NULL)
	{
		UTIL_Remove(m_CGlowSprite);
	}

	if (m_CFirstChildBeam != NULL)
	{
		UTIL_Remove(m_CFirstChildBeam);
	}

	if (shouldFireOutput)
	{
		m_OnDeactivated.FireOutput(this, this);
	}

	SetThink(&CPointElectricNail::NailThink);
	SetNextThink(gpGlobals->curtime + 1.0f);
}

/// <summary>
/// Main thinking logic, we should;
///		- Activly look for new children
///		- Apply damage on the beams that link our children
/// </summary>
void CPointElectricNail::NailThink()
{
	if (gpGlobals->curtime > m_flNextSparkTime)
	{
		m_flNextSparkTime = gpGlobals->curtime + random->RandomFloat(0.5, 1.5);
		
		if (m_bIsActive)
		{
			g_pEffects->Sparks(GetAbsOrigin());
		}
	}

	if (m_bShouldNotSearchForChildren)
	{
		SetThink(NULL);
		return;
	}

	CalculateDamage(m_CFirstChildBeam, m_CFirstChild);

	if (m_CFirstChild != NULL)
	{
		if (!m_CFirstChild->m_bIsActive)
		{
			// Our child has deactivated! discard it and search for a new one
			Deactivate(true);
			return;
		}

		SetThink(&CPointElectricNail::NailThink);
		SetNextThink(gpGlobals->curtime + 0.1f);

		return;
	}

	trace_t trace;
	CPointElectricNail* child = NULL;
	CBaseEntity* iterator = NULL;
	while ((iterator = gEntList.FindEntityByClassnameWithin(iterator, "point_electric_nail", GetAbsOrigin(), wf_nail_search_radius.GetFloat())) != NULL)
	{
		if (GetAbsOrigin() == iterator->GetAbsOrigin())
		{
			// We have found ourself
			continue;
		}

		CPointElectricNail* potentialChild = dynamic_cast<CPointElectricNail*> (iterator);

		// Ignore non active nails
		if (!potentialChild->m_bIsActive)
		{
			continue;
		}

		// Assign the first nail we find
		if (child == NULL)
		{
			child = potentialChild;
			continue;
		}

		// Are there any closer nails?
		vec_t distanceToPotentialChild = GetAbsOrigin().DistTo(potentialChild->GetAbsOrigin());
		vec_t distanceToCurrentChild = GetAbsOrigin().DistTo(child->GetAbsOrigin());

		if (distanceToPotentialChild < distanceToCurrentChild)
		{
			// Can we see it?


			child = potentialChild;
		}
	}

	if (m_CFirstChild == NULL && child != NULL)
	{
		m_CFirstChild = child;
		m_bIsActive = true;
		Activate(true);

		m_CFirstChildBeam = DrawBeamForChild(m_CFirstChild);
	}

	SetThink(&CPointElectricNail::NailThink);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

CBeam* CPointElectricNail::DrawBeamForChild(CPointElectricNail* child)
{
	CBeam* beam = CBeam::BeamCreate(NAIL_SPRITE, 3.0f);
	beam->SetStartPos(GetAbsOrigin());
	beam->PointEntInit(child->GetAbsOrigin(), this);
	beam->SetBrightness(255);
	beam->SetNoise(5.0f);

	float scrollOffset = gpGlobals->curtime + 15.5;
	beam->SetScrollRate(scrollOffset);

	return beam;
}

void CPointElectricNail::CalculateDamage(CBeam* beam, CPointElectricNail* child)
{
	if (beam == NULL || child == NULL)
	{
		return;
	}

	trace_t trace;
	UTIL_TraceLine(GetAbsOrigin(), child->GetAbsOrigin(), MASK_SOLID, this, COLLISION_GROUP_NONE, &trace);

	PerformDamage(&trace);
}

void CPointElectricNail::PerformDamage(trace_t* trace)
{
	CBaseEntity* damageTarget = trace->m_pEnt;
	if (damageTarget)
	{
		// If the player touches the beam then we want to damage them a bit, otherwise dissolve
		if (damageTarget->IsPlayer())
		{
			CTakeDamageInfo info(this, this, 1.0f, DMG_SHOCK);
			AddMultiDamage(info, this);
			damageTarget->DispatchTraceAttack(info, GetAbsOrigin(), trace);
			ApplyMultiDamage();
		}
		else
		{
			if (damageTarget->MyCombatCharacterPointer() != NULL)
			{
				if (!damageTarget->MyCombatCharacterPointer()->CanBecomeRagdoll())
					return;

				damageTarget->MyCombatCharacterPointer()->BecomeRagdollBoogie(this, GetAbsOrigin(), 5.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL);
			}

			// The commented out code below performs the dissolve effect if we hit an NPC. The ragdoll boogie
			// effect looks better so I'm going with that for now but keeping this code around if I change my mind

			/*if (FClassnameIs(damageTarget, "npc_zombie"))
			{
				damageTarget->MyCombatCharacterPointer()->BecomeRagdollBoogie(this, GetAbsOrigin(), 5.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL);
			}*/
			//pOther->MyCombatCharacterPointer()->BecomeRagdollBoogie(this, vecDamageForce, 5.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL);

			/*CTakeDamageInfo info(this, this, 200.0f, DMG_SHOCK);
			AddMultiDamage(info, this);
			damageTarget->DispatchTraceAttack(info, GetAbsOrigin(), trace);
			ApplyMultiDamage();

			DissolveEntity(damageTarget);*/
		}
	}
}

//void CPointElectricNail::DissolveEntity(CBaseEntity* entityToDissolve)
//{
//	if (entityToDissolve->IsEFlagSet(EFL_NO_DISSOLVE) || !entityToDissolve->IsNPC())
//		return;
//
//	dynamic_cast<CRagdollProp*>(entityToDissolve);
//	entityToDissolve->GetBaseAnimating()->Dissolve("", gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
//}

void CPointElectricNail::PickupNail(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Deactivate(true);
	if (pActivator->IsPlayer())
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pActivator;
		pPlayer->GiveAmmo(1, "Nailgun", false);
	}

	UTIL_RemoveImmediate(this);
}