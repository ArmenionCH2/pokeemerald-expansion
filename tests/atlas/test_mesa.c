#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_mesa.h"
#include "atlas_dune.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include "../../src/atlas_place.c"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static struct {s32 x,y;u8 ground,dune;} before[2048];
static void Generate(u32 seed,u16 version)
{assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
int main(void)
{
    setbuf(stdout,NULL);u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<6;k++)
    {
        Generate(seeds[k],19);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        struct AtlasMesa mesa,a,b;assert(!AtlasMesa_Site(0,&mesa));
        Generate(seeds[k],20);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        unsigned sites=0,first=ATLAS_PLAN_COUNT;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(!AtlasMesa_Site(id,&mesa))continue;
            if(first==ATLAS_PLAN_COUNT)first=id;
            sites++;assert(AtlasMesa_Sample(mesa.x,mesa.y,&a) && a.uplift==mesa.uplift);
            assert(AtlasMesa_Sample(mesa.x,mesa.y+mesa.radius/4,&b) && b.uplift==a.uplift);
            assert(AtlasMesa_Sample(mesa.x+ATLAS_WORLD_WIDTH,mesa.y,&b));assert(!memcmp(&a,&b,sizeof(a)));
            assert(!AtlasMesa_Sample(mesa.x,mesa.y+mesa.radius+512,&b));
            AtlasMesa_Region(mesa.x+ATLAS_WORLD_WIDTH/2,mesa.y,&b);
            assert(AtlasMesa_Sample(mesa.x,mesa.y,&b) && !memcmp(&a,&b,sizeof(a)));
            struct AtlasPlanSample plan;AtlasPlan_SampleDetail(mesa.x,mesa.y,&plan);
            assert(!AtlasDune_Envelope(mesa.x,mesa.y,&plan));
        }
        printf("mesas seed=%u sites=%u\n",seeds[k],sites);assert(sites && sites<64);
        assert(AtlasMesa_Site(first,&mesa));
        Generate(seeds[k],19);unsigned count=0;
        for(s32 d=-mesa.radius-512;d<=mesa.radius+512;d+=8)
        {
            assert(count<2048);struct AtlasPlanSample plan;
            before[count].x=mesa.x;before[count].y=mesa.y+d;
            before[count].ground=AtlasTerrain_GroundHeight(mesa.x,mesa.y+d,&plan);
            before[count].dune=AtlasDune_Rise(mesa.x,mesa.y+d,&plan);count++;
        }
        Generate(seeds[k],20);unsigned raised=0,cap=0,safe=0;
        for(unsigned i=0;i<count;i++)
        {
            struct AtlasPlanSample plan;u8 ground=AtlasTerrain_GroundHeight(before[i].x,before[i].y,&plan);
            unsigned expected=before[i].ground-before[i].dune+AtlasDune_Rise(before[i].x,before[i].y,&plan);
            if(AtlasMesa_Sample(before[i].x,before[i].y,&a)){expected+=a.uplift;raised+=a.uplift>0;cap+=a.uplift==mesa.uplift;}
            assert(ground==expected);
            struct AtlasCell cell;AtlasTerrain_Sample(before[i].x,before[i].y,&cell);
            if(!cell.waterform)assert(cell.elevation<=ground);
            if(!cell.waterform && cell.valley==255 && cell.landform==ATLAS_MESA)
            {
                assert(cell.elevation==ground);
                assert(!(AtlasTerrain_Tile(before[i].x,before[i].y)&0xC00));safe++;
            }
        }
        assert(raised && cap && safe);
        for(unsigned i=0;i<128;i++)
        {
            s32 x=mesa.x+(s32)(i%16)*mesa.radius/4-2*mesa.radius;
            s32 y=mesa.y+(s32)(i/16)*mesa.radius/2-2*mesa.radius;
            bool8 warm=AtlasMesa_Region(x,y,&a);
            AtlasMesa_Region(x+ATLAS_WORLD_WIDTH/3,y,&b);
            AtlasTerrain_SetSeed(seeds[k]);
            bool8 cold=AtlasMesa_Region(x,y,&b);
            assert(warm==cold);
            if(warm)assert(!memcmp(&a,&b,sizeof(a)));
        }
        AtlasGeo_Begin(ATLAS_GEO_MESA);unsigned step;
        for(step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
        printf("mesa inspection seed=%u status=%u steps=%u\n",seeds[k],gAtlasGeoStatus,step);assert(gAtlasGeoStatus==1);
        struct AtlasCell cell;struct AtlasPlace place;char name[ATLAS_PLACE_NAME_BYTES];
        AtlasTerrain_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&cell);
        assert(cell.landform==ATLAS_MESA && AtlasPlace_Identify(gAtlasGeoResult.x,gAtlasGeoResult.y,&cell,&place));
        assert(place.kind==ATLAS_PLACE_MESA && AtlasPlace_Name(&place,2,name) && !strcmp(name,"Mesa"));
        unsigned walls=0;
        for(s32 y=-4;y<=4;y++)for(s32 x=-6;x<=6;x++)
        {
            u16 tile=AtlasTerrain_Tile(gAtlasGeoResult.x+x,gAtlasGeoResult.y+y),id=tile&1023;
            walls+=id>=ATLAS_ROCK_CLIFF_FIRST && id<ATLAS_ROCK_CLIFF_FIRST+47;
            if(x>=-1 && x<=1 && y>=-1 && y<=1)assert(!(tile&0xC00));
        }
        assert(walls);
        printf("PASS generation20 seed=%u: flat caps, bounded height profile/shared ground, dune exclusion, unchanged macro nodes, periodic replay, walkable approach and named visible rock-contour inspection\n",seeds[k]);
    }
}
