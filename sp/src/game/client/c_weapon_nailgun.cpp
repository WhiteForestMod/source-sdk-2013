#include "cbase.h"
#include "model_types.h"
#include "clienteffectprecachesystem.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "beamdraw.h"

//
// Nail - Fired from the nailgun
//

class C_Nail : public C_BaseCombatCharacter
{
	DECLARE_CLASS(C_Nail, C_BaseCombatCharacter);
	DECLARE_CLIENTCLASS();

public:

	C_Nail(void);

	virtual RenderGroup_t GetRenderGroup(void)
	{
		// We want to draw translucent bits as well as our main model
		return RENDER_GROUP_TWOPASS;
	}
};

IMPLEMENT_CLIENTCLASS_DT(C_Nail, DT_Nail, CNail)
END_RECV_TABLE()

C_Nail::C_Nail(void)
{

}