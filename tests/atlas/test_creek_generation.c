#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static unsigned groundCalls;
static u8 CountGround(s32 x,s32 y,struct AtlasPlanSample *plan);
#define AtlasTerrain_GroundHeight CountGround
#include "../../src/atlas_water.c"
#undef AtlasTerrain_GroundHeight
static u8 CountGround(s32 x,s32 y,struct AtlasPlanSample *plan)
{groundCalls++;return AtlasTerrain_GroundHeight(x,y,plan);}
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static struct {s32 x,y;struct AtlasCell cell;} water[128];
static struct {s32 x,y;struct RiverPiece piece;struct AtlasCell cell;} found[3];
static void Generate(u32 seed,u16 version)
{assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
static void Warm(s32 x,s32 y)
{
    unsigned steps=0;
    while(1)
    {
        unsigned before=groundCalls;bool8 ready=AtlasWater_Prewarm(x,y);
        assert(groundCalls-before<=1 && ++steps<10000);
        if(ready)return;
    }
}
int main(void)
{
    setbuf(stdout,NULL);
    u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned seed=0;seed<6;seed++)
    {
        groundCalls=0;
        Generate(seeds[seed],17);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        unsigned original=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT && original<128;id++)
        {
            const struct AtlasPlanNode *n=&gAtlasPlan[id];
            if(n->height<ATLAS_PLAN_SEA || n->parent>=ATLAS_PLAN_COUNT || n->parent==id || (n->runoff<600 && !AtlasWater_IsSmallRiver(id)))continue;
            u16 h;AtlasWater_Point(id,2,&water[original].x,&water[original].y,&h);
            AtlasTerrain_Sample(water[original].x,water[original].y,&water[original].cell);original++;
        }
        Generate(seeds[seed],18);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        for(unsigned i=0;i<original;i++)
        {
            struct AtlasCell after;Warm(water[i].x,water[i].y);AtlasTerrain_Sample(water[i].x,water[i].y,&after);
            assert(after.waterform==water[i].cell.waterform && after.elevation==water[i].cell.elevation);
        }
        unsigned count=0,attempts=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT && count<3;id++)
        {
            if(!AtlasWater_CreekSource(id))continue;
            for(unsigned attempt=0;attempt<128 && count<3;attempt++)
            {
                s32 x,y;if(!AtlasWater_CreekProbe(id,attempt,&x,&y))continue;
                attempts++;Warm(x,y);if(!AtlasWater_CreekNear(x,y,0))continue;
                s32 canonical=x%ATLAS_WORLD_WIDTH;if(canonical<0)canonical+=ATLAS_WORLD_WIDTH;
                for(unsigned p=0;p<sPieceCount;p++)
                {
                    const struct RiverPiece *piece=&sPieces[p];
                    if(!(piece->widthB&CREEK_FLAG))continue;
                    s32 midX=sBucketX*WATER_BUCKET+(piece->ax+piece->bx)/2,midY=sBucketY*WATER_BUCKET+(piece->ay+piece->by)/2;
                    if((midX-canonical< -1 || midX-canonical>1) || (midY-y< -1 || midY-y>1))continue;
                    assert(piece->width>=2 && piece->width<=10 && piece->ha>=piece->hb+64 && piece->ha<=piece->hb+6*256);
                    found[count].x=x;found[count].y=y;found[count].piece=*piece;
                    struct RiverPiece record=*piece;
                    s32 bx=sBucketX*WATER_BUCKET,by=sBucketY*WATER_BUCKET;
                    unsigned previous=255;
                    for(unsigned part=0;part<=32;part++)
                    {
                        s32 px=bx+record.ax+(s64)(record.bx-record.ax)*part/32;
                        s32 py=by+record.ay+(s64)(record.by-record.ay)*part/32;
                        struct AtlasCell cell;Warm(px,py);AtlasTerrain_Sample(px,py,&cell);
                        assert(cell.waterform && cell.elevation<=previous);previous=cell.elevation;
                    }
                    Warm(x,y);AtlasTerrain_Sample(x,y,&found[count].cell);assert(found[count].cell.waterform==ATLAS_RIVER);
                    count++;break;
                }
            }
        }
        printf("creek seed=%u found=%u tested_geometry=%u ground_probes=%u\n",seeds[seed],count,attempts,groundCalls);
        assert(count==3);
        AtlasGeo_Begin(ATLAS_GEO_CREEK);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        printf("creek TP status=%u steps=%u\n",gAtlasGeoStatus,steps);
        assert(gAtlasGeoStatus==1 && AtlasWater_CreekNear(gAtlasGeoResult.x,gAtlasGeoResult.y,8));
        for(unsigned i=0;i<9;i++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+(s32)(i%3)-1,gAtlasGeoResult.y+(s32)(i/3)-1)&0xC00));
        Generate(seeds[seed],17);unsigned added=0;
        for(unsigned i=0;i<count;i++)
        {
            struct AtlasCell before;AtlasTerrain_Sample(found[i].x,found[i].y,&before);added+=!before.waterform;
        }
        assert(added);
        printf("PASS generation18 seed=%u: actual local creeks beyond macro channels, non-uphill sampled joins, preserved original wet surfaces/macro notes, yielded one-ground-probe preparation and safe creek inspection\n",seeds[seed]);
    }
}
