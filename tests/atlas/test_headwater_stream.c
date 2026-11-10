#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static u16 map[1190];
static struct {s32 x,y;u16 tiles[5];} observed[640];
static const s8 view[5][2]={{0,0},{-7,-5},{8,-5},{-7,6},{8,6}};
static void Walk(s32 x,s32 y,bool8 vertical)
{
    s32 cx=AtlasTerrain_FloorDiv(x,20),cy=AtlasTerrain_FloorDiv(y,20),oldX=cx,oldY=cy;
    AtlasWater_Reset();AtlasStream_Reset();assert(AtlasStream_Load(map,cx,cy));
    for(unsigned frame=0;frame<80;frame++)AtlasStream_Tick(cx,cy,x-cx*20,y-cy*20);
    for(unsigned step=0;step<640;step++)
    {
        bool8 forward=step<160 || step>=480;
        u8 direction=vertical?(forward?1:2):(forward?4:3);
        if(!AtlasStream_CanStep(direction))fprintf(stderr,"headwater frontier wait at %d,%d vertical=%u step=%u\n",x,y,vertical,step);
        assert(!gAtlasStreamWaits);
        if(vertical)y+=forward?1:-1;else x+=forward?1:-1;
        cx=AtlasTerrain_FloorDiv(x,20);cy=AtlasTerrain_FloorDiv(y,20);
        if(cx!=oldX || cy!=oldY){assert(AtlasStream_Load(map,cx,cy));oldX=cx;oldY=cy;}
        observed[step].x=x;observed[step].y=y;
        for(unsigned i=0;i<5;i++)
            observed[step].tiles[i]=map[(y-cy*20+7+view[i][1])*35+x-cx*20+7+view[i][0]];
        for(unsigned frame=0;frame<8;frame++)AtlasStream_Tick(cx,cy,x-cx*20,y-cy*20);
    }
    assert(!gAtlasStreamWaits && gAtlasCacheMisses==1 && gAtlasCacheHits>=32);
    /* Compare only after the timed walk. A direct geography oracle must never
     * warm hydrology ahead of streaming and hide an unready frontier. */
    for(unsigned step=0;step<640;step++)for(unsigned i=0;i<5;i++)
        assert(observed[step].tiles[i]==AtlasTerrain_Tile(observed[step].x+view[i][0],observed[step].y+view[i][1]));
}
int main(void)
{
    setbuf(stdout,NULL);u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    assert(AtlasWorld_SetGeneration(21));
    for(unsigned k=0;k<6;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));
        AtlasGeo_Begin(ATLAS_GEO_HEADWATER);
        for(unsigned step=0;step<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();step++);
        assert(gAtlasGeoStatus==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;
        Walk((AtlasTerrain_FloorDiv(x,4096)+1)*4096-100,y,FALSE);
        Walk(x,(AtlasTerrain_FloorDiv(y,4096)+1)*4096-100,TRUE);
        printf("PASS generation21 seed=%u: 1280 running steps/reversals across both water-cache axes, one cold fill, no waits, post-walk visible oracle\n",seeds[k]);
    }
}
