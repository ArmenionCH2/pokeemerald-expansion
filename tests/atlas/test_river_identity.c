#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_place.h"
#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static struct AtlasPlanNode prior[ATLAS_PLAN_COUNT];
static u16 strongest[ATLAS_PLAN_COUNT];
static bool8 Eligible(unsigned id)
{
    const struct AtlasPlanNode *n=&gAtlasPlan[id];
    return n->height>=ATLAS_PLAN_SEA && !(n->basin&ATLAS_LAKE_FLAG) && n->runoff>=240 && n->parent<ATLAS_PLAN_COUNT && n->parent!=id;
}
static void Oracle(void)
{
    memset(strongest,255,sizeof(strongest));
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)if(Eligible(id))
    {
        u16 parent=gAtlasPlan[id].parent,best=strongest[parent];
        if(best==0xffff || gAtlasPlan[id].runoff>gAtlasPlan[best].runoff || (gAtlasPlan[id].runoff==gAtlasPlan[best].runoff && id<best))strongest[parent]=id;
    }
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
    {
        unsigned expected=0;
        if(Eligible(id))
        {
            unsigned at=id;
            for(unsigned step=0;step<ATLAS_PLAN_COUNT;step++)
            {
                unsigned parent=gAtlasPlan[at].parent;
                if(gAtlasPlan[parent].height<ATLAS_PLAN_SEA || (gAtlasPlan[parent].basin&ATLAS_LAKE_FLAG) || strongest[parent]!=at){expected=at+1;break;}
                at=parent;
            }
        }
        assert(AtlasPlan_RiverRoot(id)==expected);
    }
}
int main(void)
{
    setbuf(stdout,NULL);gAtlasPlanReady=TRUE;
    unsigned join=24*96+20,west=join-1,north=join-96,out=join+1,sea=join+2;
    gAtlasPlan[west]=(struct AtlasPlanNode){.height=145,.runoff=900,.parent=join};
    gAtlasPlan[north]=(struct AtlasPlanNode){.height=145,.runoff=400,.parent=join};
    gAtlasPlan[join]=(struct AtlasPlanNode){.height=140,.runoff=1300,.parent=out};
    gAtlasPlan[out]=(struct AtlasPlanNode){.height=130,.runoff=1300,.parent=sea};
    assert(AtlasPlan_RiverRoot(west)==out+1 && AtlasPlan_RiverRoot(north)==north+1);
    gAtlasPlan[north].runoff=900;
    assert(AtlasPlan_RiverRoot(north)==out+1 && AtlasPlan_RiverRoot(west)==west+1);
    gAtlasPlan[join].basin=ATLAS_LAKE_FLAG;
    assert(AtlasPlan_RiverRoot(west)==west+1 && !AtlasPlan_RiverRoot(join) && AtlasPlan_RiverRoot(out)==out+1);
    memset(gAtlasPlan,0,sizeof(gAtlasPlan));
    unsigned a=24*96+95,b=24*96;
    gAtlasPlan[a]=(struct AtlasPlanNode){.height=140,.runoff=900,.parent=b};
    gAtlasPlan[b]=(struct AtlasPlanNode){.height=130,.runoff=900,.parent=b+1};
    assert(AtlasPlan_RiverRoot(a)==b+1);
    assert(!AtlasPlan_RiverRoot(ATLAS_PLAN_COUNT));gAtlasPlanReady=FALSE;assert(!AtlasPlan_RiverRoot(a));
    const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    unsigned outflows=0;
    for(unsigned s=0;s<6;s++)for(unsigned version=3;version<=26;version+=23)
    {
        assert(AtlasWorld_SetGeneration(version)&&AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));
        memcpy(prior,gAtlasPlan,sizeof(prior));Oracle();unsigned named=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)if(Eligible(id) && gAtlasPlan[id].runoff>=600)
        {
            s32 x,y;u16 h;struct AtlasCell c;struct AtlasPlace place;char name[21];
            AtlasWater_Point(id,2,&x,&y,&h);AtlasTerrain_Sample(x,y,&c);
            if(c.waterform!=ATLAS_RIVER)continue;
            u16 expected=AtlasPlan_RiverRoot(id);
            struct WaterBucket buckets[4];memcpy(buckets,sBuckets,sizeof(buckets));
            u32 clock=sClock;u8 active=sActive;
            assert(AtlasWater_RiverNameId(x,y,0)==expected);
            assert(clock==sClock && active==sActive && !memcmp(buckets,sBuckets,sizeof(buckets)));
            assert(AtlasWater_RiverNameId(x-ATLAS_WORLD_WIDTH,y,0)==expected);
            assert(AtlasPlace_Identify(x,y,&c,&place) && place.kind==ATLAS_PLACE_RIVER && place.landmark==expected);
            assert(AtlasPlace_Name(&place,2,name) && strstr(name," River"));named++;
            if(named>=12)break;
        }
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            const struct AtlasPlanNode *n=&gAtlasPlan[id];
            if(!(n->basin&ATLAS_LAKE_FLAG) || n->runoff<600 || n->parent>=ATLAS_PLAN_COUNT || !Eligible(n->parent))continue;
            for(unsigned part=1;part<4;part++)
            {
                s32 x,y;u16 h;struct AtlasCell c;
                AtlasWater_Point(id,part,&x,&y,&h);AtlasTerrain_Sample(x,y,&c);
                if(c.waterform!=ATLAS_RIVER)continue;
                assert(AtlasWater_RiverNameId(x,y,0)==AtlasPlan_RiverRoot(n->parent));outflows++;
            }
        }
        assert(named && !memcmp(prior,gAtlasPlan,sizeof(prior)));
        AtlasWater_Reset();assert(!AtlasWater_RiverNameId(0,0,4) && !sClock);
        assert(!AtlasWater_RiverNameId(INT_MIN,INT_MIN,4));
        assert(!AtlasWater_RiverNameId(0,ATLAS_WORLD_HALF_HEIGHT,4));
        printf("PASS river identity seed=%u generation=%u: independent all-node trunk oracle, %u named channels, wrapped lookup and immutable plan\n",seeds[s],version,named);
    }
    assert(outflows);printf("PASS %u actual lake-outflow river samples retain their downstream river identity\n",outflows);
}
