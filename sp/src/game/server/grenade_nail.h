//========================  WHITE FOREST  ========================//
//
// Purpose:	A crude nailbomb that discharges electricity to impacted nails
//
//================================================================//

#ifndef GRENADE_NAIL_H
#define GRENADE_NAIL_H
#pragma once

class CBaseGrenade;
struct edict_t;

CBaseGrenade* NailGrenade_Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner);

#endif // GRENADE_NAIL_H
