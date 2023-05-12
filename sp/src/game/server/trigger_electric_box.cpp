#include "cbase.h"
#include "triggers.h"
#include "utllinkedlist.h"
#include "point_electric_nail.h"

#define FL_NO_ARC 1<<0

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

	// Z offset of the arc
	float m_flZOffet;

	// Distance between arc start & end (min / max)
	float m_flArcDistance[2];

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

DEFINE_KEYFIELD(m_flZOffet, FIELD_FLOAT, "z_offset"),

DEFINE_KEYFIELD(m_flArcDistance[0], FIELD_FLOAT, "dist_min"),
DEFINE_KEYFIELD(m_flArcDistance[1], FIELD_FLOAT, "dist_max"),

DEFINE_KEYFIELD(m_flThickness[0], FIELD_FLOAT, "thick_min"),
DEFINE_KEYFIELD(m_flThickness[1], FIELD_FLOAT, "thick_max"),

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
	PrecacheModel("effects/tesla_glow_noz");
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
	if (HasSpawnFlags(FL_NO_ARC))
		return;

	// Choose a random start point & compute end point
	Vector arcStartPoint;
	Vector arcEndPoint;
	
	CCollisionProperty *collisionProperty = CollisionProp();
	collisionProperty->RandomPointInBounds(Vector(0, 0, 0), Vector(1, 1, 0), &arcStartPoint);
	float arcDistance = RandomFloat(m_flArcDistance[0], m_flArcDistance[1]);

	int tries = 3;
	do
	{
		// Shout out to chatGPT for helping me with the maths
		float theta = RandomFloat(0, 2 * M_PI);
		float x = arcDistance * cosf(theta);
		float y = arcDistance * sinf(theta);

		arcEndPoint.z = arcStartPoint.z;
		arcEndPoint.x = arcStartPoint.x + x;
		arcEndPoint.y = arcStartPoint.y + y;

		if (collisionProperty->IsPointInBounds(arcEndPoint))
		{
			break;
		}

		tries--;
		if (tries == 0)
		{
			// Give up, we must have selected a poor start position / angle
			return;
		}

	} while (tries > 0);

	// Apply our z offset (we don't do it in the loop as the specified offset might put us outside the bbox)
	arcStartPoint.z = arcStartPoint.z + m_flZOffet;
	arcEndPoint.z = arcEndPoint.z + m_flZOffet;

	// Grab our entity index
	int entityIndex = entindex();

	// Calculate how thick the arc should be
	float thickness = RandomFloat(m_flThickness[0], m_flThickness[1]);

	// Calculate how long the arc should be visible for
	float visibleTime = RandomFloat(m_flTimeVisible[0], m_flTimeVisible[1]);

	// Transmit all the info we need
	EntityMessageBegin(this);
		WRITE_VEC3COORD(arcStartPoint);
		WRITE_VEC3COORD(arcEndPoint);
		WRITE_SHORT(entityIndex);
		WRITE_BYTE(m_Color.r);
		WRITE_BYTE(m_Color.g);
		WRITE_BYTE(m_Color.b);
		WRITE_BYTE(m_Color.a);
		WRITE_FLOAT(thickness);
		WRITE_FLOAT(visibleTime);
	MessageEnd();
}