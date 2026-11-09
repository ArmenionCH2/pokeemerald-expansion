#include "global.h"
#include "atlas_plan.h"
#include "atlas_climate.h"

static s32 Latitude(u16 row)
{
    s32 distance=(s32)row*2-(ATLAS_PLAN_HEIGHT-1);
    if (distance<0) distance=-distance;
    return distance*90/(ATLAS_PLAN_HEIGHT-1);
}
u8 AtlasClimate_Upwind(u16 row)
{
    s32 latitude=Latitude(row);
    // Trade winds and polar easterlies; mid-latitude westerlies.
    return latitude>=30 && latitude<60?1:0;
}
void AtlasClimate_Transport(u16 row,u8 height,u8 upstreamHeight,u8 upstreamVapor,
    bool8 dryRow,u8 *vapor,u8 *rain)
{
    s32 latitude=Latitude(row),rise=(s32)height-upstreamHeight;
    s32 supply,precipitation,belt;
    if (dryRow) upstreamVapor=12;
    if (height<ATLAS_PLAN_SEA) supply=230-latitude*140/90;
    else
    {
        supply=(s32)upstreamVapor-4;
        if (rise>0) supply-=rise/2; // windward condensation removes lee-side vapor
        if (supply<12 || dryRow) supply=12;
    }
    // Smooth circulation envelopes avoid abrupt horizontal biome stripes.
    // Equatorial ascent, subtropical subsidence, storm belt and polar dryness.
    belt=latitude<30?60-latitude*105/30
        :latitude<55?-45+(latitude-30)*65/25
        :20-(latitude-55)*50/35;
    precipitation=35+supply/2+belt*supply/230;
    if (height>=ATLAS_PLAN_SEA && rise>0) precipitation+=rise*upstreamVapor/230;
    *vapor=supply;
    *rain=precipitation<0?0:precipitation>255?255:precipitation;
}
