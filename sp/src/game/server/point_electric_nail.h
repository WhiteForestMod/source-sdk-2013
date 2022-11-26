//========================  WHITE FOREST  ========================//
//
// Electric Nails - Created when a nail fired by the nailgun impacts a surface
//
//================================================================//

#include "Sprite.h"
#include "beam_shared.h"

#ifndef POINT_ELECTRIC_NAIL_H
#define POINT_ELECTRIC_NAIL_H
#pragma once

class CPointElectricNail : public CBaseAnimating
{
public:
	DECLARE_CLASS(CPointElectricNail, CPointEntity);
	DECLARE_DATADESC();

	bool m_bIsActive;
	bool m_bShouldNotSearchForChildren;

	CPointElectricNail* m_CFirstChild;
	CSoundPatch* m_pElectricitySound;

	CPointElectricNail();

	virtual void Precache(void);
	virtual void Spawn();
	virtual int	ObjectCaps() { return (FCAP_IMPULSE_USE); }

	void SpawnAtPosition(const Vector& position);
	void SpawnAtPositionNotInteracable(const Vector& position);

	void InputActivate(inputdata_t& inputData);
	void Activate(bool shouldFireOutput);
	void InputDeactivate(inputdata_t& inputData);
	void Deactivate(bool shouldFireOutput);

	void NailThink();
	void PickupNail(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	COutputEvent m_OnActivated;
	COutputEvent m_OnDeactivated;

private:
	float m_flNextSparkTime;

	CHandle<CSprite> m_CGlowSprite;
	CHandle<CBeam> m_CFirstChildBeam;

	CBeam* DrawBeamForChild(CPointElectricNail* child);
	void CalculateDamage(CBeam* beam, CPointElectricNail* child);
	void PerformDamage(trace_t* ptr);
	void DissolveEntity(CBaseEntity* entityToDissolve);
};

#endif