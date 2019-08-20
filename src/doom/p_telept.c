//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Teleportation.
//




#include "doomdef.h"
#include "doomstat.h"

#include "s_sound.h"

#include "p_local.h"
#include "m_random.h"


// Data.
#include "sounds.h"

// State.
#include "r_state.h"

// [JSD] uses M_Random() to select a bit number between 0 and 31
static int pt_PickBit(void)
{
    int b = 0;
    // 0x81 is used due to average value of rndtable at 128.8ish instead of 127.5
    // rndtable is not perfectly distributed
    b |= (M_Random() >= 0x81) ? 1<<0 : 0;
    b |= (M_Random() >= 0x81) ? 1<<1 : 0;
    b |= (M_Random() >= 0x81) ? 1<<2 : 0;
    b |= (M_Random() >= 0x81) ? 1<<3 : 0;
    b |= (M_Random() >= 0x81) ? 1<<4 : 0;
    return b;
}

static void P_EnableFizz(mobj_t *thing)
{
    uint32_t j, q, n;

    // [JSD] build up a bitmask where bits are randomly enabled per frame and build on top of the last frame:
    for (q = 0; q < 32; q++)
    {
	thing->telefizz[0][q] = 0; // (1 << pt_PickBit());
    }

    for (j = 1; j < 32; j++)
    {
	for (q = 0; q < 32; q++)
	{
	    uint32_t t = thing->telefizz[j - 1][q];
	    uint32_t k;

	    k = pt_PickBit();

	    n = 0;
	    while (++n <= 17)
	    {
		if ((t & (1 << k)) == 0) break;

		k = pt_PickBit();
	    }

	    if (t & (1<<k))
	    {
		// find a clear bit:
		for (k=0,n=1;k<32;k++,n<<=1)
		{
		    if ((t & n) == 0) break;
		}
	    }

	    t |= (1 << k);
	    thing->telefizz[j][q] = t;
	}
    }
}

// [JSD] think function for MT_TFOG to mimic visuals of object that was teleported
void P_TFogOutThinker(mobj_t *mobj)
{
    mobj_t *target = mobj->target;

    if (mobj->telefizztime < -1)
    {
	mobj->telefizztime++;
    }

#if 0
    // adjust visual properties to match target:
    mobj->angle = (target->angle - mobj->tracer->angle) + target->oldangle;
    mobj->sprite = target->sprite;
    mobj->frame = target->frame;
#endif

    // disabled: causes bugs where monsters can appear to move through walls
#if 0
    // move object relative to where teleported object moved from teleport destination:
    mobj->x = mobj->oldx + (target->x - mobj->tracer->x);
    mobj->y = mobj->oldy + (target->y - mobj->tracer->y);
#endif

    if (mobj->tics > 0)
    {
	mobj->tics--;
    }

    if (!mobj->tics)
	if (!P_SetMobjState (mobj, S_NULL))
	    return;		// freed itself
}

// just delay until sound plays through
void P_TFogInThinker(mobj_t *mobj)
{
    if (mobj->tics > 0)
    {
        mobj->tics--;
    }

    if (!mobj->tics)
	if (!P_SetMobjState (mobj, S_NULL))
	    return;		// freed itself
}

