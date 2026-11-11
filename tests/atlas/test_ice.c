#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
static bool8 Ice(u16 tile){return (tile&1023)==ATLAS_ICE_SEA || (tile&1023)==ATLAS_ICE_LAKE;}
int main(void)
{
    setbuf(stdout,NULL);assert(AtlasWorld_SetGeneration(26));AtlasTerrain_SetSeed(1);
    for(unsigned water=ATLAS_SEA;water<=ATLAS_LAKE;water++)
    {
        struct AtlasCell c={.elevation=121,.temperature=64,.waterform=water};unsigned found=0;
        for(s32 y=-32;y<32;y++)for(s32 x=0;x<128;x++)
        {
            u16 tile=AtlasTerrain_BaseTile(x,y,&c,FALSE);
            assert((tile&0xFC00)==0x3400);
            assert(tile==AtlasTerrain_BaseTile(x-ATLAS_WORLD_WIDTH,y,&c,FALSE));
            assert(tile==AtlasTerrain_BaseTile(x,y,&c,TRUE));
            if(Ice(tile)){found++;assert((tile&1023)==(water==ATLAS_SEA?ATLAS_ICE_SEA:ATLAS_ICE_LAKE));}
            struct AtlasCell warm=c;warm.temperature=65;assert(!Ice(AtlasTerrain_BaseTile(x,y,&warm,FALSE)));
        }
        if(water==ATLAS_RIVER)assert(!found);else assert(found>128 && found<400);
    }
    static const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    for(unsigned water=ATLAS_SEA;water<=ATLAS_LAKE;water++)
    for(unsigned material=0;material<4;material++)for(unsigned mask=0;mask<256;mask++)
    {
        u16 cells[9];cells[4]=(121<<8)|water;
        for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(121<<8)|((mask&(1u<<i))?water:material<<2);
        for(unsigned role=ATLAS_ICE_LAKE;role<=ATLAS_ICE_SEA;role++)
        {
            u16 tile=AtlasTiles_Render(role,cells);
            bool8 keep=mask==255 && ((water==ATLAS_SEA && role==ATLAS_ICE_SEA) || (water==ATLAS_LAKE && role==ATLAS_ICE_LAKE));
            assert(tile==((keep?role:AtlasTiles_Water(mask,material,water))|0x3400));
        }
    }
    const u32 seeds[]={0,1,2731893282u};
    for(unsigned k=0;k<3;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));unsigned status=2,total=0;
        for(unsigned retry=0;retry<3 && status==2;retry++)
        {
            AtlasGeo_Begin(ATLAS_GEO_ICE_SHORE);
            for(unsigned steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++)total++;
            status=gAtlasGeoStatus;
        }
        printf("ice seed=%u status=%u steps=%u at=%d,%d\n",seeds[k],status,total,gAtlasGeoResult.x,gAtlasGeoResult.y);assert(status==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;unsigned visible=0;
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)visible+=(AtlasTerrain_Tile(x+dx,y+dy)&1023)==ATLAS_ICE_SEA;
        assert(visible);
        for(s32 dy=-1;dy<=1;dy++)for(s32 dx=-1;dx<=1;dx++)assert(!(AtlasTerrain_Tile(x+dx,y+dy)&0xC00));
        Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
    }
    puts("PASS sparse cold lake/sea fragments, warm/river exclusions, all bank masks, water collision, longitude wrap and visible safe shore inspection");
}
