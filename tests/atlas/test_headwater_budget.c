#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady=TRUE;
static unsigned calls;
u16 AtlasWorld_Generation(void){return 21;}
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return q-(n%d<0);}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *p)
{calls++;memset(p,0,sizeof(*p));p->height=145;p->rain=255;return 145;}
static unsigned maxPieces,maxCreeks;
static void Check(void)
{
    unsigned creeks=0,parents=0,children=0,phase=0;
    for(unsigned i=0;i<sPieceCount;i++)
    {
        const struct RiverPiece *p=&sPieces[i];
        if(!(p->widthB&CREEK_FLAG)){assert(!phase);continue;}
        creeks++;
        if(p->widthB&HEADWATER_FLAG){phase=2;children++;assert(TipWidth(p)==p->width && p->width<=3);}
        else{assert(phase<2);phase=1;parents++;}
    }
    assert(parents<=32 && children<=parents*2 && creeks<=96 && sPieceCount<WATER_SEGMENTS);
    if(sPieceCount>maxPieces)maxPieces=sPieceCount;
    if(children>maxCreeks)maxCreeks=children;
}
static void Warm(s32 x,s32 y)
{
    for(unsigned step=0;;step++)
    {
        unsigned before=calls;bool8 ready=AtlasWater_Prewarm(x,y);assert(calls-before<=1 && step<10000);
        if(ready){Check();return;}
    }
}
struct Result {u8 water,surface,valley,height,wetness;};
static struct Result ReadResult(s32 x,s32 y)
{
    struct AtlasPlanSample plan={145,96,96,121,0};struct AtlasBankProfile bank;struct Result out={0};
    out.water=AtlasWater_SampleBank(x,y,145,&plan,&out.surface,&out.valley,&bank);
    out.height=bank.height;out.wetness=bank.wetness;return out;
}
static void CheckWrap(void)
{
    struct WaterBucket west={0},east={0};east.x=(ATLAS_WORLD_WIDTH-1)/WATER_BUCKET;
    struct RiverPiece a={-3000,0,3000,1000,130*256,124*256,8,CREEK_FLAG|(32<<8)|8},b=a;
    b.ax=ATLAS_WORLD_WIDTH-3000-east.x*WATER_BUCKET;
    b.bx=ATLAS_WORLD_WIDTH+3000-east.x*WATER_BUCKET;
    unsigned checked=0;
    for(unsigned seed=0;seed<64;seed++)for(unsigned slot=0;slot<2;slot++)
    {
        gAtlasWorldSeed=seed;struct CreekLine x,y;
        bool8 left=HeadwaterGeometry(&west,&a,slot,&x),right=HeadwaterGeometry(&east,&b,slot,&y);
        assert(left==right);if(!left)continue;
        assert(y.ax==x.ax+ATLAS_WORLD_WIDTH && y.bx==x.bx+ATLAS_WORLD_WIDTH);
        assert(y.ay==x.ay && y.by==x.by && y.ha==x.ha && y.hb==x.hb && y.width==x.width && y.join==x.join);checked++;
    }
    assert(checked);puts("PASS headwater geometry/receiving height is periodic across the planet's partial final water bucket");
}
int main(void)
{
    setbuf(stdout,NULL);CheckWrap();
    unsigned centre=24*ATLAS_PLAN_WIDTH+48;
    u16 owners[]={centre-1,centre-ATLAS_PLAN_WIDTH,centre+ATLAS_PLAN_WIDTH,centre,centre+1};
    for(unsigned i=0;i<5;i++)
    {
        struct AtlasPlanNode *n=&gAtlasPlan[owners[i]];
        n->height=145;n->water=i<3?125:i==3?124:123;n->runoff=5000;n->rain=255;
        n->parent=i<3?centre:i==3?centre+1:centre+2;
    }
    s32 x,y;u16 h;AtlasWater_Point(centre,0,&x,&y,&h);
    for(unsigned seed=0;seed<64;seed++)
    {
        gAtlasWorldSeed=seed;AtlasWater_Reset();
        for(unsigned owner=0;owner<5;owner++)for(unsigned part=1;part<4;part++)
        {
            s32 px,py;AtlasWater_Point(owners[owner],part,&px,&py,&h);Warm(px,py);
            for(unsigned side=0;side<2;side++)
            {
                if(AtlasWater_CreekProbe(owners[owner],part*32+10+side,&px,&py))Warm(px,py);
            }
        }
        s32 px[4]={x-4096,x+4096,x-4096,x+4096},py[4]={y-4096,y-4096,y+4096,y+4096};
        struct Result serial[4];
        for(unsigned i=0;i<4;i++){Warm(px[i],py[i]);serial[i]=ReadResult(px[i],py[i]);}
        AtlasWater_Reset();bool8 ready[4]={0};unsigned left=4;
        for(unsigned step=0;left;step++)
        {
            assert(step<40000);unsigned i=step%4;if(ready[i])continue;
            unsigned before=calls;ready[i]=AtlasWater_Prewarm(px[i],py[i]);assert(calls-before<=1);
            if(ready[i]){left--;Check();}
        }
        for(unsigned i=0;i<4;i++){struct Result interleaved=ReadResult(px[i],py[i]);assert(!memcmp(&serial[i],&interleaved,sizeof(interleaved)));}
    }
    assert(maxCreeks);
    printf("PASS 64 dense rainy drainage fixtures: max %u pieces/%u headwaters, main/parent/child ownership order, one ground probe per step, four-frontier interleaving equals serial preparation\n",maxPieces,maxCreeks);
}

bool8 AtlasPlan_CanyonDrain(u16 node){return FALSE;}
