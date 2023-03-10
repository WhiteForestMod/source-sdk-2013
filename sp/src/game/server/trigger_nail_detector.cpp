#include "cbase.h"
#include "triggers.h"
#include "utllinkedlist.h"
#include "point_electric_nail.h"


class CTriggerNailDetector : public CBaseTrigger
{
public:
	DECLARE_CLASS(CTriggerNailDetector, CBaseTrigger);
	DECLARE_DATADESC();

	void Spawn();
	void DetectorThink();

	bool m_bIsActive;

	COutputEvent m_OnActivated;
	COutputEvent m_OnDeactivated;

protected:
	bool SearchForNails();
	void UpdateState(bool activeNailInVolume);
};

LINK_ENTITY_TO_CLASS(trigger_nail_detector, CTriggerNailDetector);

BEGIN_DATADESC(CTriggerNailDetector)
	DEFINE_ENTITYFUNC(StartTouch),
	DEFINE_OUTPUT(m_OnActivated, "OnActivated"),
	DEFINE_OUTPUT(m_OnDeactivated, "OnDeactivated"),
END_DATADESC()

void CTriggerNailDetector::Spawn()
{
	InitTrigger();

	m_bIsActive = false;

	SetThink(&CTriggerNailDetector::DetectorThink);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CTriggerNailDetector::DetectorThink()
{
	bool activeNailInVolument = SearchForNails();
	UpdateState(activeNailInVolument);
	
	SetNextThink(gpGlobals->curtime + 0.1f);
}

bool CTriggerNailDetector::SearchForNails()
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "point_electric_nail")) != NULL)
	{
		if (CollisionProp()->IsPointInBounds(pEntity->GetAbsOrigin()))
		{
			CPointElectricNail* nailInVolume = dynamic_cast<CPointElectricNail*> (pEntity);
			if (nailInVolume->m_bIsActive)
			{
				return true;
			}
		}
	}

	return false;
}

void CTriggerNailDetector::UpdateState(bool activeNailInVolume)
{
	if (!m_bIsActive && activeNailInVolume)
	{
		m_bIsActive = true;
		m_OnActivated.FireOutput(this, this);

		return;
	}

	if (m_bIsActive && !activeNailInVolume)
	{
		m_bIsActive = false;
		m_OnDeactivated.FireOutput(this, this);

		return;
	}
}