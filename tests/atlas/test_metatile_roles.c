#include "global.h"
#include "atlas_tiles.h"
#include "constants/metatile_behaviors.h"
#include "constants/global.h"
typedef u32 bool32;
#define UNUSED __attribute__((unused))
#include "metatile_behavior.h"
#include "../../src/atlas_tiles_data.c"
#include "../../src/metatile_behavior.c"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    u8 marsh=gAtlasNatureAttributes[900-512]&255;
    assert(marsh==MB_SHORT_GRASS && MetatileBehavior_IsShortGrass(marsh));
    assert(!MetatileBehavior_IsCuttableGrass(marsh) && !MetatileBehavior_IsLandWildEncounter(marsh));
    assert(!MetatileBehavior_IsTallGrass(marsh) && !MetatileBehavior_IsLongGrass(marsh));
    assert(!MetatileBehavior_IsSurfableWaterOrUnderwater(marsh));
    assert(!MetatileBehavior_IsForcedMovementTile(marsh) && !MetatileBehavior_IsRunningDisallowed(marsh));
    for(unsigned role=367;role<369;role++)
    {
        assert(gAtlasPrimaryAttributes[role]==0x0000);
        for(unsigned q=0;q<4;q++)assert((gAtlasPrimaryMetatiles[role*8+q]>>12)==2);
    }
    for(unsigned role=924;role<1018;role++)
    {
        u8 behavior=gAtlasNatureAttributes[role-512]&255;
        assert(behavior==MB_NORMAL && !MetatileBehavior_IsForcedMovementTile(behavior));
    }
    puts("PASS live engine behavior predicates: dry land understory, native foreground plants, short-grass wetland without tall/long-grass, water, forced movement or running restriction");
}
