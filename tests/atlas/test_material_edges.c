#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
static const unsigned sides[4]={1,5,7,3};
int main(void)
{
    setbuf(stdout,NULL);assert(AtlasWorld_SetGeneration(19));
    for(unsigned material=2;material<=3;material++)for(unsigned mask=0;mask<16;mask++)
    {
        u16 cells[9]={0};cells[4]=(150<<8)|(material<<2);
        unsigned floorId=material==2?ATLAS_ROCK_FLOOR_FIRST:ATLAS_NATURE_SNOW_FIRST;
        unsigned edgeId=material==2?ATLAS_ROCK_GROUND_EDGE_FIRST:ATLAS_SNOW_GROUND_EDGE_FIRST;
        for(unsigned i=0;i<4;i++)if(mask&(1u<<i))cells[sides[i]]=cells[4];
        u16 floor=0x3000|floorId,tile=AtlasTiles_Render(floor,cells);
        assert((tile&0xFC00)==(floor&0xFC00));
        assert((tile&1023)==(mask==15?floorId:edgeId+mask));
        assert(AtlasTiles_GroundEdge(floor,cells[4]));
        if(mask!=15)assert(gAtlasPrimaryAttributes[tile&1023]==0x1000);
        /* Colored ground and water continue the material; neither acquires
         * a false green fringe. Diagonals do not alter cardinal geometry. */
        for(unsigned i=0;i<4;i++)cells[sides[i]]=(i&1)?ATLAS_RIVER:12;
        assert(AtlasTiles_Render(floor,cells)==floor);
        cells[4]|=ATLAS_RIVER;assert(AtlasTiles_Render(floor,cells)&0xC00);
    }
    static const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned i=0;i<6;i++)
    {
        assert(AtlasPlan_Begin(seeds[i]));while(!AtlasPlan_Step(256));
        for(unsigned type=ATLAS_GEO_ROCK_EDGE;type<=ATLAS_GEO_SNOW_EDGE;type++)
        {
            AtlasGeo_Begin(type);unsigned steps;
            for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
            printf("material boundary seed=%u type=%u status=%u steps=%u\n",seeds[i],type,gAtlasGeoStatus,steps);
            assert(gAtlasGeoStatus==1);
            unsigned first=type==ATLAS_GEO_ROCK_EDGE?ATLAS_ROCK_GROUND_EDGE_FIRST:ATLAS_SNOW_GROUND_EDGE_FIRST;
            unsigned tile=AtlasTerrain_Tile(gAtlasGeoResult.x,gAtlasGeoResult.y)&1023;
            assert(tile>=first && tile<first+15);
            for(s32 dy=-1;dy<=1;dy++)for(s32 dx=-1;dx<=1;dx++)
                assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+dx,gAtlasGeoResult.y+dy)&0xC00));
        }
    }
    puts("PASS native rock/snow cardinal borders: material interiors, no false green water fringe, unchanged collision/elevation and six-seed actual safe boundary inspection");
}
