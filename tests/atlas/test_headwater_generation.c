#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
static unsigned groundCalls;
static u8 CountGround(s32 x,s32 y,struct AtlasPlanSample *p);
#define AtlasTerrain_GroundHeight CountGround
#include "../../src/atlas_water.c"
#undef AtlasTerrain_GroundHeight
static u8 CountGround(s32 x,s32 y,struct AtlasPlanSample *p)
{groundCalls++;return AtlasTerrain_GroundHeight(x,y,p);}
static struct AtlasPlanNode old[ATLAS_PLAN_COUNT];
static struct {s32 x,y;struct AtlasCell cell;} wet[128];
struct Channel {s32 ax,ay,bx,by;u16 ha,hb,width,flags;};
static void Generate(u32 seed,u16 version)
{assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
static void Warm(s32 x,s32 y)
{
    for(unsigned step=0;;step++)
    {
        unsigned before=groundCalls;bool8 ready=AtlasWater_Prewarm(x,y);
        assert(groundCalls-before<=1 && step<20000);if(ready)return;
    }
}
static struct Channel Global(const struct WaterBucket *b,const struct RiverPiece *p)
{
    struct Channel out={b->x*WATER_BUCKET+p->ax,b->y*WATER_BUCKET+p->ay,
        b->x*WATER_BUCKET+p->bx,b->y*WATER_BUCKET+p->by,p->ha,p->hb,p->width,p->widthB};return out;
}
static void AssertResident(const struct Channel *channel,s32 x,s32 y)
{
    Warm(x,y);bool8 found=FALSE;
    for(unsigned i=0;i<sPieceCount;i++)
    {
        struct Channel other=Global(&sBuckets[sActive],&sPieces[i]);
        if(other.ax==channel->ax && other.ay==channel->ay && other.bx==channel->bx && other.by==channel->by)
        {assert(other.ha==channel->ha && other.hb==channel->hb && other.width==channel->width && other.flags==channel->flags);found=TRUE;}
    }
    assert(found);
}
int main(void)
{
    setbuf(stdout,NULL);u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<6;k++)
    {
        Generate(seeds[k],20);memcpy(old,gAtlasPlan,sizeof(old));unsigned wetCount=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT && wetCount<128;id++)
        {
            if(!AtlasWater_CreekSource(id))continue;
            s32 x,y;u16 h;AtlasWater_Point(id,2,&x,&y,&h);Warm(x,y);
            struct AtlasCell cell;AtlasTerrain_Sample(x,y,&cell);
            if(cell.waterform==ATLAS_RIVER){wet[wetCount].x=x;wet[wetCount].y=y;wet[wetCount++].cell=cell;}
            if(wetCount>=128)break;
            for(unsigned attempt=0;attempt<128;attempt++)
            {
                if(!AtlasWater_CreekProbe(id,attempt,&x,&y))continue;
                Warm(x,y);AtlasTerrain_Sample(x,y,&cell);
                if(cell.waterform==ATLAS_RIVER && AtlasWater_CreekNear(x,y,0))
                {wet[wetCount].x=x;wet[wetCount].y=y;wet[wetCount++].cell=cell;break;}
            }
        }
        assert(wetCount);Generate(seeds[k],21);assert(!memcmp(old,gAtlasPlan,sizeof(old)));
        for(unsigned i=0;i<wetCount;i++)
        {
            Warm(wet[i].x,wet[i].y);struct AtlasCell cell;AtlasTerrain_Sample(wet[i].x,wet[i].y,&cell);
            assert(cell.waterform==wet[i].cell.waterform && cell.elevation==wet[i].cell.elevation);
        }
        struct Channel channels[3];unsigned found=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT && found<3;id++)
        {
            if(!AtlasWater_CreekSource(id))continue;
            for(unsigned attempt=0;attempt<128 && found<3;attempt++)
            {
                s32 x,y;if(!AtlasWater_CreekProbe(id,attempt,&x,&y))continue;Warm(x,y);
                for(unsigned i=0;i<sPieceCount && found<3;i++)
                {
                    if((sPieces[i].widthB&(CREEK_FLAG|HEADWATER_FLAG))!=(CREEK_FLAG|HEADWATER_FLAG))continue;
                    struct Channel channel=Global(&sBuckets[sActive],&sPieces[i]);bool8 duplicate=FALSE;
                    for(unsigned j=0;j<found;j++)if(channels[j].ax==channel.ax && channels[j].ay==channel.ay)duplicate=TRUE;
                    if(!duplicate)channels[found++]=channel;
                }
            }
        }
        printf("headwaters seed=%u found=%u old wet samples=%u\n",seeds[k],found,wetCount);assert(found);
        for(unsigned j=0;j<found;j++)
        {
            const struct Channel *p=&channels[j];assert(p->ha>p->hb && p->width>=1 && p->width<=3);
            AssertResident(p,p->ax,p->ay);AssertResident(p,p->bx,p->by);
            unsigned prior=255;struct AtlasCell samples[65];
            for(unsigned i=0;i<65;i++)
            {
                s32 x=p->ax+(s64)(p->bx-p->ax)*i/64,y=p->ay+(s64)(p->by-p->ay)*i/64;
                Warm(x,y);AtlasTerrain_Sample(x,y,&samples[i]);
                assert(samples[i].waterform==ATLAS_RIVER && samples[i].elevation<=prior);prior=samples[i].elevation;
                u32 stamp=sClock,before=groundCalls;assert(AtlasWater_HeadwaterNear(x,y,0));assert(stamp==sClock && before==groundCalls);
            }
            AtlasWater_Reset();
            for(int i=64;i>=0;i--)
            {
                s32 x=p->ax+(s64)(p->bx-p->ax)*i/64,y=p->ay+(s64)(p->by-p->ay)*i/64;
                Warm(x,y);struct AtlasCell after;AtlasTerrain_Sample(x,y,&after);assert(!memcmp(&samples[i],&after,sizeof(after)));
            }
        }
        AtlasGeo_Begin(ATLAS_GEO_HEADWATER);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        printf("headwater inspection seed=%u status=%u steps=%u\n",seeds[k],gAtlasGeoStatus,steps);assert(gAtlasGeoStatus==1);
        assert(AtlasWater_HeadwaterNear(gAtlasGeoResult.x,gAtlasGeoResult.y,8));
        for(s32 y=-1;y<=1;y++)for(s32 x=-1;x<=1;x++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+x,gAtlasGeoResult.y+y)&0xC00));
        printf("PASS generation21 seed=%u: real second-level tributaries, descending sampled joins, canonical cross-bucket endpoints, reverse query replay, protected parent water and safe inspector\n",seeds[k]);
    }
}
