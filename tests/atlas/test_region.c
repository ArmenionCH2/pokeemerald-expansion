#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_region.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
bool8 gAtlasPlanReady;
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
static struct AtlasPlanNode before[ATLAS_PLAN_COUNT];
int main(void)
{
    u8 h[9];
    memset(h,140,sizeof(h));assert(AtlasRegion_Classify(h)==ATLAS_REGION_PLAIN);
    memset(h,190,sizeof(h));assert(AtlasRegion_Classify(h)==ATLAS_REGION_PLATEAU);
    h[3]=h[5]=150;assert(AtlasRegion_Classify(h)==ATLAS_REGION_RIDGE);
    memset(h,190,sizeof(h));h[4]=h[1]=h[7]=160;assert(AtlasRegion_Classify(h)==ATLAS_REGION_VALLEY);
    memset(h,170,sizeof(h));h[4]=220;assert(AtlasRegion_Classify(h)==ATLAS_REGION_MOUNTAINS); // isolated peak is not a ridge
    memset(h,190,sizeof(h));h[4]=160;assert(AtlasRegion_Classify(h)!=ATLAS_REGION_VALLEY); // closed pit is not a valley
    static const unsigned axes[4][4]={{3,5,1,7},{1,7,3,5},{0,8,2,6},{2,6,0,8}};
    for(unsigned axis=0;axis<4;axis++)for(int grade=-24;grade<=24;grade+=8)
    {
        memset(h,180,sizeof(h));h[4]=155;
        h[axes[axis][2]]=155+grade;h[axes[axis][3]]=155-grade;
        assert(AtlasRegion_Classify(h)==ATLAS_REGION_VALLEY);
        memset(h,130,sizeof(h));h[4]=155;
        h[axes[axis][2]]=155+grade;h[axes[axis][3]]=155-grade;
        assert(AtlasRegion_Classify(h)==ATLAS_REGION_RIDGE);
    }
    memset(h,190,sizeof(h));h[0]=h[8]=150;assert(AtlasRegion_Classify(h)==ATLAS_REGION_RIDGE); // diagonal range axis
    memset(h,190,sizeof(h));h[0]=230;assert(AtlasRegion_Classify(h)==ATLAS_REGION_MOUNTAINS);
    memset(h,160,sizeof(h));assert(AtlasRegion_Classify(h)==ATLAS_REGION_HILLS);
    h[0]=100;assert(AtlasRegion_Classify(h)==ATLAS_REGION_COAST);
    h[4]=100;assert(AtlasRegion_Classify(h)==ATLAS_REGION_OCEAN);
    assert(AtlasRegion_Classify(NULL)==ATLAS_REGION_UNKNOWN);
    assert(AtlasRegion_At(0,0)==ATLAS_REGION_UNKNOWN);
    for (unsigned i=0;i<ATLAS_PLAN_COUNT;i++) gAtlasPlan[i].height=190;
    memcpy(before,gAtlasPlan,sizeof(before));
    gAtlasPlanReady=TRUE;
    assert(AtlasRegion_At(INT_MIN,INT_MIN)==ATLAS_REGION_UNKNOWN);
    assert(AtlasRegion_At(INT_MIN,-ATLAS_WORLD_HALF_HEIGHT)==ATLAS_REGION_PLATEAU);
    assert(AtlasRegion_At(INT_MAX,ATLAS_WORLD_HALF_HEIGHT-1)==ATLAS_REGION_PLATEAU);
    assert(AtlasRegion_At(-1,0)==AtlasRegion_At(ATLAS_WORLD_WIDTH-1,0));
    assert(!memcmp(before,gAtlasPlan,sizeof(before)));
    puts("PASS contextual regional relief: flat uplands, relative valleys/ridges, coasts, bounded sampling and dateline/poles");
    return 0;
}
