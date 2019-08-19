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



//
// TELEPORTATION
//
int
EV_Teleport
( line_t*	line,
  int		side,
  mobj_t*	thing )
{
    int		i, j, q;
    int		tag;
    mobj_t*	m;
    mobj_t*	fog;
    unsigned	an;
    thinker_t*	thinker;
    sector_t*	sector;
    fixed_t	oldx;
    fixed_t	oldy;
    fixed_t	oldz;
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
		// [JSD] build up a bitmask where bits are randomly enabled per frame and build on top of the last frame:
		thing->telefizz[0] = 0;
		for (j = 1; j < 32; j++) {
		    uint32_t t = thing->telefizz[j-1];
		    int k = P_Random() & 31;
		    q = 0;
		    while ((t & (1<<k)) == 0) {
			k = P_Random() & 31;
			if (q++ >= 5) break;
		    }
		    t |= (1<<k);
		    thing->telefizz[j] = t;
		}

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

		    // spawn teleport fog at source and destination
		    fog = P_SpawnMobj (oldx, oldy, oldz - 4*8*FRACUNIT, MT_TFOG);
		    S_StartSound (fog, sfx_telept);
		    an = m->angle >> ANGLETOFINESHIFT;
		    fog = P_SpawnMobj (m->x+20*finecosine[an], m->y+20*finesine[an]
				   , thing->z - 4*8*FRACUNIT, MT_TFOG);
		}
		else
		{
		    // kill momentum:
		    thing->momx = thing->momy = thing->momz = 0;

		    // spawn teleport fog at source and destination
		    fog = P_SpawnMobj (oldx, oldy, oldz, MT_TFOG);
		    S_StartSound (fog, sfx_telept);
		    an = m->angle >> ANGLETOFINESHIFT;
		    fog = P_SpawnMobj (m->x+20*finecosine[an], m->y+20*finesine[an]
				   , thing->z, MT_TFOG);

		}

		// emit sound, where?
		S_StartSound (fog, sfx_telept);

		return 1;
	    }	
	}
    }
    return 0;
}

