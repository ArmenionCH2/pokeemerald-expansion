#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady=TRUE;
u16 AtlasWorld_Generation(void){return 18;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p)
{assert(!"injected ready creek must not query ground");return 0;}
static void Inject(s32 x,s32 y)
{
    UseBucket(x,y);sBuckets[sActive].ready=TRUE;sPieceCount=0;
    AddTaper(2000,3000,6000,3000,129*256,123*256,4,4);
    assert(sPieceCount==1);sPieces[0].widthB=CREEK_FLAG|(32<<8)|4;
}
int main(void)
{
    struct AtlasPlanSample plan={140,96,96,121,0};
    unsigned prior=255,count=0;
    for(s32 x=2000;x<=6000;x++)
    {
        Inject(x,3000);u8 surface=0,valley;
        assert(AtlasWater_Sample(x,3000,140,&plan,&surface,&valley)==ATLAS_RIVER);
        unsigned distance=x-2000;if(distance>3000)distance=3000;
        unsigned expected=(129*256-(6*256*distance)/3000)>>8;
        assert(surface==expected && surface<=prior);prior=surface;
        assert(AtlasWater_CreekNear(x,3000,0));count++;
        for(s32 side=5;side<=100;side+=5)
        {
            Inject(x,3000+side);struct AtlasBankProfile bank;
            assert(AtlasWater_SampleBank(x,3000+side,140,&plan,&surface,&valley,&bank)==ATLAS_DRY);
            struct WaterBucket before[4];memcpy(before,sBuckets,sizeof(before));unsigned clock=sClock,active=sActive;
            assert(AtlasWater_CreekNear(x,3000+side,10)==(side<=14));
            assert(sClock==clock && sActive==active && !memcmp(before,sBuckets,sizeof(before)));
        }
    }
    Inject(3000,3000);sPieceCount=0;
    AddPiece(2000,3000,6000,3000,124*256,124*256,30);
    AddTaper(2000,3000,6000,3000,129*256,123*256,4,4);sPieces[1].widthB=CREEK_FLAG|(32<<8)|4;
    u8 surface,valley;assert(AtlasWater_Sample(3000,3000,140,&plan,&surface,&valley)==ATLAS_RIVER && surface==124);
    printf("PASS %u descending local creek surface points, flat receiving reach, narrow flagged width, read-only creek inspection and protected main-river surface\n",count);
}
