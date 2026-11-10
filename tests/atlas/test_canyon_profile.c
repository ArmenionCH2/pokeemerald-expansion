#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];u32 gAtlasWorldSeed;bool8 gAtlasPlanReady=TRUE;
static u16 version=24;u16 AtlasWorld_Generation(void){return version;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p){assert(!"ready fixture must not query terrain");return 0;}
static void Inject(s32 x,s32 y)
{UseBucket(x,y);sBuckets[sActive].ready=TRUE;sPieceCount=0;AddPiece(3500,0,3500,4000,150*256,150*256,32);}
int main(void)
{
    struct AtlasPlanSample plan={200,96,96,121,0};struct AtlasBankProfile bank;
    u8 surface,valley;unsigned last=0,labels=0;
    for(s32 d=33;d<=600;d++)
    {
        Inject(3500+d,2000);assert(AtlasWater_SampleBank(3500+d,2000,200,&plan,&surface,&valley,&bank)==ATLAS_DRY);
        assert(bank.height>=151 && bank.height<=200 && bank.height>=last);last=bank.height;labels+=bank.canyon;
        if(d<288)assert(bank.height<=154); // subdued inner half-section
        if(d>544)assert(bank.height==200 && !bank.canyon);
    }
    assert(labels>400);
    Inject(3700,2000);AtlasWater_SampleBank(3700,2000,200,&plan,&surface,&valley,&bank);assert(bank.canyon);u8 floor=bank.height;
    version=23;Inject(3700,2000);AtlasWater_SampleBank(3700,2000,200,&plan,&surface,&valley,&bank);assert(!bank.canyon && bank.height>floor);
    version=24;
    for(unsigned width=2;width<=128;width+=7)for(unsigned height=125;height<=250;height+=5)
    {
        Inject(3500,2000);sPieces[0].width=width;sPieces[0].widthB=CREEK_FLAG;
        AtlasWater_SampleBank(3500+width+20,2000,height,&plan,&surface,&valley,&bank);assert(!bank.canyon && bank.height<=height);
        sPieces[0].widthB=0;AtlasWater_SampleBank(3500+width+20,2000,height,&plan,&surface,&valley,&bank);
        if(width<16 || width>96 || height<166)assert(!bank.canyon);
    }
    Inject(3700,2000);AddPiece(3600,0,3600,4000,160*256,160*256,32);
    AtlasWater_SampleBank(3750,2000,200,&plan,&surface,&valley,&bank);struct AtlasBankProfile before=bank;
    struct RiverPiece swap=sPieces[0];sPieces[0]=sPieces[1];sPieces[1]=swap;
    AtlasWater_SampleBank(3750,2000,200,&plan,&surface,&valley,&bank);assert(!memcmp(&before,&bank,sizeof(bank)));
    puts("PASS canyon section: subdued floor/steep outer walls, monotone profile, finite footprint, exact older profile, creek/lowland exclusion and order-stable dry overlap");
}

bool8 AtlasPlan_CanyonDrain(u16 node){return FALSE;}
