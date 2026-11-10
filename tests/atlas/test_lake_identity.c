/* Independent connected-component oracle against the real stage-8 planner. */
#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_place.h"
#include "atlas_geo.h"
#include "../../src/atlas_plan.c"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static u16 labels[ATLAS_PLAN_COUNT],queue[ATLAS_PLAN_COUNT];
static unsigned Verify(void)
{
    memset(labels,0,sizeof(labels));unsigned count=0;
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
    {
        if(!(gAtlasPlan[id].basin&ATLAS_LAKE_FLAG) || labels[id])continue;
        count++;unsigned begin=0,end=1;queue[0]=id;labels[id]=id+1;
        while(begin<end)
        {
            unsigned at=queue[begin++],x=at%ATLAS_PLAN_WIDTH,y=at/ATLAS_PLAN_WIDTH;
            unsigned adjacent[4]={y*ATLAS_PLAN_WIDTH+(x+1)%ATLAS_PLAN_WIDTH,
                y*ATLAS_PLAN_WIDTH+(x+ATLAS_PLAN_WIDTH-1)%ATLAS_PLAN_WIDTH,
                y?at-ATLAS_PLAN_WIDTH:at,y+1<ATLAS_PLAN_HEIGHT?at+ATLAS_PLAN_WIDTH:at};
            for(unsigned d=0;d<4;d++)
            {
                unsigned next=adjacent[d];
                if(labels[next] || !(gAtlasPlan[next].basin&ATLAS_LAKE_FLAG) || gAtlasPlan[next].water!=gAtlasPlan[at].water)continue;
                labels[next]=id+1;queue[end++]=next;
            }
        }
    }
    assert(count==gAtlasPlanStats.lakes);
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)assert(LakeRoot(id)==labels[id]);
    assert(!LakeRoot(ATLAS_PLAN_COUNT));
    return count;
}
static void FinishFixture(void)
{
    sStage=8;sAt=0;sLabeling=FALSE;while(!AtlasPlan_Step(17));
}
int main(void)
{
    setbuf(stdout,NULL);assert(AtlasWorld_SetGeneration(25));
    assert(AtlasPlan_Begin(0));
    /* Worst occupancy: one lake through all packing offsets and polar rows. */
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)gAtlasPlan[id]=(struct AtlasPlanNode){.height=140,.water=150,.basin=ATLAS_LAKE_FLAG};
    FinishFixture();assert(Verify()==1);
    assert(AtlasPlan_LakeId(INT_MIN,INT_MIN)==0);
    assert(AtlasPlan_LakeId(0,ATLAS_WORLD_HALF_HEIGHT)==0);
    assert(AtlasPlan_Begin(1));
    assert(!AtlasPlan_LakeId(0,0));
    const unsigned nodes[]={0,95,96,193,194,288,4607};
    for(unsigned i=0;i<sizeof(nodes)/sizeof(nodes[0]);i++)
        gAtlasPlan[nodes[i]]=(struct AtlasPlanNode){.height=140,.water=i==4?151:150,.basin=ATLAS_LAKE_FLAG};
    FinishFixture();assert(Verify()==5);
    assert(labels[0]==labels[95] && labels[0]==labels[96]);
    assert(labels[193]!=labels[194] && labels[193]!=labels[288]);
    assert(labels[0]!=labels[4607]); /* Latitude does not wrap. */
    const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned s=0;s<6;s++)
    {
        assert(AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));
        unsigned count=Verify(),named=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)if(labels[id])
        {
            s32 x=((u32)(id%96)*ATLAS_WORLD_WIDTH+95)/96;
            s32 y=((u32)(id/96)*(2*ATLAS_WORLD_HALF_HEIGHT)+46)/47-ATLAS_WORLD_HALF_HEIGHT;
            if(y>=ATLAS_WORLD_HALF_HEIGHT)y=ATLAS_WORLD_HALF_HEIGHT-1;
            assert(AtlasPlan_LakeId(x,y)==labels[id]);
            assert(AtlasPlan_LakeId(x-ATLAS_WORLD_WIDTH,y)==labels[id]);
            struct AtlasCell cell;struct AtlasPlace place;char name[21],again[21];
            AtlasTerrain_Sample(x,y,&cell);
            if(cell.waterform!=ATLAS_LAKE)continue;
            assert(AtlasPlace_Identify(x,y,&cell,&place) && place.landmark==labels[id]);
            assert(AtlasPlace_Name(&place,2,name) && strstr(name," Lake"));
            AtlasPlan_ResetSample();assert(AtlasPlan_LakeId(x,y)==labels[id]);
            assert(AtlasPlace_Name(&place,2,again) && !strcmp(name,again));named++;
        }
        assert(named);
        AtlasGeo_Begin(ATLAS_GEO_LAKE);unsigned steps=0;
        while(!AtlasGeo_Step())assert(++steps<ATLAS_GEO_SEARCH_STEPS);
        assert(gAtlasGeoStatus==1);
        struct AtlasPlace shore={.kind=ATLAS_PLACE_LAKE};
        AtlasPlace_SetLandmark(&shore,gAtlasGeoResult.x,gAtlasGeoResult.y);
        assert(shore.landmark);
        printf("PASS lake identity seed=%u: %u components, %u actual named water samples, safe named shore\n",seeds[s],count,named);
    }
    puts("PASS lake component topology, dateline/poles, adjacent spill levels, diagonal separation, full occupancy, reset and cached owner continuity");
}
