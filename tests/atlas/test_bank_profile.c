#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
u16 AtlasWorld_Generation(void){return 3;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady;
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *plan)
{assert(!"ready fixture must not sample ground");return 0;}
int main(void)
{
    struct AtlasPlanSample plan={200,96,96,121,0};
    struct AtlasBankProfile bank;
    AtlasWater_Reset();UseBucket(0,0);sBuckets[sActive].ready=TRUE;sPieceCount=1;
    sPieces[0]=(struct RiverPiece){0,1000,4000,1000,140*256,140*256,30,0};
    unsigned last=0,wet=255;
    for(s32 d=31;d<=120;d++)
    {
        u8 a=0,b=0,va,vb;
        u8 old=AtlasWater_Sample(2000,1000+d,200,&plan,&a,&va);
        u8 now=AtlasWater_SampleBank(2000,1000+d,200,&plan,&b,&vb,&bank);
        assert(old==now && a==b && va==vb && now==ATLAS_DRY);
        assert(bank.height>=141 && bank.height<=200 && bank.height>=last);
        assert(bank.wetness<=wet);last=bank.height;wet=bank.wetness;
        if(d>110)assert(bank.height==200 && !bank.wetness);
    }
    for(unsigned height=121;height<256;height+=7)for(s32 x=0;x<=4095;x+=63)for(s32 y=900;y<=1120;y+=7)
    {
        u8 a=0,b=0,va,vb;
        u8 old=AtlasWater_Sample(x,y,height,&plan,&a,&va);
        u8 now=AtlasWater_SampleBank(x,y,height,&plan,&b,&vb,&bank);
        assert(old==now && a==b && va==vb);
        assert(bank.height<=height && bank.wetness<=7);
    }
    /* Lowering is order-independent when multiple dry bank aprons overlap. */
    sPieceCount=2;sPieces[1]=sPieces[0];sPieces[1].ay=sPieces[1].by=1080;
    sPieces[1].ha=sPieces[1].hb=150*256;
    u8 a,b;AtlasWater_SampleBank(2000,1040,200,&plan,&a,&b,&bank);
    struct AtlasBankProfile before=bank;struct RiverPiece swap=sPieces[0];sPieces[0]=sPieces[1];sPieces[1]=swap;
    AtlasWater_SampleBank(2000,1040,200,&plan,&a,&b,&bank);
    assert(bank.height==before.height && bank.wetness==before.wetness);
    puts("PASS graded dry bank heights, moisture falloff, overlap ordering and unchanged water membership/surface/legacy valley caps");
}
