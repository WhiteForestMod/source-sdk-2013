#include "cbase.h"
#include "triggers.h"
#include "utllinkedlist.h"
#include "point_electric_nail.h"


class CTriggerElectricBox : public CBaseTrigger
{
public:
	DECLARE_CLASS(CTriggerElectricBox, CBaseTrigger);
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CTriggerElectricBox();

	void Spawn();
	void DetectorThink();
	virtual void Precache();

	// always send to all clients
	virtual int UpdateTransmitState()	
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

	CNetworkVar(bool, m_bIsActive);

	// How many arcs we should create at once
	int m_iFrequency;

	// The colour of the arcs
	color32 m_Color;

	// Time between Tesla arcs (min / max)
	float m_flArcInterval[2];

	// Beam thickness (min / max)
	float m_flThickness[2];

	// How long each beam stays around (min / max)
	float m_flTimeVisible[2];

	COutputEvent m_OnActivated;
	COutputEvent m_OnDeactivated;

protected:
	bool SearchForNails();
	void UpdateState(bool activeNailInVolume);
	void TransmitArc();

	float m_fNextTeslaTime;
};

LINK_ENTITY_TO_CLASS(trigger_electric_box, CTriggerElectricBox);

BEGIN_DATADESC(CTriggerElectricBox)

DEFINE_KEYFIELD(m_Color, FIELD_COLOR32, "Color"),

DEFINE_KEYFIELD(m_iFrequency, FIELD_INTEGER, "frequency"),

DEFINE_KEYFIELD(m_flThickness[0], FIELD_FLOAT, "thick_min"),
DEFINE_KEYFIELD(m_flThickness[1], FIELD_FLOAT, "thick_max"),

DEFINE_KEYFIELD(m_flTimeVisible[0], FIELD_FLOAT, "lifetime_min"),
DEFINE_KEYFIELD(m_flTimeVisible[1], FIELD_FLOAT, "lifetime_max"),

DEFINE_KEYFIELD(m_flArcInterval[0], FIELD_FLOAT, "interval_min"),
DEFINE_KEYFIELD(m_flArcInterval[1], FIELD_FLOAT, "interval_max"),


DEFINE_OUTPUT(m_OnActivated, "OnActivated"),
DEFINE_OUTPUT(m_OnDeactivated, "OnDeactivated"),

DEFINE_FUNCTION(DetectorThink),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CTriggerElectricBox, DT_TriggerElectricBox)
	SendPropBool( SENDINFO(m_bIsActive) ),
END_SEND_TABLE()

CTriggerElectricBox::CTriggerElectricBox()
{
}

void CTriggerElectricBox::Precache()
{
	PrecacheModel("sprites/bluelight1.vmt");
	BaseClass::Precache();
}

void CTriggerElectricBox::Spawn()
{
	Precache();
	InitTrigger();
	// AddEFlags(EFL_FORCE_CHECK_TRANSMIT);

	m_bIsActive = false;

	SetThink(&CTriggerElectricBox::DetectorThink);
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CTriggerElectricBox::DetectorThink()
{
	bool activeNailInVolument = SearchForNails();
	UpdateState(activeNailInVolument);

	if (m_bIsActive && (m_fNextTeslaTime < gpGlobals->curtime))
	{
		for (int i = 0; i < m_iFrequency; i++)
		{
			TransmitArc();
		}

		m_fNextTeslaTime = (gpGlobals->curtime + RandomFloat(m_flArcInterval[0], m_flArcInterval[1]));
	}
	
	SetNextThink(gpGlobals->curtime + 0.1f);
}

bool CTriggerElectricBox::SearchForNails()
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

void CTriggerElectricBox::UpdateState(bool activeNailInVolume)
{
	if (!m_bIsActive && activeNailInVolume)
	{
		m_bIsActive = true;
		m_OnActivated.FireOutput(this, this);
		m_fNextTeslaTime = gpGlobals->curtime;

		return;
	}

	if (m_bIsActive && !activeNailInVolume)
	{
		m_bIsActive = false;
		m_OnDeactivated.FireOutput(this, this);

		return;
	}
}

void CTriggerElectricBox::TransmitArc()
{
	// Choose a random start point
	Vector m_arcStartPoint;
	CollisionProp()->RandomPointInBounds(Vector(0, 0, 0), Vector(1, 1, 1), &m_arcStartPoint);

	// Choose a random end point
	Vector m_arcEndPoint;
	CollisionProp()->RandomPointInBounds(Vector(0, 0, 0), Vector(1, 1, 1), &m_arcEndPoint);

	// TEMP HACK; Set z = 1 to make testing obvious
	m_arcStartPoint.z = 1;
	m_arcEndPoint.z = 1;

	// Grab our entity index
	int entityIndex = entindex();

	// Calculate how thick the arc should be
	float thickness = RandomFloat(m_flThickness[0], m_flThickness[1]);

	// Calculate how long the arc should be visible for
	float visibleTime = RandomFloat(m_flTimeVisible[0], m_flTimeVisible[1]);

	// Transmit all the info we need
	EntityMessageBegin(this);
		WRITE_VEC3COORD(m_arcStartPoint);
		WRITE_VEC3COORD(m_arcEndPoint);
		WRITE_SHORT(entityIndex);
		WRITE_BYTE(m_Color.r);
		WRITE_BYTE(m_Color.g);
		WRITE_BYTE(m_Color.b);
		WRITE_BYTE(m_Color.a);
		WRITE_FLOAT(thickness);
		WRITE_FLOAT(visibleTime);
	MessageEnd();
}