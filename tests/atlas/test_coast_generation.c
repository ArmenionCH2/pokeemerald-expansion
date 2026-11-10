#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_coast.h"
#include "atlas_dune.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static struct {s32 x,y;u8 ground,dune;struct AtlasCell cell;} before[1089];
static void Generate(u32 seed,u16 version)
{assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
static void Context(void)
{
    Generate(0,22);
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i]=(struct AtlasPlanNode){.height=140};
    AtlasPlan_ResetSample();assert(!AtlasPlan_SeaContext(0,0));
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i].height=110;
    AtlasPlan_ResetSample();assert(AtlasPlan_SeaContext(0,0)==255);
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i].height=140;
    gAtlasPlan[0].height=110;AtlasPlan_ResetSample();
    s32 x=ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
    s32 y=-ATLAS_WORLD_HALF_HEIGHT+ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
    unsigned quarter=AtlasPlan_SeaContext(x,y);assert(quarter>=62 && quarter<=65);
    assert(AtlasPlan_SeaContext(ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH,y)<=1);
    assert(!AtlasPlan_SeaContext(ATLAS_WORLD_WIDTH/2,0));
    assert(AtlasPlan_SeaContext(x,y)==quarter);
    assert(AtlasPlan_SeaContext(-1,-ATLAS_WORLD_HALF_HEIGHT)==AtlasPlan_SeaContext(ATLAS_WORLD_WIDTH-1,-ATLAS_WORLD_HALF_HEIGHT));
    assert(!AtlasPlan_SeaContext(0,ATLAS_WORLD_HALF_HEIGHT));
    puts("PASS smooth ocean-context weights: uniform land/sea, mixed corner, zero outer edge, cache replay, dateline and latitude bounds");
}
int main(void)
{
    setbuf(stdout,NULL);Context();u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<6;k++)
    {
        Generate(seeds[k],21);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        Generate(seeds[k],22);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        AtlasGeo_Begin(ATLAS_GEO_SEA_CLIFF);unsigned step;
        for(step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
        printf("sea-cliff inspection seed=%u status=%u steps=%u\n",seeds[k],gAtlasGeoStatus,step);assert(gAtlasGeoStatus==1);
        s32 cx=gAtlasGeoResult.x,cy=gAtlasGeoResult.y;struct AtlasCell cell;
        AtlasTerrain_Sample(cx,cy,&cell);assert(!cell.waterform && cell.landform==ATLAS_ROCK_COAST);
        unsigned walls=0,sea=0;
        for(s32 y=-4;y<=4;y++)for(s32 x=-6;x<=6;x++)
        {
            u16 tile=AtlasTerrain_Tile(cx+x,cy+y);walls+=AtlasTiles_IsCliff(tile&1023);
            AtlasTerrain_Sample(cx+x,cy+y,&cell);sea+=cell.waterform==ATLAS_SEA;
            if(x>=-1 && x<=1 && y>=-1 && y<=1)assert(!(tile&0xC00));
        }
        assert(walls && sea);unsigned near=0;
        static const s8 offsets[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
        for(unsigned side=0;side<4;side++)for(s32 d=1;d<=3;d++)
        {AtlasTerrain_Sample(cx+offsets[side][0]*d,cy+offsets[side][1]*d,&cell);near+=cell.waterform==ATLAS_SEA;}
        assert(near);
        Generate(seeds[k],21);unsigned count=0;
        for(s32 y=cy-64;y<=cy+64;y+=4)for(s32 x=cx-64;x<=cx+64;x+=4)
        {
            struct AtlasPlanSample plan;before[count].x=x;before[count].y=y;
            before[count].ground=AtlasTerrain_GroundHeight(x,y,&plan);before[count].dune=AtlasDune_Rise(x,y,&plan);
            AtlasTerrain_Sample(x,y,&before[count].cell);count++;
        }
        Generate(seeds[k],22);unsigned raised=0;
        for(unsigned i=0;i<count;i++)
        {
            struct AtlasPlanSample plan;u8 ground=AtlasTerrain_GroundHeight(before[i].x,before[i].y,&plan);
            unsigned original=before[i].ground-before[i].dune;
            unsigned rise=AtlasCoast_Rise(before[i].x,before[i].y,&plan,original);assert(rise<=24);
            unsigned expected=original+rise+AtlasDune_Rise(before[i].x,before[i].y,&plan);assert(ground==expected);
            AtlasTerrain_Sample(before[i].x,before[i].y,&cell);
            assert((cell.waterform==ATLAS_SEA)==(before[i].cell.waterform==ATLAS_SEA));
            if(original<ATLAS_PLAN_SEA)assert(!rise);else raised+=rise>0;
        }
        assert(raised);
        struct AtlasPlanSample fake={125,96,96,121,0};
        assert(AtlasCoast_Rise(-1,0,&fake,125)==AtlasCoast_Rise(ATLAS_WORLD_WIDTH-1,0,&fake,125));
        assert(!AtlasCoast_Rise(0,0,NULL,125));
        assert(!AtlasCoast_Rise(0,0,&fake,120) && !AtlasCoast_Rise(0,0,&fake,136));
        printf("PASS generation22 seed=%u: shared bounded dry coastal height, original sea-floor protection, sampled shore membership, unchanged macro notes and safe visible cliff inspection\n",seeds[k]);
    }
}
