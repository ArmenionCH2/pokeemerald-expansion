#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];u32 gAtlasWorldSeed;bool8 gAtlasPlanReady=TRUE;
static u16 version=25;u16 AtlasWorld_Generation(void){return version;}
bool8 AtlasPlan_CanyonDrain(u16 node){return FALSE;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p){assert(!"ready fixture must not query ground");return 0;}
static void Inject(void){UseBucket(0,0);sBuckets[sActive].ready=TRUE;sPieceCount=1;sPieces[0]=(struct RiverPiece){1000,0,1000,4000,130*256,130*256,32,0};}
int main(void)
{
    struct AtlasPlanSample plan={140,130,120,121,0};struct AtlasBankProfile bank;u8 surface,valley;
    Inject();assert(AtlasWater_SampleBank(1050,2000,140,&plan,&surface,&valley,&bank)==ATLAS_DRY);assert(bank.flood==ATLAS_BANK_LEVEE && bank.height==133 && bank.wetness==4);
    AtlasWater_SampleBank(1200,2000,140,&plan,&surface,&valley,&bank);assert(bank.flood==ATLAS_BANK_BACKSWAMP && bank.height==131 && bank.wetness==7);
    version=24;AtlasWater_SampleBank(1200,2000,140,&plan,&surface,&valley,&bank);assert(!bank.flood);version=25;
    for(unsigned condition=0;condition<5;condition++)
    {
        Inject();struct AtlasPlanSample p=plan;
        if(condition==0)p.rain=95;
        if(condition==1)p.humidity=79;
        if(condition==2)p.lake=1;
        if(condition==3)sPieces[0].width=23;
        if(condition==4)sPieces[0].widthB=CREEK_FLAG;
        AtlasWater_SampleBank(1200,2000,140,&p,&surface,&valley,&bank);assert(!bank.flood);
    }
    Inject();sPieceCount=2;sPieces[1]=sPieces[0];sPieces[1].ax=sPieces[1].bx=1200;
    AtlasWater_SampleBank(1100,2000,140,&plan,&surface,&valley,&bank);struct AtlasBankProfile before=bank;
    struct RiverPiece swap=sPieces[0];sPieces[0]=sPieces[1];sPieces[1]=swap;
    AtlasWater_SampleBank(1100,2000,140,&plan,&surface,&valley,&bank);assert(!memcmp(&before,&bank,sizeof(bank)));
    puts("PASS wet floodplain: raised levee, lower/wetter backswamp, rainfall/vapor/lake/width/creek gates, old-version preservation and overlap order");
}
