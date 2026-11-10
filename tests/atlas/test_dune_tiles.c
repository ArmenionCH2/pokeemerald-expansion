#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(AtlasWorld_SetGeneration(19));
    static const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    for(unsigned mask=0;mask<256;mask++)
    {
        u16 cells[9],floor=0x3000|ATLAS_NATURE_SAND_FIRST;
        for(unsigned i=0;i<9;i++)cells[i]=(139<<8)|0x44;
        cells[4]=(140<<8)|0x54;
        for(unsigned i=0;i<8;i++)if(mask&(1u<<i))cells[neighbors[i]]=(140<<8)|0x54;
        u16 tile=AtlasTiles_Render(floor,cells),id=tile&1023;
        assert(!(tile&0xC00) && (tile&0xF000)==0x3000);
        assert(id==(mask==255?ATLAS_NATURE_SAND_FIRST:ATLAS_DUNE_EDGE_FIRST+gAtlasNatureMaskIndex[mask]));
        if(mask!=255)assert(gAtlasPrimaryAttributes[id]==0x1021);
        cells[1]=ATLAS_RIVER;assert((AtlasTiles_Render(floor,cells)&1023)<ATLAS_DUNE_EDGE_FIRST || (AtlasTiles_Render(floor,cells)&1023)>=ATLAS_DUNE_EDGE_FIRST+47);
        cells[4]=ATLAS_RIVER;assert((AtlasTiles_Render(floor,cells)&1023)>=ATLAS_NATURE_WATER_FIRST);
    }
    struct AtlasCell cell={0};cell.biome=ATLAS_DESERT;cell.elevation=145;cell.valley=255;cell.landform=ATLAS_DUNE;
    assert(AtlasMountain_Relief(1,1,&cell)==0x40);cell.landform=ATLAS_DUNE_CREST;assert(AtlasMountain_Relief(1,1,&cell)==0x50);
    cell.waterform=ATLAS_RIVER;assert(!AtlasMountain_Relief(1,1,&cell));
    puts("PASS all 256 native sand-ridge masks walk without cliff collision, retain sand behavior/elevation, distinguish crest/trough and protect water priority");
}
