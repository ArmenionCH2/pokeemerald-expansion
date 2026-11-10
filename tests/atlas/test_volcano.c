#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_volcano.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include "../../src/atlas_place.c"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode old[ATLAS_PLAN_COUNT];
static void Generate(u32 seed,u16 version)
{
    assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));
    while(!AtlasPlan_Step(256));
}
int main(void)
{
    setbuf(stdout,NULL);
    u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<sizeof(seeds)/sizeof(seeds[0]);k++)
    {
        Generate(seeds[k],12);memcpy(old,gAtlasPlan,sizeof(old));
        struct AtlasVolcano v,a,b;assert(!AtlasVolcano_Site(0,&v));
        Generate(seeds[k],13);assert(!memcmp(old,gAtlasPlan,sizeof(old)));
        unsigned sites=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(!AtlasVolcano_Site(id,&v))continue;
            sites++;
            assert(AtlasVolcano_Sample(v.x,v.y,&a) && a.distance==0);
            assert(AtlasVolcano_Sample(v.x,v.y+v.radius/4,&b));
            assert(b.uplift==a.uplift+24 && (b.relief&0x70)==0x30);
            assert(!AtlasVolcano_Sample(v.x+v.radius,v.y,&b));
            /* Reset the cache and query from another owner, then replay. */
            AtlasVolcano_Sample(v.x+ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH,v.y,&b);
            assert(AtlasVolcano_Sample(v.x+ATLAS_WORLD_WIDTH,v.y,&b));
            assert(!memcmp(&a,&b,sizeof(a)));
            struct AtlasPlanSample plan;struct AtlasCell centre,rim;
            AtlasTerrain_Sample(v.x,v.y,&centre);
            AtlasTerrain_Sample(v.x,v.y+v.radius/4,&rim);
            assert(!centre.waterform && !rim.waterform && rim.elevation>centre.elevation+18);
            assert(AtlasTerrain_GroundHeight(v.x,v.y,&plan)==centre.elevation);
            assert(AtlasTerrain_GroundHeight(v.x,v.y+v.radius/4,&plan)==rim.elevation);
            /* Actual safe path across every terrace, crater and outer flank. */
            for(s32 d= -v.radius+1;d<v.radius;d+=8)
            {
                s32 x=v.x,y=v.y+d;
                for(unsigned tries=0;tries<8 && !AtlasWater_Prewarm(x,y);tries++);
                assert(!(AtlasTerrain_Tile(x,y)&0xC00));
            }
        }
        assert(sites && sites<64);
        AtlasGeo_Begin(ATLAS_GEO_VOLCANO);unsigned step;
        for(step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
        assert(gAtlasGeoStatus==1);
        assert(AtlasVolcano_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&a));
        unsigned walls=0;
        for(s32 y=-4;y<=4;y++)for(s32 x=-6;x<=6;x++)
        {
            u16 tile=AtlasTerrain_Tile(gAtlasGeoResult.x+x,gAtlasGeoResult.y+y),id=tile&1023;
            walls+=id>=ATLAS_NATURE_MOUNTAIN_FIRST && id<ATLAS_NATURE_MOUNTAIN_FIRST+94;
            if(x>=-1 && x<=1 && y>=-1 && y<=1)assert(!(tile&0xC00));
        }
        assert(walls);
        struct AtlasPlace p={0};char name[ATLAS_PLACE_NAME_BYTES];
        p.kind=ATLAS_PLACE_VOLCANO;assert(AtlasPlace_Name(&p,2,name) && !strcmp(name,"Volcanic Cone"));
        p.kind=ATLAS_PLACE_CALDERA;assert(AtlasPlace_Name(&p,2,name) && !strcmp(name,"Caldera"));
        printf("PASS generation13 seed=%u: %u rare dormant cones, crater/rim height, finite footprint, periodic replay, continuous walking gap, visible cliff TP in %u steps; unchanged macro climate/drainage\n",seeds[k],sites,step);
    }
}
