#include "cbase.h"
#include "c_baseentity.h"
#include "fx.h"
#include <bitbuf.h>
#include "utllinkedlist.h"
#include "beam_flags.h"
#include "gamestringpool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SPRITE_NAME "sprites/physbeam.vmt";

// Simple class to hold the received message
class CTriggerElectricBoxTeslaInfo
{
public:
	Vector m_vStart;
	Vector m_vEnd;
	Vector m_vColor;
	int m_iEntIndex;
	float m_fAlpha;
	float m_fThickness;
	float m_fVisibleTime;
};

// Actual client implementation
class C_TriggerElectricBox : public C_BaseEntity
{
public:
	DECLARE_CLASS(C_TriggerElectricBox, C_BaseEntity);
	DECLARE_CLIENTCLASS();

	C_TriggerElectricBox();

	virtual void ReceiveMessage(int classID, bf_read& msg);
	virtual void ClientThink();

	bool m_bIsActive;

protected:
	CUtlLinkedList<CTriggerElectricBoxTeslaInfo, int> m_QueuedTeslas;
};

IMPLEMENT_CLIENTCLASS_DT(C_TriggerElectricBox, DT_TriggerElectricBox, CTriggerElectricBox)
	RecvPropBool( RECVINFO(m_bIsActive) )
END_RECV_TABLE()


C_TriggerElectricBox::C_TriggerElectricBox()
{
}

void C_TriggerElectricBox::ReceiveMessage(int classID, bf_read& msg)
{
	CTriggerElectricBoxTeslaInfo teslaInfo;

	// Read the start point
	msg.ReadBitVec3Coord(teslaInfo.m_vStart);

	// Read the end point
	msg.ReadBitVec3Coord(teslaInfo.m_vEnd);

	// Read the entity index
	teslaInfo.m_iEntIndex = msg.ReadShort();

	// Read the color information
	teslaInfo.m_vColor.x = ((unsigned char)msg.ReadChar()) / 255.0f;
	teslaInfo.m_vColor.y = ((unsigned char)msg.ReadChar()) / 255.0f;
	teslaInfo.m_vColor.z = ((unsigned char)msg.ReadChar()) / 255.0f;

	float flAlpha = 0;
	flAlpha = ((unsigned char)msg.ReadChar()) / 255.0f;

	// Read the thickness
	teslaInfo.m_fThickness = msg.ReadFloat();

	// Read the visible time
	teslaInfo.m_fVisibleTime = msg.ReadFloat();

	// Add the info to the queue for processing by the think logic
	m_QueuedTeslas.AddToTail(teslaInfo);

	SetNextClientThink(CLIENT_THINK_ALWAYS);
}

void C_TriggerElectricBox::ClientThink()
{
	// TODO: Replace FX_BuildTesla with something that doesn't look like trash

	FOR_EACH_LL(m_QueuedTeslas, i)
	{
		C_BaseEntity* pEntity = ClientEntityList().GetEnt(m_QueuedTeslas[i].m_iEntIndex);	
		FX_BuildTesla(pEntity, m_QueuedTeslas[i].m_vStart, m_QueuedTeslas[i].m_vEnd, "sprites/physbeam.vmt", m_QueuedTeslas[i].m_fThickness, m_QueuedTeslas[i].m_vColor, FBEAM_ONLYNOISEONCE, m_QueuedTeslas[i].m_fVisibleTime);

	}
	m_QueuedTeslas.Purge();

	SetNextClientThink(CLIENT_THINK_NEVER);
}