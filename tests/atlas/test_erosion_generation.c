#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode before[ATLAS_PLAN_COUNT],after[ATLAS_PLAN_COUNT];
static u8 heights[ATLAS_PLAN_COUNT];
static void Generate(u32 seed,unsigned version,unsigned batch)
{
    assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(seed));
    unsigned last=0,steps=0;
    while(!AtlasPlan_Step(batch))
    {
        unsigned p=AtlasPlan_Progress();assert(p>=last && p<=100);last=p;assert(++steps<200000);
    }
    assert(AtlasPlan_Progress()==100 && !AtlasPlan_Eroding());
}
int main(void)
{
    setbuf(stdout,NULL);
    u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};unsigned total=0;
    FILE *survey=fopen("build/generation10-erosion.csv","w");assert(survey);
    fprintf(survey,"seed,node,old_height,new_height,runoff\n");
    for(unsigned s=0;s<sizeof(seeds)/sizeof(seeds[0]);s++)
    {
        Generate(seeds[s],9,256);memcpy(before,gAtlasPlan,sizeof(before));
        Generate(seeds[s],10,s?31:1);memcpy(after,gAtlasPlan,sizeof(after));
        unsigned changed=0,rivers=0;
        for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)
        {
            struct AtlasPlanNode *n=&gAtlasPlan[i];
            assert(n->height<=before[i].height);
            assert((n->height>=ATLAS_PLAN_SEA)==(before[i].height>=ATLAS_PLAN_SEA));
            assert(n->continent==before[i].continent);
            if(n->height<before[i].height)
            {
                unsigned parent=before[i].parent;assert(parent<ATLAS_PLAN_COUNT);
                assert(n->height>=ATLAS_PLAN_SEA+1 && n->height>before[parent].height);
                assert(before[i].height-n->height<=12 && before[i].runoff>=600);changed++;
                fprintf(survey,"%u,%u,%u,%u,%u\n",seeds[s],i,before[i].height,n->height,before[i].runoff);
            }
            unsigned at=i,steps=0;
            while(gAtlasPlan[at].parent!=at)
            {assert(gAtlasPlan[at].water>=gAtlasPlan[gAtlasPlan[at].parent].water);at=gAtlasPlan[at].parent;assert(++steps<ATLAS_PLAN_COUNT);}
            assert(gAtlasPlan[at].height<ATLAS_PLAN_SEA);
            rivers+=n->height>=ATLAS_PLAN_SEA && n->runoff>600;
            heights[i]=n->height;
        }
        assert(rivers==gAtlasPlanStats.rivers);total+=changed;
        u32 checksum=gAtlasPlanStats.checksum,habitat=gAtlasPlanStats.habitatChecksum;
        Generate(seeds[s],10,256);assert(!memcmp(after,gAtlasPlan,sizeof(after)));
        assert(AtlasPlan_Begin(seeds[s]) && AtlasPlan_ImportHeights(heights));
        while(!AtlasPlan_Step(31));
        assert(checksum==gAtlasPlanStats.checksum && habitat==gAtlasPlanStats.habitatChecksum);
        assert(!memcmp(after,gAtlasPlan,sizeof(after))); // imported final heights never erode again
        printf("PASS generation10 seed=%u eroded=%u: unchanged land mask/components, downstream base level, monotone progress, rebuilt drainage/statistics, exact batching and final-height import\n",seeds[s],changed);
    }
    assert(total>0);
    fclose(survey);
    Generate(0x9a44e351u,10,256);AtlasGeo_Begin(ATLAS_GEO_FOOTHILLS);
    unsigned search;
    for(search=0;search<6000 && !AtlasGeo_Step();search++);
    assert(search<6000 && gAtlasGeoStatus==1);
    printf("PASS foothill inspection near first contour in %u bounded search steps\n",search);
}
