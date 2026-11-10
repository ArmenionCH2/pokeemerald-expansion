#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady=TRUE;
static u16 version=15;
u16 AtlasWorld_Generation(void){return version;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p)
{assert(!"ready injected profile must not query ground");return 0;}
static void Inject(s32 x,s32 y)
{
    UseBucket(x,y);sBuckets[sActive].ready=TRUE;sPieceCount=0;
    AddTaper(3000,2000,6000,2000,122*256,121*256,40,160);
    assert(sPieceCount==1);
}
int main(void)
{
    struct AtlasPlanSample plan={145,96,96,121,0};
    unsigned checked=0;
    for(s32 x=2800;x<=6200;x+=8)for(s32 y=1800;y<=2200;y+=2)
    {
        Inject(x,y);s32 at=x<3000?0:x>6000?3000:x-3000;
        s32 width=40+120*at/3000,dx=x<3000?3000-x:x>6000?x-6000:0,dy=y-2000;
        bool8 inside=dx*dx+dy*dy<=width*width;
        u8 surface=0,valley;struct AtlasBankProfile bank;
        u8 result=AtlasWater_SampleBank(x,y,145,&plan,&surface,&valley,&bank);
        assert((result==ATLAS_RIVER)==inside);
        if(inside)assert(surface>=121 && surface<=122);
        assert(bank.height<=145 && bank.wetness<=7);
        struct WaterBucket saved[4];memcpy(saved,sBuckets,sizeof(saved));
        unsigned clock=sClock,active=sActive;
        bool8 stroke=AtlasWater_RiverNear(x,y,10);
        assert(stroke==(dx*dx+dy*dy<=(width+10)*(width+10)));
        assert(sClock==clock && sActive==active && !memcmp(saved,sBuckets,sizeof(saved)));
        checked++;
    }
    /* The same retained original ramp gives the same width at a bucket seam. */
    Inject(4095,2000);assert(sPieces[0].ax==3000 && sPieces[0].bx==6000);
    Inject(4096,2000);assert(sPieces[0].ax==3000-4096 && sPieces[0].bx==6000-4096);
    assert(sPieces[0].width==40 && sPieces[0].widthB==160);
    version=14;sPieceCount=0;
    for(unsigned n=0;n<100;n++)AddPiece(3000,2000,6000,2000,121*256,121*256,40);
    assert(sPieceCount==24);
    version=15;sPieceCount=0;
    for(unsigned n=0;n<110;n++)AddPiece(3000,2000,6000,2000,121*256,121*256,40);
    assert(sPieceCount==96);
    printf("PASS %u independent tapered capsule samples, width/water continuity across bucket boundary, read-only cartographic stroke, graded banks, preserved legacy cap and bounded 96-piece admission\n",checked);
}
