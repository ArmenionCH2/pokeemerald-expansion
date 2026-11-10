#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode old[ATLAS_PLAN_COUNT];
static u8 shown[ATLAS_PLAN_COUNT];
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
        Generate(seeds[seed],15);memcpy(old,gAtlasPlan,sizeof(old));
        for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)shown[i]=AtlasWater_IsSmallRiver(i);
        Generate(seeds[seed],16);assert(!memcmp(old,gAtlasPlan,sizeof(old)));
        unsigned added=0,coastal=0,interior=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(shown[id])assert(AtlasWater_IsSmallRiver(id));
            if(!AtlasWater_IsSmallRiver(id))continue;
            struct AtlasPlanNode *n=&gAtlasPlan[id];assert(n->runoff>=240 && n->runoff<600);
            u16 at=id;unsigned steps=0;
            while(gAtlasPlan[at].height>=ATLAS_PLAN_SEA)
            {
                if(gAtlasPlan[at].runoff<600)assert(AtlasWater_IsSmallRiver(at));
                at=gAtlasPlan[at].parent;assert(++steps<ATLAS_PLAN_COUNT);
            }
            if(shown[id])continue;
            added++;
            if(gAtlasPlan[n->parent].height<ATLAS_PLAN_SEA){coastal++;assert(!AtlasWater_IsTributary(id));}
            else{interior++;assert(AtlasWater_IsTributary(id));}
            s32 x,y;u16 height;struct AtlasCell cell;
            AtlasWater_Point(id,2,&x,&y,&height);AtlasTerrain_Sample(x,y,&cell);assert(cell.waterform);
            if(added<=8)
                for(unsigned part=0;part<=4;part++)
                {AtlasWater_Point(id,part,&x,&y,&height);AtlasTerrain_Sample(x,y,&cell);assert(cell.waterform);}
        }
        assert(coastal && interior);
        AtlasGeo_Begin(ATLAS_GEO_TRIBUTARY);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        assert(gAtlasGeoStatus==1 && AtlasWater_IsSmallRiver(gAtlasGeoResult.node));
        for(unsigned i=0;i<9;i++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+(s32)(i%3)-1,gAtlasGeoResult.y+(s32)(i/3)-1)&0xC00));
        printf("PASS generation16 seed=%u: %u connected small edges added (%u coastal, %u interior), all downstream paths reach sea, actual channel samples and safe TP, unchanged macro geography/climate\n",seeds[seed],added,coastal,interior);
    }
}
