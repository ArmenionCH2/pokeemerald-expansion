#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
static bool8 Under(u16 tile){unsigned id=tile&1023;return id>=ATLAS_UNDERGROWTH_FIRST && id<ATLAS_UNDERGROWTH_FIRST+ATLAS_UNDERGROWTH_COUNT;}
int main(void)
{
    unsigned counts[2]={0};struct AtlasCell c={.elevation=140,.temperature=140,.moisture=180,.biome=ATLAS_FOREST,.valley=255};
    AtlasTerrain_SetSeed(1);
    for(s32 y=-64;y<64;y++)for(s32 x=0;x<128;x++)
    {
        u16 tile=AtlasTerrain_BaseTile(x,y,&c,FALSE);
        if(!Under(tile))continue;
        counts[(tile&1023)-ATLAS_UNDERGROWTH_FIRST]++;
        assert((tile&0xFC00)==0x3000);
        assert(tile==AtlasTerrain_BaseTile(x-ATLAS_WORLD_WIDTH,y,&c,FALSE));
        assert(!Under(AtlasTerrain_BaseTile(x,y,&c,TRUE)));
        struct AtlasCell forbidden=c;
        forbidden.temperature=84;assert(!Under(AtlasTerrain_BaseTile(x,y,&forbidden,FALSE)));
        forbidden=c;forbidden.moisture=131;assert(!Under(AtlasTerrain_BaseTile(x,y,&forbidden,FALSE)));
        forbidden=c;forbidden.elevation=175;assert(!Under(AtlasTerrain_BaseTile(x,y,&forbidden,FALSE)));
        for(unsigned biome=0;biome<=ATLAS_WETLAND;biome++)
        {if(biome==ATLAS_FOREST || biome==ATLAS_JUNGLE)continue;forbidden=c;forbidden.biome=biome;assert(!Under(AtlasTerrain_BaseTile(x,y,&forbidden,FALSE)));}
    }
    assert(counts[0] && counts[1]);
    const u32 seeds[]={0,1,2731893282u};assert(AtlasWorld_SetGeneration(26));
    for(unsigned k=0;k<3;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));AtlasGeo_Begin(ATLAS_GEO_GROVE);
        unsigned steps=0;while(!AtlasGeo_Step())assert(++steps<ATLAS_GEO_SEARCH_STEPS);
        assert(gAtlasGeoStatus==1);s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;unsigned plants=0;
        for(s32 dy=-16;dy<=16;dy++)for(s32 dx=-16;dx<=16;dx++)
        {u16 tile=AtlasTerrain_Tile(x+dx,y+dy);if(Under(tile)){plants++;assert(!(tile&0xC00));}}
        printf("forest seed=%u undergrowth=%u\n",seeds[k],plants);assert(plants);
        Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
        printf("PASS undergrowth seed=%u:1280 strict camera steps, intact tree footprints and exact post-walk tiles\n",seeds[k]);
    }
    printf("PASS undergrowth: warm/moist forest gates, protected cover, unchanged walkability, wrapped detail and %u/%u native variants\n",counts[0],counts[1]);
}
