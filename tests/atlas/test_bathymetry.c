/* Real creation code with controlled initial-height experiments. Private phase
 * flags distinguish these synthetic inputs from imported final recipe notes. */
#include "../../src/atlas_plan.c"
#include "atlas_ocean.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode before[ATLAS_PLAN_COUNT],after[ATLAS_PLAN_COUNT];
static u8 heights[ATLAS_PLAN_COUNT];
static void Finish(unsigned batch)
{
    unsigned last=AtlasPlan_Progress(),steps=0;
    while(!AtlasPlan_Step(batch))
    {unsigned p=AtlasPlan_Progress();assert(p>=last && p<=100);last=p;assert(++steps<200000);}
    assert(AtlasPlan_Progress()==100);
}
static void Generate(u32 seed,unsigned version,unsigned batch)
{
    assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(seed));Finish(batch);
}
int main(void)
{
    setbuf(stdout,NULL);
    assert(AtlasWorld_SetGeneration(11) && AtlasPlan_Begin(42));
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)gAtlasPlan[id].height=0;
    gAtlasPlan[24*ATLAS_PLAN_WIDTH].height=150;
    for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)heights[id]=AtlasOcean_Height(id);
    for(unsigned id=ATLAS_PLAN_COUNT;id--;)
    {gAtlasPlan[id].height=AtlasOcean_Height(id);assert(gAtlasPlan[id].height==heights[id]);}
    assert(heights[24*ATLAS_PLAN_WIDTH]==150);
    assert(heights[24*ATLAS_PLAN_WIDTH+95]>=110 && heights[24*ATLAS_PLAN_WIDTH+95]<121);
    assert(heights[24*ATLAS_PLAN_WIDTH+94]>=75 && heights[24*ATLAS_PLAN_WIDTH+94]<100);
    u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned seed=0;seed<sizeof(seeds)/sizeof(seeds[0]);seed++)
    {
        Generate(seeds[seed],10,256);memcpy(before,gAtlasPlan,sizeof(before));
        Generate(seeds[seed],11,31);memcpy(after,gAtlasPlan,sizeof(after));
        if(!seed)
        {
            FILE *f=fopen("build/generation11-before.bin","wb");assert(f);assert(fwrite(before,sizeof(before),1,f)==1);fclose(f);
            f=fopen("build/generation11-after.bin","wb");assert(f);assert(fwrite(after,sizeof(after),1,f)==1);fclose(f);
        }
        unsigned shelves=0,slopes=0,deep=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            struct AtlasPlanNode *n=&gAtlasPlan[id];
            assert((n->height>=121)==(before[id].height>=121));assert(n->continent==before[id].continent);
            if(n->height<121){assert(n->height>=6);shelves+=n->height>=110;slopes+=n->height>=75 && n->height<110;deep+=n->height<75;}
            heights[id]=n->height;
        }
        assert(shelves && slopes && deep && gAtlasPlanStats.land==1345);
        u32 checksum=gAtlasPlanStats.checksum;
        assert(AtlasPlan_Begin(seeds[seed]) && AtlasPlan_ImportHeights(heights));Finish(1);
        assert(gAtlasPlanStats.checksum==checksum && !memcmp(after,gAtlasPlan,sizeof(after)));
        Generate(seeds[seed],11,256);assert(!memcmp(after,gAtlasPlan,sizeof(after)));
        printf("PASS generation11 seed=%u shelf=%u mid_depth=%u deep=%u checksum=%08x: unchanged macro mask/components, exact batch replay and final-height import\n",seeds[seed],shelves,slopes,deep,checksum);
    }
    u32 habitat=0;
    for(unsigned pass=0;pass<2;pass++)
    {
        assert(AtlasPlan_Begin(123));
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
            heights[id]=id%ATLAS_PLAN_WIDTH<16?(pass?120:0):130+(id%8)*12;
        assert(AtlasPlan_ImportHeights(heights));
        sEroded=FALSE;sOceanReady=TRUE; // controlled initial heights, retain experimental ocean depth
        Finish(31);
        if(!pass){memcpy(before,gAtlasPlan,sizeof(before));habitat=gAtlasPlanStats.habitatChecksum;}
        else
        {
            for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
            {
                struct AtlasPlanNode *a=&before[id],*b=&gAtlasPlan[id];
                if(a->height>=121)assert(a->height==b->height);
                assert(a->parent==b->parent && a->runoff==b->runoff && a->continent==b->continent
                    && a->basin==b->basin && a->rain==b->rain && a->humidity==b->humidity && a->water==b->water);
            }
            assert(habitat==gAtlasPlanStats.habitatChecksum);
        }
    }
    puts("PASS ocean depth does not manufacture coastal atmospheric uplift or change erosion, routing, runoff and spill planes");
}
