#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_vegetation.h"
#include <assert.h>
#include <stdio.h>

static struct AtlasPlanNode previous[ATLAS_PLAN_COUNT], current[ATLAS_PLAN_COUNT];
static u8 heights[ATLAS_PLAN_COUNT], climate[ATLAS_PLAN_COUNT][2], seen[ATLAS_PLAN_COUNT];
static u16 queue[ATLAS_PLAN_COUNT];
static void Generate(u32 seed,u16 version,u16 batch)
{
    assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(seed));
    while(!AtlasPlan_Step(batch));
}
static void Dump(const char *path)
{
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(gAtlasPlan,sizeof(gAtlasPlan),1,f)==1);fclose(f);
}
static unsigned MountainComponent(unsigned start)
{
    unsigned head=0,tail=1;queue[0]=start;seen[start]=1;
    while(head<tail)
    {
        unsigned id=queue[head++],x=id%ATLAS_PLAN_WIDTH,y=id/ATLAS_PLAN_WIDTH;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
        {
            if((!dx&&!dy) || (int)y+dy<0 || (int)y+dy>=ATLAS_PLAN_HEIGHT)continue;
            unsigned at=(y+dy)*ATLAS_PLAN_WIDTH+(x+dx+ATLAS_PLAN_WIDTH)%ATLAS_PLAN_WIDTH;
            if(seen[at] || gAtlasPlan[at].height<195)continue;
            seen[at]=1;queue[tail++]=at;
        }
    }
    return tail;
}
int main(void)
{
    setbuf(stdout,NULL);
    const u32 seeds[]={0,1,0xFFFFFFFFu,0xDEADBEEFu,4220706268u,123456789};
    const u32 oldChecksums[]={0x6e3de169,0x5ef8a4c6,0x427927a2,0x46bd01ca,0x4cb73304,0x5e3d0857};
    u32 firstChecksum=0;
    for(unsigned seed=0;seed<sizeof(seeds)/sizeof(seeds[0]);seed++)
    {
        Generate(seeds[seed],7,256);assert(gAtlasPlanStats.checksum==oldChecksums[seed]);
        memcpy(previous,gAtlasPlan,sizeof(previous));
        if(!seed)Dump("build/relief8-generation7.bin");
        Generate(seeds[seed],8,256);memcpy(current,gAtlasPlan,sizeof(current));
        if(!seed)firstChecksum=gAtlasPlanStats.checksum;
        if(!seed)Dump("build/relief8-generation8.bin");
        unsigned low=0,high=0,isolated=0,largest=0;
        memset(seen,0,sizeof(seen));
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            struct AtlasPlanNode *n=&gAtlasPlan[id];
            assert((n->height>=ATLAS_PLAN_SEA)==(previous[id].height>=ATLAS_PLAN_SEA));
            assert(n->continent==previous[id].continent); // same land/ocean components
            low+=n->height>=ATLAS_PLAN_SEA && n->height<160;
            high+=n->height>=195;
            if(n->height>=195 && !seen[id])
            {
                unsigned size=MountainComponent(id);
                if(size==1)isolated++;
                if(size>largest)largest=size;
            }
            unsigned steps=0,at=id;
            while(gAtlasPlan[at].parent!=at)
            {at=gAtlasPlan[at].parent;assert(at<ATLAS_PLAN_COUNT && ++steps<ATLAS_PLAN_COUNT);}
            assert(gAtlasPlan[at].height<ATLAS_PLAN_SEA);
        }
        assert(gAtlasPlanStats.land==1345 && low>gAtlasPlanStats.land/3);
        assert(high>20 && high<gAtlasPlanStats.land/3 && largest>=8);
        assert(isolated*10<high); // ranges dominate isolated peaks at macro resolution
        Generate(seeds[seed],8,31);assert(!memcmp(current,gAtlasPlan,sizeof(current)));
        s32 x,y;assert(AtlasTerrain_FindSpawn(&x,&y));
        for(unsigned p=0;p<9;p++)assert(!(AtlasTerrain_Tile(x+(s32)(p%3)-1,y+(s32)(p/3)-1)&0xC00));
        struct AtlasCell a,b;AtlasTerrain_Sample(0,0,&a);
        AtlasTerrain_Sample(ATLAS_WORLD_WIDTH,0,&b);assert(!memcmp(&a,&b,sizeof(a)));
        printf("PASS generation8 seed=%u lowlands=%u mountains=%u largest_belt=%u isolated=%u unchanged land mask, components, drainage, batching and safe spawn\n",seeds[seed],low,high,largest,isolated);
    }
    /* A cancelled search must not trap its next attempt in a coastal forest:
     * these may have forest habitat, but coast ground never proposes trees. */
    Generate(0xfbe9f0e9u,8,256);AtlasGeo_Begin(ATLAS_GEO_GROVE);
    for(unsigned step=0;step<31;step++)AtlasGeo_Step();
    AtlasGeo_Begin(ATLAS_GEO_GROVE);
    unsigned search;
    for(search=0;search<2000 && !AtlasGeo_Step();search++);
    assert(search<2000 && gAtlasGeoStatus==1);
    unsigned trees=0;
    for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)trees+=AtlasVegetation_IsTree(AtlasTerrain_Tile(gAtlasGeoResult.x+dx,gAtlasGeoResult.y+dy));
    assert(trees>=9);
    printf("PASS generation8 cancelled grove inspection resumes to %u visible tree tiles in %u steps\n",trees,search);
    /* Changing underwater floor height must not create atmospheric uplift at
     * an otherwise identical coast. Air reaches land from sea level. */
    for(unsigned pass=0;pass<2;pass++)
    {
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)heights[id]=id%ATLAS_PLAN_WIDTH<16?(pass?120:0):140;
        assert(AtlasPlan_Begin(123));assert(AtlasPlan_ImportHeights(heights));
        while(AtlasPlan_Stage()<3)AtlasPlan_Step(1);
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(!pass){climate[id][0]=gAtlasPlan[id].humidity;climate[id][1]=gAtlasPlan[id].rain;}
            else {assert(climate[id][0]==gAtlasPlan[id].humidity);assert(climate[id][1]==gAtlasPlan[id].rain);}
        }
    }
    Generate(0,8,1);assert(gAtlasPlanStats.checksum==firstChecksum);
    puts("PASS generation8 sea-level climate exposure independent of ocean-floor depth");
    return 0;
}
