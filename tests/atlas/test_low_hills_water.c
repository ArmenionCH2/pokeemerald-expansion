#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
static s32 Wrap(s32 x){x%=ATLAS_WORLD_WIDTH;return x<0?x+ATLAS_WORLD_WIDTH:x;}
int main(void)
{
    setbuf(stdout,NULL);const u32 seeds[]={0,1,2731893282u};unsigned river=0,lake=0;
    assert(AtlasWorld_SetGeneration(27));
    for(unsigned k=0;k<3;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));unsigned reaches=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            struct AtlasPlanNode *n=&gAtlasPlan[id];
            if(n->basin&ATLAS_LAKE_FLAG)
            {
                s32 x=(u32)(id%96)*ATLAS_WORLD_WIDTH/96,y=(id/96)*(2*ATLAS_WORLD_HALF_HEIGHT/47)-ATLAS_WORLD_HALF_HEIGHT;
                struct AtlasCell c;AtlasTerrain_Sample(x,y,&c);
                assert(c.waterform==ATLAS_LAKE && c.elevation==n->water && c.landform!=ATLAS_KNOLL);lake++;
            }
            if(reaches>=100 || n->runoff<600 || n->height<ATLAS_PLAN_SEA || n->parent==id || n->parent>=ATLAS_PLAN_COUNT)continue;
            reaches++;unsigned previous=255;
            for(unsigned part=0;part<5;part++)
            {
                s32 x,y;u16 unused;struct AtlasCell c;AtlasWater_Point(id,part,&x,&y,&unused);x=Wrap(x);
                AtlasTerrain_Sample(x,y,&c);
                if(c.waterform==ATLAS_DRY)fprintf(stderr,"dry channel seed=%u node=%u part=%u at=%d,%d height=%u\n",seeds[k],id,part,x,y,c.elevation);
                assert(c.waterform!=ATLAS_DRY && c.elevation<=previous && c.landform!=ATLAS_KNOLL);previous=c.elevation;
                u16 tile=AtlasTerrain_Tile(x,y),idTile=tile&1023;assert((tile&0xFC00)==0x3400 && !AtlasTiles_IsCliff(idTile));
                assert((idTile>=512 && idTile<887) || idTile==ATLAS_ICE_LAKE || idTile==ATLAS_ICE_SEA);river++;
            }
        }
        assert(reaches);printf("generation27 seed=%u checked %u main reaches\n",seeds[k],reaches);
    }
    assert(lake && river);printf("PASS generation27 lowland relief preserves %u sampled main-channel points, downhill water levels and %u lake centres\n",river,lake);
}
