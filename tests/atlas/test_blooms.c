#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
static bool8 Bloom(u16 tile){unsigned id=tile&1023;return id>=ATLAS_WARM_FLOWER_FIRST && id<ATLAS_WARM_FLOWER_FIRST+4;}
int main(void)
{
    setbuf(stdout,NULL);unsigned purple[2]={0},gold[2]={0};AtlasTerrain_SetSeed(1);
    struct AtlasCell c={.elevation=140,.temperature=180,.moisture=180,.biome=ATLAS_GRASSLAND,.valley=255};
    for(s32 y=-64;y<64;y++)for(s32 x=0;x<128;x++)
    {
        u16 tile=AtlasTerrain_BaseTile(x,y,&c,FALSE);if(!Bloom(tile))continue;
        assert((tile&1023)<ATLAS_WARM_FLOWER_FIRST+2);purple[(tile&1023)-ATLAS_WARM_FLOWER_FIRST]++;
        assert((tile&0xFC00)==0x3000 && tile==AtlasTerrain_BaseTile(x-ATLAS_WORLD_WIDTH,y,&c,FALSE));
        assert(!Bloom(AtlasTerrain_BaseTile(x,y,&c,TRUE)));
        struct AtlasCell dry=c;dry.moisture=120;tile=AtlasTerrain_BaseTile(x,y,&dry,FALSE);
        assert((tile&1023)>=ATLAS_WARM_FLOWER_FIRST+2 && (tile&1023)<ATLAS_WARM_FLOWER_FIRST+4);gold[(tile&1023)-ATLAS_WARM_FLOWER_FIRST-2]++;
        struct AtlasCell mild=c;mild.temperature=130;tile=AtlasTerrain_BaseTile(x,y,&mild,FALSE);assert((tile&1023)>=ATLAS_NATURE_FLOWER_FIRST && (tile&1023)<ATLAS_NATURE_FLOWER_FIRST+2);
        mild.temperature=84;assert(!Bloom(AtlasTerrain_BaseTile(x,y,&mild,FALSE)));
        mild=c;mild.elevation=175;assert(!Bloom(AtlasTerrain_BaseTile(x,y,&mild,FALSE)));
        mild=c;mild.moisture=99;assert(!Bloom(AtlasTerrain_BaseTile(x,y,&mild,FALSE)));
    }
    assert(purple[0] && purple[1] && gold[0] && gold[1]);
    const u32 seeds[]={0,1,2731893282u};assert(AtlasWorld_SetGeneration(26));
    for(unsigned k=0;k<3;k++)
    for(unsigned type=ATLAS_GEO_BLOOMS;type<=ATLAS_GEO_GOLD_BLOOMS;type++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));unsigned status=2,steps=0;
        for(unsigned batch=0;batch<3 && status==2;batch++)
        {AtlasGeo_Begin(type);for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);status=gAtlasGeoStatus;}
        printf("bloom seed=%u type=%u status=%u steps=%u\n",seeds[k],type,status,steps);assert(status==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;
        u16 tile=AtlasTerrain_Tile(x+2,y);assert(Bloom(tile));
        if(type==ATLAS_GEO_PURPLE_BLOOMS)assert((tile&1023)<ATLAS_WARM_FLOWER_FIRST+2);
        if(type==ATLAS_GEO_GOLD_BLOOMS)assert((tile&1023)>=ATLAS_WARM_FLOWER_FIRST+2);
        Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
        for(s32 dy=-1;dy<=1;dy++)for(s32 dx=-1;dx<=1;dx++)assert(!(AtlasTerrain_Tile(x+dx,y+dy)&0xC00));
    }
    puts("PASS warm bloom climate gates, both mirrored colors, mild native flowers, world wrap, protected cover and safe off-flower inspection");
}
