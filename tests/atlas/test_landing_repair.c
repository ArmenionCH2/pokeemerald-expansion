#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
static s32 Abs(s32 n){return n<0?-n:n;}
int main(void)
{
    u32 seeds[]={0,4220706268u,1,123456789};unsigned repaired=0,fallbacks=0;
    assert(!AtlasTerrain_RepairLanding(NULL,NULL));
    for(unsigned s=0;s<4;s++)
    {
        s32 x,y,oldX,oldY;
        assert(AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));
        assert(AtlasTerrain_FindSpawn(&x,&y));oldX=x;oldY=y;
        assert(AtlasTerrain_RepairLanding(&x,&y) && x==oldX && y==oldY);
        for(u16 id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            struct AtlasPlanSample plan;struct AtlasCell c;u16 unused;bool8 local=FALSE;
            if(!AtlasWater_IsTributary(id) || gAtlasPlan[id].runoff<544)continue;
            AtlasWater_Point(id,2,&x,&y,&unused);x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
            AtlasTerrain_GroundHeight(x,y,&plan);if(plan.lake)continue;
            for(s32 dy=-32;dy<=32 && !local;dy++)for(s32 dx=-32;dx<=32;dx++)
                if((dx==-32 || dx==32 || dy==-32 || dy==32) && !(AtlasTerrain_Tile(x+dx,y+dy)&0xC00)){local=TRUE;break;}
            if(!local)continue;
            for(s32 dy=-16;dy<=16;dy++)for(s32 dx=-16;dx<=16;dx++)
            {AtlasTerrain_Sample(x+dx,y+dy,&c);assert(c.waterform);}
            oldX=x;oldY=y;assert(AtlasTerrain_RepairLanding(&x,&y));
            assert(!(AtlasTerrain_Tile(x,y)&0xC00));
            assert(Abs(x-oldX)<=32 && Abs(y-oldY)<=32 && (Abs(x-oldX)>16 || Abs(y-oldY)>16));
            printf("Repair fixture seed=%u from=%d,%d to=%d,%d\n",seeds[s],oldX,oldY,x,y);
            repaired++;break;
        }
        for(u16 id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(gAtlasPlan[id].height>=100)continue;
            x=(u32)(id%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
            y=(id/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
            oldX=x;oldY=y;assert(AtlasTerrain_RepairLanding(&x,&y));
            assert(!(AtlasTerrain_Tile(x,y)&0xC00) && (x!=oldX || y!=oldY));fallbacks++;break;
        }
    }
    printf("PASS landing repair: %u channels needing >16-tile search, %u verified spawn recoveries; valid land unchanged\n",repaired,fallbacks);
    assert(repaired && fallbacks==4);
}
