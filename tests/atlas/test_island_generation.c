#include "../../src/atlas_plan.c"
#include "../../src/atlas_place.c"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode snapshot[ATLAS_PLAN_COUNT];
static u16 sizes[ATLAS_PLAN_COUNT+1];
static u8 heights[ATLAS_PLAN_COUNT];
static void Generate(u32 seed,unsigned version,unsigned batch)
{
    assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(seed));
    unsigned last=0,steps=0;
    while(!AtlasPlan_Step(batch))
    {unsigned p=AtlasPlan_Progress();assert(p>=last && p<=100);last=p;assert(++steps<200000);}
}
int main(void)
{
    setbuf(stdout,NULL);
    u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    u32 old[]={0x6c6845fc,0xf511ab39,0x143b350b,0xde403ddf,0x05451e36,0xc1ed2aca};
    for(unsigned seed=0;seed<sizeof(seeds)/sizeof(seeds[0]);seed++)
    {
        Generate(seeds[seed],11,256);assert(gAtlasPlanStats.checksum==old[seed]);
        Generate(seeds[seed],12,31);memcpy(snapshot,gAtlasPlan,sizeof(snapshot));
        assert(sIslandCount && sIslandCount<=96 && gAtlasPlanStats.land==1345);
        memset(sizes,0,sizeof(sizes));unsigned land=0,islands=0,largest=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            struct AtlasPlanNode *n=&gAtlasPlan[id];
            assert(n->parent<ATLAS_PLAN_COUNT);heights[id]=n->height;
            if(n->height>=ATLAS_PLAN_SEA)
            {assert(n->continent && n->continent<=ATLAS_PLAN_COUNT);sizes[n->continent]++;land++;}
            unsigned at=id,steps=0;
            while(gAtlasPlan[at].parent!=at){at=gAtlasPlan[at].parent;assert(++steps<ATLAS_PLAN_COUNT);}
            assert(gAtlasPlan[at].height<ATLAS_PLAN_SEA);
        }
        for(unsigned id=1;id<=gAtlasPlanStats.continents;id++)
        {
            assert(sizes[id]);if(sizes[id]>largest)largest=sizes[id];
            assert(AtlasPlan_IsIsland(id)==(sizes[id]<32));
            if(sizes[id]<32)
            {
                struct AtlasPlace place={0};char name[ATLAS_PLACE_NAME_BYTES];place.landmass=id;
                assert(AtlasPlace_Name(&place,0,name) && strstr(name," Island"));islands++;
            }
        }
        assert(land==1345 && islands && largest>=180);
        Generate(seeds[seed],12,256);assert(!memcmp(snapshot,gAtlasPlan,sizeof(snapshot)));
        assert(AtlasPlan_Begin(seeds[seed]) && AtlasPlan_ImportHeights(heights));
        while(!AtlasPlan_Step(31));
        assert(!memcmp(snapshot,gAtlasPlan,sizeof(snapshot)));
        AtlasGeo_Begin(ATLAS_GEO_ISLAND);unsigned search;
        for(search=0;search<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();search++);
        assert(gAtlasGeoStatus==1 && AtlasPlan_IsIsland(gAtlasGeoResult.landmass));
        for(unsigned i=0;i<9;i++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+(s32)(i%3)-1,gAtlasGeoResult.y+(s32)(i/3)-1)&0xC00));
        struct AtlasCell a,b;AtlasTerrain_Sample(-1,0,&a);AtlasTerrain_Sample(ATLAS_WORLD_WIDTH-1,0,&b);assert(!memcmp(&a,&b,sizeof(a)));
        printf("PASS generation12 seed=%u islands=%u largest_landmass=%u: fixed land budget, seeded geometry replay/import, island names, safe island TP in %u steps, dateline and drainage\n",seeds[seed],islands,largest,search);
    }
}
