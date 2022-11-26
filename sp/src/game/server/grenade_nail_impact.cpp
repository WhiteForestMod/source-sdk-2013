//========================  WHITE FOREST  ========================//
//
// Purpose:	These represent an impacted nail fired from a nail grenade
//
//================================================================//

#include "cbase.h"
#include "gamerules.h"
#include "beam_shared.h"
#include "grenade_nail_impact.h"
#include "IEffects.h"
#include "RagdollBoogie.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Begin Macros

#define GRENADE_BEAM_SPRITE			"sprites/bluelight1.vmt"
#define GRENADE_GLOW_SPRITE			"sprites/blueglow1.vmt"

#define GRENADE_IMPACT_MODEL		"models/props_mining/railroad_spike01.mdl"


LINK_ENTITY_TO_CLASS(point_grenade_nail_impact, CGrenadeNailImpact);

BEGIN_DATADESC(CGrenadeNailImpact)
DEFINE_FIELD(m_bElectricalBeam, FIELD_EHANDLE),
DEFINE_FIELD(m_sElectricalGlow, FIELD_EHANDLE),
DEFINE_FUNCTION(Thinking),
END_DATADESC()


CGrenadeNailImpact* GrenadeNailImpact_Create(const Vector& position, const QAngle& angles, const Vector& electricitySource)
{
	CGrenadeNailImpact* impact = (CGrenadeNailImpact*)CreateEntityByName("point_grenade_nail_impact");
	impact->SetAbsOrigin(position);
	impact->SetAbsAngles(angles);
	impact->SetElectricalSource(electricitySource);
	impact->Spawn();

	return impact;
}

void CGrenadeNailImpact::Spawn(void)
{
	Precache();

	SetModel(GRENADE_IMPACT_MODEL);
	SetSolid(SOLID_NONE);

	m_bBeamActive = true;
	m_fNextSparkTime = gpGlobals->curtime + random->RandomFloat(0.5, 1.5);

	EmitSound( "Roller.Impact" );

	SetThink(&CGrenadeNailImpact::Thinking);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CGrenadeNailImpact::Precache(void)
{
	PrecacheModel(GRENADE_IMPACT_MODEL);
	PrecacheModel(GRENADE_BEAM_SPRITE);
	PrecacheModel(GRENADE_GLOW_SPRITE);

	PrecacheScriptSound("Roller.Impact");
}

void CGrenadeNailImpact::Thinking(void)
{
	// - Draw our sprite and beam if not done already
	// - Check to see if anyone touched our beam & act appropriatly

	if (m_bElectricalBeam == NULL && m_bBeamActive)
	{
		m_bElectricalBeam = CreateElectricalBeam();
	}

	if (m_sElectricalGlow == NULL && m_bBeamActive)
	{
		m_sElectricalGlow = CreateElectricalGlow();
	}

	if (gpGlobals->curtime > m_fNextSparkTime && m_bBeamActive)
	{
		m_fNextSparkTime = gpGlobals->curtime + random->RandomFloat(0.5, 1.5);
		g_pEffects->Sparks(GetAbsOrigin());
	}

	if (m_bBeamActive)
	{
		trace_t trace;
		UTIL_TraceLine(GetAbsOrigin(), m_vElectricitySource, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace);

		PerformDamage(&trace);
	}

	SetThink(&CGrenadeNailImpact::Thinking);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CGrenadeNailImpact::PerformDamage(trace_t* trace)
{
	CBaseEntity* damageTarget = trace->m_pEnt;
	if (damageTarget)
	{
		// If the player touches the beam then we want to damage them a bit, otherwise ragdoll
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
		}
	}
}

void CGrenadeNailImpact::SetElectricalSource(const Vector& electricitySource)
{
	m_vElectricitySource = electricitySource;
}

void CGrenadeNailImpact::ShutdownBeam()
{
	m_bBeamActive = false;

	if (m_bElectricalBeam)
	{
		UTIL_Remove(m_bElectricalBeam);
	}

	if (m_sElectricalGlow)
	{
		UTIL_Remove(m_sElectricalGlow);
	}
}

CBeam* CGrenadeNailImpact::CreateElectricalBeam()
{
	CBeam* beam = CBeam::BeamCreate(GRENADE_BEAM_SPRITE, 2.0f);
	beam->SetStartPos(GetAbsOrigin());
	beam->PointEntInit(m_vElectricitySource, this);
	beam->SetBrightness(255);
	beam->SetNoise(3.0f);

	float scrollOffset = gpGlobals->curtime + 15.5;
	beam->SetScrollRate(scrollOffset);

	return beam;
}

CSprite* CGrenadeNailImpact::CreateElectricalGlow()
{
	CSprite* sprite = CSprite::SpriteCreate(GRENADE_GLOW_SPRITE, GetLocalOrigin(), false);
	sprite->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxPulseFast);
	sprite->SetScale(0.1f);

	return sprite;
}