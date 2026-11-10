#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    setbuf(stdout,NULL);assert(AtlasWorld_SetGeneration(22));
    const unsigned sides[]={1,5,7,3};
    for(unsigned mask=0;mask<16;mask++)
    {
        u16 cells[9]={0};for(unsigned i=0;i<9;i++)cells[i]=(150<<8)|12;
        for(unsigned i=0;i<4;i++)if(!(mask&(1u<<i)))cells[sides[i]]=(150<<8)|4;
        u16 floor=0x3000|ATLAS_NATURE_SNOW_FIRST,tile=AtlasTiles_Render(floor,cells);
        assert((tile&0xFC00)==(floor&0xFC00));
        assert((tile&1023)==(mask==15?ATLAS_NATURE_SNOW_FIRST:ATLAS_SNOW_SAND_EDGE_FIRST+mask));
        if(mask!=15)assert(gAtlasPrimaryAttributes[tile&1023]==0x1000);
        for(unsigned i=0;i<4;i++)cells[sides[i]]=(i&1)?ATLAS_SEA:(150<<8)|12;
        assert(AtlasTiles_Render(floor,cells)==floor);
        /* At a three-material junction the established grass outline wins;
         * no sand-colored pixels should replace actual green neighbors. */
        cells[1]=(150<<8);cells[5]=(150<<8)|4;
        tile=AtlasTiles_Render(floor,cells)&1023;
        assert(tile>=ATLAS_SNOW_GROUND_EDGE_FIRST && tile<ATLAS_SNOW_GROUND_EDGE_FIRST+15);
    }
    /* A controlled cold coastal field exercises final terrain, rather than
     * asserting that every random world must contain a rare beach/snow join. */
    assert(AtlasPlan_Begin(0));while(!AtlasPlan_Step(256));
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i]=(struct AtlasPlanNode){.parent=i,.continent=1,.basin=1,.height=126,.rain=70,.humidity=70,.water=121};
    gAtlasPlan[47*96+3].height=120;AtlasPlan_ResetSample();AtlasWater_Reset();
    s32 boundary=(194*ATLAS_WORLD_HALF_HEIGHT+199)/200;unsigned found=0;
    for(s32 x=2*(ATLAS_WORLD_WIDTH/96)+1000;x<3*(ATLAS_WORLD_WIDTH/96);x+=4096)
    {
        struct AtlasCell a,b;AtlasTerrain_Sample(x,boundary-1,&a);AtlasTerrain_Sample(x,boundary,&b);
        if(a.waterform || b.waterform || a.landform!=ATLAS_COAST || b.landform!=ATLAS_COAST)continue;
        if(a.biome==ATLAS_TUNDRA || b.biome!=ATLAS_TUNDRA)continue;
        u16 tile=AtlasTerrain_Tile(x,boundary),id=tile&1023;
        assert(id>=ATLAS_SNOW_SAND_EDGE_FIRST && id<ATLAS_SNOW_SAND_EDGE_FIRST+15);
        assert(!(tile&0xC00));found++;
        s32 cx=AtlasTerrain_FloorDiv(x,20),cy=AtlasTerrain_FloorDiv(boundary,20);u16 map[1190];
        AtlasStream_Reset();assert(AtlasStream_Load(map,cx,cy));
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
            assert(map[(boundary-cy*20+7+dy)*35+x-cx*20+7+dx]==AtlasTerrain_Tile(x+dx,boundary+dy));
        if(found==4)break;
    }
    assert(found);
    puts("PASS snow/sand masks and controlled actual coastal terrain: temperature boundary, collision/elevation, mixed-material priority and direct/ring agreement; random-world occurrence is not assumed");
}
