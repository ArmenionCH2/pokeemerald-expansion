#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_karst.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    setbuf(stdout,NULL);u32 seed=1290640624u;unsigned absent=0,misses=0;
    assert(AtlasWorld_SetGeneration(23));
    for(unsigned k=0;k<64;k++,seed=seed*1664525u+1013904223u)
    {
        assert(AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));
        unsigned sites=0;struct AtlasKarst karst;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)sites+=AtlasKarst_Site(id,&karst);
        if(!sites){absent++;continue;}
        unsigned status=2;
        for(unsigned batch=0;batch<3 && status==2;batch++)
        {
            AtlasGeo_Begin(ATLAS_GEO_SINKHOLE);
            for(unsigned step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
            status=gAtlasGeoStatus;
        }
        if(status!=1){printf("karst inspector seed=%u sites=%u status=%u\n",seed,sites,status);misses++;}
    }
    printf("Karst search sweep64: %u worlds without suitable sites, %u worlds with no safe inspector landing\n",absent,misses);
    assert(!misses);
}
