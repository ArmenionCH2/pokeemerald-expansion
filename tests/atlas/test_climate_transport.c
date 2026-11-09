#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
static u8 heights[ATLAS_PLAN_COUNT],original[ATLAS_PLAN_COUNT][2];
static void Climate(void)
{
    assert(AtlasPlan_Begin(123));assert(AtlasPlan_ImportHeights(heights));
    while (AtlasPlan_Stage()<3) AtlasPlan_Step(1);
}
int main(void)
{
    assert(AtlasWorld_SetGeneration(6));
    for (unsigned shift=0;shift<ATLAS_PLAN_WIDTH;shift++)
    {
        for (unsigned row=0;row<ATLAS_PLAN_HEIGHT;row++)
            for (unsigned x=0;x<ATLAS_PLAN_WIDTH;x++)
            {
                unsigned source=(x+ATLAS_PLAN_WIDTH-shift)%ATLAS_PLAN_WIDTH;
                heights[row*ATLAS_PLAN_WIDTH+x]=row%3==0?140+(source%13)*6
                    :source<12?90:source<30?140:source<34?220:140;
            }
        Climate();
        for (unsigned row=0;row<ATLAS_PLAN_HEIGHT;row++)
            for (unsigned x=0;x<ATLAS_PLAN_WIDTH;x++)
            {
                unsigned id=row*ATLAS_PLAN_WIDTH+x;
                unsigned source=row*ATLAS_PLAN_WIDTH+(x+ATLAS_PLAN_WIDTH-shift)%ATLAS_PLAN_WIDTH;
                if (!shift) {original[id][0]=gAtlasPlan[id].humidity;original[id][1]=gAtlasPlan[id].rain;}
                else {assert(original[source][0]==gAtlasPlan[id].humidity);assert(original[source][1]==gAtlasPlan[id].rain);}
            }
    }
    puts("PASS macro climate invariant under all 96 longitude rotations: ocean rows, mountain barriers and land-only rows");
    return 0;
}
