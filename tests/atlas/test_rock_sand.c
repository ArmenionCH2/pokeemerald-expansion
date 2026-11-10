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
        u16 cells[9]={0};for(unsigned i=0;i<9;i++)cells[i]=(150<<8)|8;
        for(unsigned i=0;i<4;i++)if(!(mask&(1u<<i)))cells[sides[i]]=(150<<8)|4;
        u16 floor=0x3000|ATLAS_ROCK_FLOOR_FIRST,tile=AtlasTiles_Render(floor,cells);
        assert((tile&0xFC00)==(floor&0xFC00));
        assert((tile&1023)==(mask==15?ATLAS_ROCK_FLOOR_FIRST:ATLAS_ROCK_SAND_EDGE_FIRST+mask));
        if(mask!=15)assert(gAtlasPrimaryAttributes[tile&1023]==0x1000);
        for(unsigned i=0;i<4;i++)cells[sides[i]]=(i&1)?ATLAS_SEA:(150<<8)|8;
        assert(AtlasTiles_Render(floor,cells)==floor);
        /* At a three-material junction the established grass outline wins;
         * no sand-colored pixels should replace actual green neighbors. */
        cells[1]=(150<<8);cells[5]=(150<<8)|4;
        tile=AtlasTiles_Render(floor,cells)&1023;
        assert(tile>=ATLAS_ROCK_GROUND_EDGE_FIRST && tile<ATLAS_ROCK_GROUND_EDGE_FIRST+15);
    }
    const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<6;k++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));
        unsigned status=2,steps=0;
        for(unsigned batch=0;batch<3 && status==2;batch++)
        {
            AtlasGeo_Begin(ATLAS_GEO_ROCK_SAND);
            for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
            status=gAtlasGeoStatus;
        }
        printf("rock/sand seed=%u status=%u steps=%u\n",seeds[k],status,steps);assert(status==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;
        u16 id=AtlasTerrain_Tile(x,y)&1023;
        assert(id>=ATLAS_ROCK_SAND_EDGE_FIRST && id<ATLAS_ROCK_SAND_EDGE_FIRST+15);
        for(s32 dy=-1;dy<=1;dy++)for(s32 dx=-1;dx<=1;dx++)assert(!(AtlasTerrain_Tile(x+dx,y+dy)&0xC00));
        /* Real ring rendering must agree with direct queries at this border. */
        u16 map[1190];AtlasStream_Reset();assert(AtlasStream_Load(map,AtlasTerrain_FloorDiv(x,20),AtlasTerrain_FloorDiv(y,20)));
        s32 cx=AtlasTerrain_FloorDiv(x,20),cy=AtlasTerrain_FloorDiv(y,20);
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
            assert(map[(y-cy*20+7+dy)*35+x-cx*20+7+dx]==AtlasTerrain_Tile(x+dx,y+dy));
    }
    puts("PASS rock/sand rounded borders: all cardinal masks, mixed-material priority, unchanged movement, six-seed safe inspection and direct/ring agreement");
}
