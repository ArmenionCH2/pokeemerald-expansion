#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
static u16 map[1190];
static void Walk(s32 x,s32 y)
{
    unsigned leg,step,frame;
    s32 cx=AtlasTerrain_FloorDiv(x,20),cy=AtlasTerrain_FloorDiv(y,20),oldCX=cx,oldCY=cy;
    AtlasStream_Reset();assert(AtlasStream_Load(map,cx,cy));
    for(frame=0;frame<80;frame++)AtlasStream_Tick(cx,cy,x-cx*20,y-cy*20);
    for(leg=0;leg<8;leg++)for(step=0;step<40;step++)
    {
        unsigned dirs[8]={4,1,3,2,3,1,4,2};unsigned dir=dirs[leg];
        s32 dx,dy;
        assert(AtlasStream_CanStep(dir));
        if(dir==4)x++;
        if(dir==3)x--;
        if(dir==1)y++;
        if(dir==2)y--;
        x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
        cx=AtlasTerrain_FloorDiv(x,20);cy=AtlasTerrain_FloorDiv(y,20);
        if(cx!=oldCX || cy!=oldCY){assert(AtlasStream_Load(map,cx,cy));oldCX=cx;oldCY=cy;}
        for(dy=-5;dy<=6;dy++)for(dx=-7;dx<=8;dx++)
        {
            u16 actual=map[(y-cy*20+7+dy)*35+x-cx*20+7+dx], expected=AtlasTerrain_Tile(x+dx,y+dy);
            if(actual!=expected)fprintf(stderr,"MISMATCH at %d,%d leg=%u step=%u offset=%d,%d map=%04x direct=%04x\n",x,y,leg,step,dx,dy,actual,expected);
            assert(actual==expected);
        }
        for(frame=0;frame<8;frame++)AtlasStream_Tick(cx,cy,x-cx*20,y-cy*20);
    }
    assert(gAtlasCacheMisses==1 && !gAtlasStreamWaits && gAtlasCacheHits>=16);
    printf("PASS rolling apron at %d,%d: %u rebases, one cold load, no readiness waits\n",x,y,gAtlasCacheHits);
}
int main(void)
{
    setbuf(stdout,NULL);
    assert(AtlasPlan_Begin(0));while(!AtlasPlan_Step(256));
    Walk(15654290,5327430);Walk(ATLAS_WORLD_WIDTH-30,-4090);Walk(4090,4090);Walk(7931504,212940);
    puts("PASS: native-sized visible entries match direct geography throughout running, corners, reversals, bucket boundaries and X wrap");
    return 0;
}
