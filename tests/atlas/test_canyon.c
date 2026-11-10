#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode prior[ATLAS_PLAN_COUNT];
int main(void)
{
    setbuf(stdout,NULL);const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    assert(AtlasWorld_SetGeneration(24));
    for(unsigned k=0;k<6;k++)
    {
        assert(AtlasWorld_SetGeneration(23) && AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));
        memcpy(prior,gAtlasPlan,sizeof(prior));assert(!AtlasPlan_CanyonDrain(0));
        assert(AtlasWorld_SetGeneration(24) && AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));
        assert(!memcmp(prior,gAtlasPlan,sizeof(prior)));unsigned status=2,steps=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(gAtlasPlan[id].height<ATLAS_PLAN_SEA || gAtlasPlan[id].parent==id)continue;
            s32 px,py;u16 head,tail;AtlasWater_Point(id,0,&px,&py,&head);AtlasWater_Point(id,4,&px,&py,&tail);assert(head>=tail);
            if(AtlasPlan_CanyonDrain(id))
            {
                unsigned at=id;for(unsigned count=0;count<ATLAS_PLAN_COUNT;count++)
                {assert(!(gAtlasPlan[at].basin&ATLAS_LAKE_FLAG));if(gAtlasPlan[at].height<ATLAS_PLAN_SEA)break;at=gAtlasPlan[at].parent;assert(count+1<ATLAS_PLAN_COUNT);}
            }
            if(gAtlasPlan[gAtlasPlan[id].parent].basin&ATLAS_LAKE_FLAG)assert(head==gAtlasPlan[id].water*256);
        }
        for(unsigned batch=0;batch<3 && status==2;batch++)
        {AtlasGeo_Begin(ATLAS_GEO_CANYON);for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);status=gAtlasGeoStatus;}
        printf("canyon seed=%u status=%u steps=%u\n",seeds[k],status,steps);assert(status==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;struct AtlasCell cell;AtlasTerrain_Sample(x,y,&cell);
        assert(!cell.waterform && cell.landform==ATLAS_CANYON);unsigned walls=0;
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
        {u16 tile=AtlasTerrain_Tile(x+dx,y+dy);walls+=AtlasTiles_IsCliff(tile&1023);if(dx>=-1 && dx<=1 && dy>=-1 && dy<=1)assert(!(tile&0xC00));}
        assert(walls);puts("PASS actual connected-river canyon terrain and safe visible bedrock-wall inspector");
    }
}
