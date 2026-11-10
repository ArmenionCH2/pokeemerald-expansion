#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "../../src/atlas_place.c"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static struct {s32 x,y;struct AtlasCell cell;} samples[2048];
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
        Generate(seeds[seed],16);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        unsigned selected=0,count=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT && selected<6;id++)
        {
            const struct AtlasPlanNode *n=&gAtlasPlan[id];
            if(n->height<132 || n->height>170 || n->parent>=ATLAS_PLAN_COUNT || n->parent==id
             || (n->runoff<600 && !AtlasWater_IsSmallRiver(id)))continue;
            s32 x,y,ax,ay,bx,by;u16 h;
            AtlasWater_Point(id,2,&x,&y,&h);AtlasWater_Point(id,1,&ax,&ay,&h);AtlasWater_Point(id,3,&bx,&by,&h);
            s32 dx=bx-ax,dy=by-ay;bool8 horizontal=(dx<0?-dx:dx)>(dy<0?-dy:dy);
            struct AtlasCell channel,bank;AtlasTerrain_Sample(x,y,&channel);
            if(channel.waterform!=ATLAS_RIVER)continue;
            bool8 incisable=FALSE;
            for(s32 side=-1;side<=1;side+=2)
            {
                AtlasTerrain_Sample(x+(horizontal?0:side*200),y+(horizontal?side*200:0),&bank);
                if(!bank.waterform && bank.elevation>channel.elevation+1)incisable=TRUE;
            }
            if(!incisable)continue;
            for(s32 offset=-1200;offset<=1200;offset+=16)
            {
                assert(count<2048);samples[count].x=x+(horizontal?0:offset);samples[count].y=y+(horizontal?offset:0);
                AtlasTerrain_Sample(samples[count].x,samples[count].y,&samples[count].cell);count++;
            }
            selected++;
        }
        Generate(seeds[seed],17);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        unsigned lower=0,moister=0;
        for(unsigned i=0;i<count;i++)
        {
            struct AtlasCell after;AtlasTerrain_Sample(samples[i].x,samples[i].y,&after);
            const struct AtlasCell *before=&samples[i].cell;
            assert(after.waterform==before->waterform);
            if(!after.waterform){assert(after.elevation<=before->elevation);lower+=after.elevation<before->elevation;moister+=after.moisture>before->moisture;}
        }
        assert(lower && moister);
        AtlasGeo_Begin(ATLAS_GEO_FLOODPLAIN);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        printf("floodplain seed=%u TP=%u in %u steps\n",seeds[seed],gAtlasGeoStatus,steps);
        assert(gAtlasGeoStatus==1);
        struct AtlasCell cell;AtlasTerrain_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&cell);
        assert(!cell.waterform && cell.valley==0 && cell.elevation<155 && cell.moisture>=132);
        for(unsigned i=0;i<9;i++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+(s32)(i%3)-1,gAtlasGeoResult.y+(s32)(i/3)-1)&0xC00));
        struct AtlasPlace place={0};char name[ATLAS_PLACE_NAME_BYTES];place.kind=ATLAS_PLACE_FLOODPLAIN;
        assert(AtlasPlace_Name(&place,2,name) && !strcmp(name,"Floodplain"));
        printf("PASS generation17 seed=%u: %u lower and %u moister dry samples across %u river transect points; sampled water membership and macro geography/climate retained, named safe floodplain TP\n",seeds[seed],lower,moister,count);
    }
}
