#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>

static struct AtlasPlanNode legacy[ATLAS_PLAN_COUNT];
static struct AtlasPlanNode balanced[ATLAS_PLAN_COUNT];
static void Generate(u32 seed,u16 version,u16 batch)
{
    assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(seed));
    while (!AtlasPlan_Step(batch)) {}
}
int main(void)
{
    const u32 seeds[]={0,1,0xFFFFFFFFu,0xDEADBEEFu,4220706268u,123456789};
    const u32 version5[]={0xfa6b6928u,0xe2628f10u,0x88be887eu,0x1e0b8eb0u,0x646bfdaeu,0x0cf60e1eu};
    unsigned i,id;
    Generate(0,3,256);
    assert(gAtlasPlanStats.checksum==0xEDDF7C74u);
    memcpy(legacy,gAtlasPlan,sizeof(legacy));
    Generate(0,4,256);assert(gAtlasPlanStats.checksum==0x718DD91Eu);memcpy(balanced,gAtlasPlan,sizeof(balanced));
    for (i=0;i<sizeof(seeds)/sizeof(seeds[0]);i++)
    {
        unsigned land=0,mountains=0;
        Generate(seeds[i],5,256);assert(gAtlasPlanStats.checksum==version5[i]);
        Generate(seeds[i],6,256);
        u32 climateChecksum=gAtlasPlanStats.checksum;
        Generate(seeds[i],7,256);assert(gAtlasPlanStats.checksum==climateChecksum);
        for (id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            u16 at=id;unsigned steps=0;
            land+=gAtlasPlan[id].height>=ATLAS_PLAN_SEA;
            mountains+=gAtlasPlan[id].height>=195;
            assert(!!(gAtlasPlan[id].continent&ATLAS_OCEAN_COMPONENT)==(gAtlasPlan[id].height<ATLAS_PLAN_SEA));
            u16 east=(id/ATLAS_PLAN_WIDTH)*ATLAS_PLAN_WIDTH+(id+1)%ATLAS_PLAN_WIDTH;
            if (gAtlasPlan[id].height<ATLAS_PLAN_SEA && gAtlasPlan[east].height<ATLAS_PLAN_SEA)
                assert(gAtlasPlan[id].continent==gAtlasPlan[east].continent);
            assert(gAtlasPlan[id].parent<ATLAS_PLAN_COUNT);
            while (gAtlasPlan[at].parent!=at)
            {at=gAtlasPlan[at].parent;assert(++steps<ATLAS_PLAN_COUNT);}
            assert(gAtlasPlan[at].height<ATLAS_PLAN_SEA);
        }
        assert(land==ATLAS_PLAN_COUNT*292u/1000 && gAtlasPlanStats.land==land && mountains);
        unsigned sizes[ATLAS_PLAN_COUNT+1]={0},largest=0,large=0;
        for (id=0;id<ATLAS_PLAN_COUNT;id++)
            if (gAtlasPlan[id].height>=ATLAS_PLAN_SEA) sizes[gAtlasPlan[id].continent]++;
        for (id=1;id<=ATLAS_PLAN_COUNT;id++) {if (sizes[id]>largest) largest=sizes[id];if (sizes[id]>=80) large++;}
        printf("Continental structure seed=%u largest=%u substantial=%u\n",seeds[i],largest,large);
        assert(largest>=180 && large>=2);
        u32 checksum=gAtlasPlanStats.checksum;
        Generate(seeds[i],7,31);assert(checksum==gAtlasPlanStats.checksum);
        s32 x,y;assert(AtlasTerrain_FindSpawn(&x,&y));
        for (unsigned p=0;p<9;p++) assert(!(AtlasTerrain_Tile(x+(s32)(p%3)-1,y+(s32)(p/3)-1)&0xC00));
        printf("PASS ocean generation seed=%u land=%u/%u mountains=%u checksum=%08x\n",seeds[i],land,ATLAS_PLAN_COUNT,mountains,checksum);
    }
    assert(!AtlasWorld_SetGeneration(99) && AtlasWorld_Generation()==7);
    Generate(0,4,31);assert(!memcmp(balanced,gAtlasPlan,sizeof(balanced)));
    Generate(0,3,31);assert(!memcmp(legacy,gAtlasPlan,sizeof(legacy)));
    puts("PASS generation 7 climate/continental structure, drainage, spawn, batch determinism and unchanged generations 3/4/5/6");
    return 0;
}
