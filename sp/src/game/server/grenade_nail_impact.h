//========================  WHITE FOREST  ========================//
//
// Purpose:	These represent an impacted nail fired from a nail grenade
//
//================================================================//

#include "Sprite.h"

#ifndef GRENADE_NAIL_IMPACT_H
#define GRENADE_NAIL_IMPACT_H
#pragma once

class CGrenadeNailImpact : public CBaseAnimating
{
public:
	DECLARE_CLASS(CGrenadeNailImpact, CBaseAnimating);
	DECLARE_DATADESC();

	CGrenadeNailImpact* GrenadeNailImpact_Create(const Vector& position, const QAngle& angles, const Vector& electricitySource);

	void Spawn(void);
	void Precache(void);
	void Thinking(void);
	void SetElectricalSource(const Vector& electricitySource);
	void ShutdownBeam();
	void PerformDamage(trace_t* trace);

	CBeam* CreateElectricalBeam();
	CSprite* CreateElectricalGlow();

private:
	bool m_bBeamActive;
	float m_fNextSparkTime;
	Vector m_vElectricitySource;

	CHandle<CBeam> m_bElectricalBeam;
	CHandle<CSprite> m_sElectricalGlow;
};

struct edict_t;

CGrenadeNailImpact* GrenadeNailImpact_Create(const Vector& position, const QAngle& angles, const Vector& electricitySource);

#endif