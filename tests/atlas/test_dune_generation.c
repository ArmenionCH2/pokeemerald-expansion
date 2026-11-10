#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_dune.h"
#include "atlas_volcano.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include "../../src/atlas_place.c"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static struct {s32 x,y;struct AtlasCell cell;u8 rise;} sample[4096];
static void Generate(u32 seed,u16 version)
{assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
int main(void)
{
    setbuf(stdout,NULL);u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    unsigned totalCooling=0;
    for(unsigned seed=0;seed<6;seed++)
    {
        Generate(seeds[seed],18);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        Generate(seeds[seed],19);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        unsigned count=0;bool8 located=FALSE;s32 cx=0,cy=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT && !located;id++)
        {
            s32 x=(u32)(id%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
            s32 y=(id/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
            struct AtlasPlanSample plan;AtlasPlan_SampleDetail(x,y,&plan);
            if(AtlasDune_Envelope(x,y,&plan)<64)continue;
            struct AtlasCell cell;AtlasTerrain_Sample(x,y,&cell);
            if(cell.landform!=ATLAS_DUNE && cell.landform!=ATLAS_DUNE_CREST)continue;
            cx=x;cy=y;located=TRUE;
        }
        assert(located);
        unsigned crest=0,trough=0;
        for(s32 y=cy;y<=cy+512;y+=16)for(s32 x=cx;x<=cx+512;x+=8)
        {
            struct AtlasCell cell;AtlasTerrain_Sample(x,y,&cell);
            if(cell.landform!=ATLAS_DUNE && cell.landform!=ATLAS_DUNE_CREST)continue;
            assert(!cell.waterform);struct AtlasPlanSample plan;
            assert(AtlasTerrain_GroundHeight(x,y,&plan)==cell.elevation);
            u8 rise=AtlasDune_Rise(x,y,&plan);assert(rise<=6);
            assert(AtlasDune_Envelope(x,y,&plan));
            crest+=cell.landform==ATLAS_DUNE_CREST;trough+=cell.landform==ATLAS_DUNE;
            sample[count].x=x;sample[count].y=y;sample[count].cell=cell;sample[count].rise=rise;count++;
        }
        assert(count && crest && trough);
        AtlasGeo_Begin(ATLAS_GEO_DUNES);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        printf("dunes seed=%u status=%u steps=%u\n",seeds[seed],gAtlasGeoStatus,steps);
        assert(gAtlasGeoStatus==1);
        unsigned faces=0;
        for(s32 y=-4;y<=4;y++)for(s32 x=-6;x<=6;x++)
        {
            u16 tile=AtlasTerrain_Tile(gAtlasGeoResult.x+x,gAtlasGeoResult.y+y),id=tile&1023;
            faces+=id>=ATLAS_DUNE_EDGE_FIRST && id<ATLAS_DUNE_EDGE_FIRST+47;
            if(x>=-1 && x<=1 && y>=-1 && y<=1)assert(!(tile&0xC00));
        }
        assert(faces);
        struct AtlasCell cell;struct AtlasPlace place;char name[ATLAS_PLACE_NAME_BYTES];
        AtlasTerrain_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&cell);assert(AtlasPlace_Identify(gAtlasGeoResult.x,gAtlasGeoResult.y,&cell,&place));
        assert(place.kind==ATLAS_PLACE_DUNES && AtlasPlace_Name(&place,2,name) && !strcmp(name,"Dunes"));
        Generate(seeds[seed],18);unsigned raised=0,cooled=0;
        for(unsigned i=0;i<count;i++)
        {
            struct AtlasCell before;AtlasTerrain_Sample(sample[i].x,sample[i].y,&before);
            assert(!before.waterform && sample[i].cell.elevation==before.elevation+sample[i].rise);
            assert(sample[i].cell.temperature<=before.temperature && sample[i].cell.humidity>=before.humidity);
            raised+=sample[i].rise>0;cooled+=sample[i].cell.temperature<before.temperature;
        }
        assert(raised);totalCooling+=cooled;
        Generate(seeds[seed],19);
        struct AtlasPlanSample fake={140,40,40,121,0};
        assert(AtlasDune_Rise(-1,0,&fake)==AtlasDune_Rise(ATLAS_WORLD_WIDTH-1,0,&fake));
        s32 extremes[]={INT_MIN,INT_MAX,-1,ATLAS_WORLD_WIDTH};
        for(unsigned i=0;i<4;i++)
        {
            s32 normalized=extremes[i]%ATLAS_WORLD_WIDTH;if(normalized<0)normalized+=ATLAS_WORLD_WIDTH;
            assert(AtlasDune_Probe(extremes[i],cy)==AtlasDune_Probe(normalized,cy));
        }
        assert(!AtlasDune_Probe(0,INT_MIN) && !AtlasDune_Probe(0,INT_MAX));
        assert(!AtlasDune_Envelope(0,0,NULL));
        printf("PASS generation19 seed=%u: %u actual sand samples, %u raised/%u cooled, crest/troughs, shared ground height, unchanged macro geography and monotone relative humidity and native safe dune inspection\n",seeds[seed],count,raised,cooled);
    }
    assert(totalCooling);
    /* Native regression: a curved bank can be within the naming stencil even
     * after a 40-tile axis offset. The floodplain inspector must reach a plain,
     * rather than a place correctly announced as Riverbank or Creekbank. */
    Generate(0xf227d31e,19);AtlasGeo_Begin(ATLAS_GEO_FLOODPLAIN);
    for(unsigned step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
    assert(gAtlasGeoStatus==1);
    struct AtlasCell plain;struct AtlasVolcano volcano;
    AtlasTerrain_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&plain);
    assert(!plain.waterform && plain.valley==0 && plain.elevation<155 && plain.moisture>=132);
    assert(!AtlasVolcano_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&volcano));
    static const s8 sides[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(unsigned side=0;side<4;side++)for(s32 distance=1;distance<=3;distance++)
    {
        AtlasTerrain_Sample(gAtlasGeoResult.x+sides[side][0]*distance,gAtlasGeoResult.y+sides[side][1]*distance,&plain);
        assert(!plain.waterform);
    }
    puts("PASS native-seed floodplain inspector regression: dry soil landing outside the shoreline naming stencil and volcanic precedence");
}
