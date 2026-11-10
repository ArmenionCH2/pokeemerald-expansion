#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady=TRUE;
static unsigned calls,mode;static s32 coastX,coastY;
u16 AtlasWorld_Generation(void){return 26;}
bool8 AtlasPlan_CanyonDrain(u16 node){return FALSE;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p)
{
    calls++;memset(p,0,sizeof(*p));
    if(mode==1)return 145;
    if(mode==2)p->lake=1;
    if(mode==3 && y>coastY+150)return 145; /* Only one branch fails. */
    if(mode==4)return 127; /* No receiving sea. */
    if(mode==5 && x>coastX+200)return 127; /* Re-enter land after sea. */
    return x>=coastX?120:127;
}
int main(void)
{
    unsigned id=24*96;gAtlasPlan[id]=(struct AtlasPlanNode){.height=130,.water=130,.runoff=1200,.parent=id+1};
    gAtlasPlan[id+1].height=110;
    while(!AtlasWater_IsEstuary(id))gAtlasWorldSeed++;
    struct WaterBucket fit={.coastHi=COAST_ONE/2,.coastLo=COAST_ONE/2-1};
    CoastPoint(id,fit.coastHi,&coastX,&coastY);
    for(mode=0;mode<=5;mode++)
    {
        struct WaterBucket b=fit;unsigned steps=0;
        for(;;){unsigned before=calls;bool8 ready=DistributaryStep(&b,id);assert(calls-before<=1);assert(++steps<=35);if(ready)break;}
        assert(b.deltaValid==(mode==0));assert(b.deltaProbe==35);
        unsigned before=calls;assert(DistributaryStep(&b,id) && calls==before);
    }
    struct CreekLine line;assert(DistributaryLine(&fit,id,0,&line));
    /* Whole endpoints and physical width survive flag decoding and either bank. */
    UseBucket(line.ax,line.ay);unsigned i=sPieceCount;
    AddTaper(line.ax,line.ay,line.bx,line.by,line.ha,line.hb,line.width,line.width);
    assert(sPieceCount==i+1);sPieces[i].widthB=DISTRIBUTARY_FLAG|line.width;
    assert(TipWidth(&sPieces[i])==line.width && PieceHeight(&sPieces[i],0,1)==ATLAS_PLAN_SEA);
    printf("PASS distributary admission: <=1 ground query/warm step, both branches required, cliff/lake/dry-end/sea-reentry rejection, retained endpoints and flag-safe width\n");
}
