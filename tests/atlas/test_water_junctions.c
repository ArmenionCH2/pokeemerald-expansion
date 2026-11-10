/* Controlled ground supplies an independent coast position. Production bucket
 * preparation must fit it with one ground query per yielded step, even when
 * four frontiers interleave, and preserve a monotone terminal surface. */
#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
u16 AtlasWorld_Generation(void){return 3;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady=TRUE;
static unsigned groundCalls;
static s32 shore;
static u8 mode;
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *plan)
{
    (void)y;memset(plan,0,sizeof(*plan));groundCalls++;
    if(mode==3)
    {
        plan->lake=x>=shore;plan->water=150;
        return x>=shore?130:180;
    }
    /* Shore varies only in X, with optional all-land/all-sea cases. */
    return mode==1?150:mode==2?100:x>=shore?100:150;
}
static void install(void)
{
    memset(gAtlasPlan,0,sizeof(gAtlasPlan));AtlasWater_Reset();
    gAtlasPlan[0].height=180;gAtlasPlan[0].water=180;
    gAtlasPlan[0].parent=1;gAtlasPlan[0].runoff=4096;
    gAtlasPlan[1].height=90;gAtlasPlan[1].water=ATLAS_PLAN_SEA;gAtlasPlan[1].parent=1;
}
static void ownership(void)
{
    struct AtlasPlanSample p={180,96,96,150,1};u8 h,v,type;
    AtlasWater_Reset();UseBucket(0,0);sBuckets[sActive].ready=TRUE;sPieceCount=1;
    sPieces[0]=(struct RiverPiece){0,0,1000,0,150*256,150*256,30,0};
    type=AtlasWater_Sample(500,0,140,&p,&h,&v);assert(type==ATLAS_LAKE && h==150);
    p.water=160;type=AtlasWater_Sample(500,0,140,&p,&h,&v);
    assert(type==ATLAS_RIVER && h==150); /* incompatible plane is not raised */
    p.lake=0;sPieces[0].ha=sPieces[0].hb=ATLAS_PLAN_SEA*256;
    type=AtlasWater_Sample(500,0,110,&p,&h,&v);assert(type==ATLAS_SEA && h==ATLAS_PLAN_SEA);
    sPieces[0].ha=sPieces[0].hb=140*256;
    type=AtlasWater_Sample(500,0,110,&p,&h,&v);assert(type==ATLAS_RIVER && h==140);
    p.lake=1;p.water=160;
    type=AtlasWater_Sample(2000,0,110,&p,&h,&v);assert(type==ATLAS_LAKE && h==160);
}
int main(void)
{
    unsigned turn,queries;u8 previous;struct AtlasPlanSample plan={180,96,96,121,0};
    s32 xs[4]={100000,105000,110000,115000},ys[4]={-10000000,-9999999,-9999000,-9998000};
    ownership();
    for(mode=0;mode<4;mode++)
    {
        install();shore=180000;queries=0;
        if(mode==3){gAtlasPlan[1].height=130;gAtlasPlan[1].water=150;gAtlasPlan[1].basin=ATLAS_LAKE_FLAG|1;}
        for(turn=0;turn<2000;turn++)
        {
            unsigned i,ready=0;
            for(i=0;i<4;i++)
            {
                unsigned before=groundCalls;
                ready+=AtlasWater_Prewarm(xs[i],ys[i]);
                assert(groundCalls-before<=1);queries++;
            }
            if(ready==4)break;
        }
        assert(turn<2000);previous=255;
        for(unsigned part=0;part<4;part++)
        {
            s32 ax,ay,bx,by;u16 unused;
            AtlasWater_Point(0,part,&ax,&ay,&unused);AtlasWater_Point(0,part+1,&bx,&by,&unused);
            for(unsigned i=0;i<128;i++)
            {
                s32 x=ax+(s64)(bx-ax)*i/128,y=ay+(s64)(by-ay)*i/128;
                u8 surface,valley,ground=AtlasTerrain_GroundHeight(x,y,&plan);
                u8 water=AtlasWater_Sample(x,y,ground,&plan,&surface,&valley);
                assert(water==ATLAS_RIVER || water==(mode==3?ATLAS_LAKE:ATLAS_SEA));
                assert(surface<=previous && surface>=(mode==3?150:ATLAS_PLAN_SEA));previous=surface;
                if(!mode && x>=shore)assert(water==ATLAS_SEA && surface==ATLAS_PLAN_SEA);
                if(!mode && x<shore-1)assert(water==ATLAS_RIVER);
                if(mode==3 && x>=shore)assert(water==ATLAS_LAKE && surface==150);
            }
        }
        printf("PASS terminal coast mode %u, %u interleaved prewarm calls, monotone connected water\n",mode,queries);
    }
    puts("PASS: compatible lake/sea ownership, no upstream plane jump, bounded resumable terminal fitting");
}

bool8 AtlasPlan_CanyonDrain(u16 node){return FALSE;}
