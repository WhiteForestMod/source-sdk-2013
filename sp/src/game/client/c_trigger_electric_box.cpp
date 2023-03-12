#include "cbase.h"
#include "c_baseentity.h"
#include "fx.h"
#include <bitbuf.h>
#include "utllinkedlist.h"
#include "beam_flags.h"
#include "gamestringpool.h"
#include "iviewrender_beams.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SPRITE_NAME "sprites/bluelight1.vmt";

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

	void DrawBeam(CTriggerElectricBoxTeslaInfo teslaInfo);
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
	FOR_EACH_LL(m_QueuedTeslas, i)
	{
		DrawBeam(m_QueuedTeslas[i]);
	}
	m_QueuedTeslas.Purge();

	SetNextClientThink(CLIENT_THINK_NEVER);
}

void C_TriggerElectricBox::DrawBeam(CTriggerElectricBoxTeslaInfo teslaInfo)
{
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_vecStart = teslaInfo.m_vStart;
	beamInfo.m_vecEnd = teslaInfo.m_vEnd;
	beamInfo.m_pszModelName = SPRITE_NAME;
	beamInfo.m_flHaloScale = 1.0;
	beamInfo.m_flLife = teslaInfo.m_fVisibleTime;
	beamInfo.m_flWidth = teslaInfo.m_fThickness;
	beamInfo.m_flEndWidth = 1;
	beamInfo.m_flFadeLength = 0.3;
	beamInfo.m_flAmplitude = 8;
	beamInfo.m_flBrightness = 200.0;
	beamInfo.m_flSpeed = 0.0;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 1.0;
	beamInfo.m_flRed = teslaInfo.m_vColor.x * 255.0;
	beamInfo.m_flGreen = teslaInfo.m_vColor.y * 255.0;
	beamInfo.m_flBlue = teslaInfo.m_vColor.z * 255.0;
	beamInfo.m_nSegments = 10;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE;
	beamInfo.m_flSpeed = gpGlobals->curtime + 15.5f;

	beams->CreateBeamPoints(beamInfo);
}