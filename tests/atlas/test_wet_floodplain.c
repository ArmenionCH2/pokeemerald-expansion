#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include "../../src/atlas_place.c"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode prior[ATLAS_PLAN_COUNT];
int main(void)
{
    setbuf(stdout,NULL);const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<6;k++)
    {
        assert(AtlasWorld_SetGeneration(24) && AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));memcpy(prior,gAtlasPlan,sizeof(prior));
        assert(AtlasWorld_SetGeneration(25) && AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));assert(!memcmp(prior,gAtlasPlan,sizeof(prior)));
        for(unsigned type=ATLAS_GEO_BACKSWAMP;type<=ATLAS_GEO_LEVEE;type++)
        {
            unsigned status=2,steps=0;
            for(unsigned batch=0;batch<3 && status==2;batch++)
            {AtlasGeo_Begin(type);for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);status=gAtlasGeoStatus;}
            printf("wet floodplain seed=%u type=%u status=%u steps=%u\n",seeds[k],type,status,steps);assert(status==1);
            s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;struct AtlasCell cell;struct AtlasPlace place;
            AtlasTerrain_Sample(x,y,&cell);assert(!cell.waterform && cell.landform==(type==ATLAS_GEO_LEVEE?ATLAS_LEVEE:ATLAS_BACKSWAMP));
            assert(AtlasPlace_Identify(x,y,&cell,&place) && place.kind==(type==ATLAS_GEO_LEVEE?ATLAS_PLACE_LEVEE:ATLAS_PLACE_BACKSWAMP));
            char name[ATLAS_PLACE_NAME_BYTES];assert(AtlasPlace_Name(&place,2,name) && !strcmp(name,type==ATLAS_GEO_LEVEE?"Natural levee":"Backswamp"));
            for(s32 dy=-1;dy<=1;dy++)for(s32 dx=-1;dx<=1;dx++)assert(!(AtlasTerrain_Tile(x+dx,y+dy)&0xC00));
            if(type==ATLAS_GEO_BACKSWAMP){assert(cell.moisture>=200);if(cell.temperature>=85)assert(cell.biome==ATLAS_WETLAND);}
            puts("PASS actual wet floodplain shape, habitat, safe named inspector and unchanged macro notes");
        }
    }
}
