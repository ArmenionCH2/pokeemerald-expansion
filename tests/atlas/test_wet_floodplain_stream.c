#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
int main(void)
{
    setbuf(stdout,NULL);const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};assert(AtlasWorld_SetGeneration(25));
    for(unsigned k=0;k<6;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));
        for(unsigned type=ATLAS_GEO_BACKSWAMP;type<=ATLAS_GEO_LEVEE;type++)
        {
            unsigned status=2;for(unsigned batch=0;batch<3 && status==2;batch++)
            {AtlasGeo_Begin(type);for(unsigned step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);status=gAtlasGeoStatus;}
            assert(status==1);s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;
            Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
            Walk((AtlasTerrain_FloorDiv(x,4096)+1)*4096-100,y,FALSE);Walk(x,(AtlasTerrain_FloorDiv(y,4096)+1)*4096-100,TRUE);
            printf("PASS generation25 seed=%u type=%u:2560 running-cadence camera steps, cache axes/reversals, no waits and post-walk visible-tile equality\n",seeds[k],type);
        }
    }
}
