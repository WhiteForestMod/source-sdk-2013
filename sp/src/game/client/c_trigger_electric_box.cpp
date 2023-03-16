#include "cbase.h"
#include "c_baseentity.h"
#include "fx.h"
#include <bitbuf.h>
#include "utllinkedlist.h"
#include "beam_flags.h"
#include "gamestringpool.h"
#include "iviewrender_beams.h"
#include "IEffects.h"
#include "view.h"

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
	void DrawBeamPoint(Vector beamPoint, float visibleTime);
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
	//m_QueuedTeslas.AddToTail(teslaInfo);

	// Disabled for now, will see if directly drawing the beams is a good idea
	DrawBeam(teslaInfo);

	//SetNextClientThink(CLIENT_THINK_ALWAYS);
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
	DrawBeamPoint(teslaInfo.m_vStart, teslaInfo.m_fVisibleTime);
	DrawBeamPoint(teslaInfo.m_vEnd, teslaInfo.m_fVisibleTime);
}

void C_TriggerElectricBox::DrawBeamPoint(Vector beamPoint, float visibleTime)
{
	Vector vecFlash = beamPoint;
	Vector vecForward;
	AngleVectors(MainViewAngles(), &vecForward);
	vecFlash -= (vecForward);

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create("dust");
	pSimple->SetSortOrigin(vecFlash);
	SimpleParticle* pParticle;
	pParticle = (SimpleParticle*)pSimple->AddParticle(sizeof(SimpleParticle), pSimple->GetPMaterial("effects/tesla_glow_noz"), vecFlash);
	if (pParticle != NULL)
	{
		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime = visibleTime;
		pParticle->m_vecVelocity = vec3_origin;
		Vector color(1, 1, 1);
		float  colorRamp = RandomFloat(0.75f, 1.25f);
		pParticle->m_uchColor[0] = MIN(1.0f, color[0] * colorRamp) * 255.0f;
		pParticle->m_uchColor[1] = MIN(1.0f, color[1] * colorRamp) * 255.0f;
		pParticle->m_uchColor[2] = MIN(1.0f, color[2] * colorRamp) * 255.0f;
		pParticle->m_uchStartSize = RandomFloat(2, 4);
		pParticle->m_uchEndSize = pParticle->m_uchStartSize - 2;
		pParticle->m_uchStartAlpha = 255;
		pParticle->m_uchEndAlpha = 10;
		pParticle->m_flRoll = RandomFloat(0, 360);
		pParticle->m_flRollDelta = 0;
	}
}