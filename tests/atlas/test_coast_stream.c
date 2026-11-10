#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
int main(void)
{
    setbuf(stdout,NULL);u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    assert(AtlasWorld_SetGeneration(22));
    for(unsigned k=0;k<6;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));
        AtlasGeo_Begin(ATLAS_GEO_SEA_CLIFF);
        for(unsigned step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
        assert(gAtlasGeoStatus==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;
        Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
        Walk((AtlasTerrain_FloorDiv(x,4096)+1)*4096-100,y,FALSE);
        Walk(x,(AtlasTerrain_FloorDiv(y,4096)+1)*4096-100,TRUE);
        printf("PASS generation22 seed=%u: 2560 streaming-camera steps at running cadence across cliffs and both water-cache axes; no waits, correct post-walk visible tiles\n",seeds[k]);
    }
}
