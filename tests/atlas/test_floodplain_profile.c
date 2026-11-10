#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady=TRUE;
static u16 version=17;
u16 AtlasWorld_Generation(void){return version;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p)
{assert(!"injected ready profile must not query terrain");return 0;}
static void Inject(s32 x,s32 y)
{
    UseBucket(x,y);sBuckets[sActive].ready=TRUE;sPieceCount=0;
    AddPiece(3500,0,3500,4000,123*256,123*256,64);
}
int main(void)
{
    struct AtlasPlanSample plan={145,96,96,121,0};
    u8 surface,valley;struct AtlasBankProfile bank;
    unsigned last=0,wet=255,lowered=0;
    for(s32 distance=65;distance<=1200;distance++)
    {
        s32 x=3500+distance;Inject(x,2000);
        assert(AtlasWater_SampleBank(x,2000,145,&plan,&surface,&valley,&bank)==ATLAS_DRY);
        assert(bank.height>=124 && bank.height<=145 && bank.height>=last);
        assert(bank.wetness<=wet);last=bank.height;wet=bank.wetness;
        if(distance>144 && bank.height<145)lowered++;
        if(distance>1088)assert(bank.height==145 && !bank.wetness);
    }
    assert(lowered>700);
    Inject(4095,2000);AtlasWater_SampleBank(4095,2000,145,&plan,&surface,&valley,&bank);
    struct AtlasBankProfile before=bank;
    Inject(4096,2000);assert(sPieceCount==1);
    AtlasWater_SampleBank(4096,2000,145,&plan,&surface,&valley,&bank);
    assert(bank.height==before.height && bank.wetness==before.wetness && bank.height<145);
    version=16;Inject(4096,2000);assert(!sPieceCount);
    AtlasWater_SampleBank(4096,2000,145,&plan,&surface,&valley,&bank);
    assert(bank.height==145 && !bank.wetness);
    version=17;Inject(3700,2000);AddPiece(3600,0,3600,4000,130*256,130*256,32);
    AtlasWater_SampleBank(3750,2000,145,&plan,&surface,&valley,&bank);before=bank;
    struct RiverPiece swap=sPieces[0];sPieces[0]=sPieces[1];sPieces[1]=swap;
    AtlasWater_SampleBank(3750,2000,145,&plan,&surface,&valley,&bank);
    assert(bank.height==before.height && bank.wetness==before.wetness);
    puts("PASS river-scaled graded floodplain floor/moisture, extended bucket coverage with no boundary jump, dry overlap ordering and exact old 80-tile profile");
}
