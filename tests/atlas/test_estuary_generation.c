#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static struct {s32 x,y;struct AtlasCell cell;} samples[1024];
static void Generate(u32 seed,u16 version)
{
    assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));
}
int main(void)
{
    setbuf(stdout,NULL);
    u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned seed=0;seed<6;seed++)
    {
        Generate(seeds[seed],14);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        Generate(seeds[seed],15);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        unsigned eligible=0,count=0,chosen=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(!AtlasWater_IsEstuary(id))continue;
            eligible++;
            if(chosen>=2)continue;
            s32 x,y,dx,dy;unsigned status,steps=0;
            do{status=AtlasWater_MouthStep(id,&x,&y,&dx,&dy);assert(++steps<1024);}while(!status);
            if(status!=1)continue;
            chosen++;
            unsigned previous=255;
            for(s32 d=3000;d>=-32;d-=8)
            {
                struct AtlasCell cell;AtlasTerrain_Sample(x-(s64)dx*d/512,y-(s64)dy*d/512,&cell);
                assert(cell.waterform && cell.elevation<=previous);previous=cell.elevation;
            }
            for(unsigned up=500;up<=2500;up+=1000)for(s32 side=-450;side<=450;side+=6)
            {
                assert(count<1024);
                samples[count].x=x-(s64)dx*up/512+(s64)dy*side/512;
                samples[count].y=y-(s64)dy*up/512-(s64)dx*side/512;
                AtlasTerrain_Sample(samples[count].x,samples[count].y,&samples[count].cell);count++;
            }
        }
        if(!eligible || !chosen)
        {
            for(unsigned n=0;n<ATLAS_PLAN_COUNT;n++)
                if(gAtlasPlan[n].height>=ATLAS_PLAN_SEA && gAtlasPlan[n].parent<ATLAS_PLAN_COUNT && gAtlasPlan[gAtlasPlan[n].parent].height<ATLAS_PLAN_SEA && gAtlasPlan[n].runoff>=600)
                    printf("outlet %u h=%u water=%u runoff=%u\n",n,gAtlasPlan[n].height,gAtlasPlan[n].water,gAtlasPlan[n].runoff);
            printf("missing seed=%u eligible=%u chosen=%u\n",seeds[seed],eligible,chosen);
        }
        assert(eligible && chosen);
        AtlasGeo_Begin(ATLAS_GEO_ESTUARY);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        printf("estuary seed=%u eligible=%u surveyed=%u TP=%u in %u steps\n",seeds[seed],eligible,chosen,gAtlasGeoStatus,steps);
        assert(gAtlasGeoStatus==1 && AtlasWater_IsEstuary(gAtlasGeoResult.node));
        for(unsigned i=0;i<9;i++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+(s32)(i%3)-1,gAtlasGeoResult.y+(s32)(i/3)-1)&0xC00));
        Generate(seeds[seed],14);unsigned wider=0;
        for(unsigned i=0;i<count;i++)
        {
            struct AtlasCell before;AtlasTerrain_Sample(samples[i].x,samples[i].y,&before);
            const struct AtlasCell *after=&samples[i].cell;
            if(before.waterform)assert(after->waterform);
            if(!before.waterform && after->waterform==ATLAS_RIVER){wider++;assert(after->elevation<=before.elevation);}
        }
        assert(wider);
        printf("PASS generation15 seed=%u: %u added river tiles in %u actual mouth transect samples, no lost water, descending receiver surfaces, safe estuary inspection and unchanged macro geography/climate\n",seeds[seed],wider,count);
    }
}
