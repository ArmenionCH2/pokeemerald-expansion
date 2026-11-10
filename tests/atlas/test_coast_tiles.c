#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(AtlasWorld_SetGeneration(22));static const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    for(unsigned surface=0;surface<3;surface++)for(unsigned mask=0;mask<256;mask++)
    {
        bool8 snow=surface==2;
        u16 cells[9],floor=0x3000|(snow?ATLAS_NATURE_SNOW_FIRST:surface?ATLAS_ROCK_FLOOR_FIRST:ATLAS_NATURE_GRASS_FIRST);
        cells[4]=(145<<8)|0x60|(snow?12:surface?8:0);
        for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(mask&(1u<<i))?((145<<8)|8):((121<<8)|ATLAS_SEA);
        u16 tile=AtlasTiles_Render(floor,cells),id=tile&1023;
        assert((tile&0xF000)==0x3000);
        if(mask==255)assert(tile==floor);
        else
        {
            assert(tile&0x400);
            assert(id==(snow?ATLAS_NATURE_MOUNTAIN_FIRST+47:ATLAS_ROCK_CLIFF_FIRST)+gAtlasNatureMaskIndex[mask]);
        }
        for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(121<<8)|ATLAS_RIVER;
        assert(AtlasTiles_Render(floor,cells)==floor);
        for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(139<<8)|ATLAS_SEA;
        assert(AtlasTiles_Render(floor,cells)==floor);
        cells[4]=(121<<8)|ATLAS_SEA;
        assert((AtlasTiles_Render(floor,cells)&1023)>=ATLAS_NATURE_WATER_FIRST);
    }
    u16 cells[9];for(unsigned i=0;i<9;i++)cells[i]=(121<<8)|ATLAS_SEA;
    cells[1]=(145<<8)|0x60; /* grass top over a rocky seaward bank */
    assert((AtlasTiles_Render(0x3400,cells)&1023)==AtlasTiles_Water(254,2,ATLAS_SEA));
    struct AtlasCell cell={0};cell.landform=ATLAS_ROCK_COAST;cell.biome=ATLAS_FOREST;
    cell.elevation=140;cell.moisture=160;cell.temperature=150;cell.valley=255;
    for(unsigned x=0;x<128;x++)
    {
        u16 id=AtlasTerrain_BaseTile(x,0,&cell,FALSE)&1023;
        assert(id<ATLAS_ROCK_FLOOR_FIRST || id>=ATLAS_ROCK_FLOOR_FIRST+4);
        assert(id<ATLAS_NATURE_TREE_FIRST || id>=ATLAS_NATURE_TREE_FIRST+9);
    }
    cell.moisture=70;cell.biome=ATLAS_DESERT;
    unsigned id=AtlasTerrain_BaseTile(0,0,&cell,FALSE)&1023;assert(id>=ATLAS_ROCK_FLOOR_FIRST && id<ATLAS_ROCK_FLOOR_FIRST+4);
    cell.biome=ATLAS_TUNDRA;id=AtlasTerrain_BaseTile(0,0,&cell,FALSE)&1023;assert(id>=ATLAS_NATURE_SNOW_FIRST && id<ATLAS_NATURE_SNOW_FIRST+4);
    puts("PASS 768 grassy/rocky/snowy sea-cliff masks: blocked seaward faces, inland/river openings, height threshold and preserved water priority");
}