//
// TELEPORTATION
//
int
EV_Teleport
( line_t*	line,
  int		side,
  mobj_t*	thing )
{
    int		i;
    int		tag;
    mobj_t*	m;
    mobj_t*	fog;
    unsigned	an;
    thinker_t*	thinker;
    sector_t*	sector;
    fixed_t	oldx;
    fixed_t	oldy;
    fixed_t	oldz;
    fixed_t	zoffs;
    angle_t oldangle;
    fixed_t	oldmomx;
    fixed_t	oldmomy;

    // don't teleport missiles
    //if (thing->flags & MF_MISSILE)
	//return 0;		

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)		
	return 0;	

    
    tag = line->tag;
    for (i = 0; i < numsectors; i++)
    {
	if (sectors[ i ].tag == tag )
	{
	    thinker = thinkercap.next;
	    for (thinker = thinkercap.next;
		 thinker != &thinkercap;
		 thinker = thinker->next)
	    {
		// not a mobj
		if (thinker->function.acp1 != (actionf_p1)P_MobjThinker)
		    continue;	

		m = (mobj_t *)thinker;
		
		// not a teleportman
		if (m->type != MT_TELEPORTMAN )
		    continue;		

		sector = m->subsector->sector;
		// wrong sector
		if (sector-sectors != i )
		    continue;	

		oldx = thing->x;
		oldy = thing->y;
		oldz = thing->z;
		oldangle = thing->angle;
		oldmomx = thing->momx;
		oldmomy = thing->momy;

		if (!P_TeleportMove (thing, m->x, m->y))
		    return 0;

                // The first Final Doom executable does not set thing->z
                // when teleporting. This quirk is unique to this
                // particular version; the later version included in
                // some versions of the Id Anthology fixed this.

                if (gameversion != exe_final)
		    thing->z = thing->floorz;

		if (thing->player)
		{
		    thing->player->viewz = thing->z+thing->player->viewheight;
		    // [crispy] center view after teleporting
		    thing->player->centering = true;
		}

		// don't move for a bit
		if (thing->player)
		    thing->reactiontime = 18;	

		// [JSD] enable fizz effect:
		thing->telefizztime = 32;
		P_EnableFizz(thing);

		thing->angle = m->angle;
		if (thing->flags & MF_MISSILE)
		{
		    // determine rotation angle from previous angle to new angle:
		    angle_t rotation = m->angle - oldangle;
		    fixed_t s = finesine[rotation>>ANGLETOFINESHIFT];
		    fixed_t c = finecosine[rotation>>ANGLETOFINESHIFT];
		    // move missile up above floorz to the height missiles are normally spawned at above the floor:
		    thing->z += 4*8*FRACUNIT;
		    // rotate momentum to the new angle:
		    thing->momx = FixedMul(oldmomx, c) - FixedMul(oldmomy, s);
		    thing->momy = FixedMul(oldmomy, c) + FixedMul(oldmomx, s);
		    // kill Z momentum so missiles don't fly upwards or downwards out of teleporter:
		    thing->momz = 0;
		    // spawn teleport fog up off the floor:
		    zoffs = -4*8*FRACUNIT;
		}
		else
		{
		    // kill momentum:
		    thing->momx = thing->momy = thing->momz = 0;
		    zoffs = 0;
		}

		// spawn teleport fog at source and destination
		fog = P_SpawnMobj (oldx, oldy, oldz, MT_TFOG);
		// customize fog object to be a visual clone of object that is teleporting
		fog->target = thing;
		fog->tracer = m; // record teleport target mobj
		fog->angle = oldangle;
		fog->oldangle = oldangle;
		fog->sprite = thing->sprite;
		fog->frame = thing->frame;
		fog->flags &= ~MF_TRANSLUCENT;
		// enough tics to let sound play out
		fog->tics = 64;
		// fizzle out effect using same fizzle pattern as object that is teleporting
		fog->telefizztime = -32;
		memcpy(fog->telefizz, thing->telefizz, sizeof(thing->telefizz));
		fog->thinker.function.acp1 = (actionf_p1)P_TFogOutThinker;
		S_StartSound (fog, sfx_telept);

		an = m->angle >> ANGLETOFINESHIFT;
		fog = P_SpawnMobj (m->x+20*finecosine[an], m->y+20*finesine[an]
			       , thing->z + zoffs, MT_TFOG);
		// enough tics to let sound play out
		fog->tics = 64;
		fog->sprite = SPR_TNT1;
		fog->thinker.function.acp1 = (actionf_p1)P_TFogInThinker;
		// emit sound, where?
		S_StartSound (fog, sfx_telept);

		return 1;
	    }	
	}
    }
    return 0;
}

