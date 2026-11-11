#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_vegetation.h"
#include <assert.h>
#include <stdio.h>
static u16 map[1190];
int main(void)
{
    u16 floors[9],semantics[9];unsigned i,trees=0,proposals=0;
    for (i=0;i<9;i++) {floors[i]=0x3000|(ATLAS_NATURE_TREE_FIRST+i);semantics[i]=140<<8;}
    assert(AtlasVegetation_Valid(floors,semantics));
    for (i=0;i<9;i++)
    {
        u16 old=semantics[i];
        semantics[i]|=ATLAS_RIVER;assert(!AtlasVegetation_Valid(floors,semantics));
        semantics[i]=old|0x10;assert(!AtlasVegetation_Valid(floors,semantics));
        semantics[i]=old|0x80;assert(!AtlasVegetation_Valid(floors,semantics));
        semantics[i]=old;floors[i]=0x3001;assert(!AtlasVegetation_Valid(floors,semantics));
        floors[i]=0x3000|(ATLAS_NATURE_TREE_FIRST+i);
    }
    for(i=0;i<9;i++)floors[i]=0x3000|(ATLAS_ALT_TREE_FIRST+i);
    assert(AtlasVegetation_Valid(floors,semantics));
    floors[4]=0x3000|(ATLAS_NATURE_TREE_FIRST+4);assert(!AtlasVegetation_Valid(floors,semantics));
    struct AtlasCell forest={0};forest.biome=ATLAS_FOREST;forest.elevation=140;forest.valley=255;
    AtlasWorld_SetGeneration(6);
    for (i=0;i<1000;i++) assert(!AtlasVegetation_IsTree(AtlasTerrain_BaseTile(i%100,i/100,&forest,FALSE)));
    AtlasWorld_SetGeneration(7);assert(AtlasPlan_Begin(0));while(!AtlasPlan_Step(256));
    AtlasGeo_Begin(ATLAS_GEO_FOREST);
    for (i=0;i<8000 && !AtlasGeo_Step();i++);
    assert(gAtlasGeoStatus==1);
    s32 cx=AtlasTerrain_FloorDiv(gAtlasGeoResult.x,20),cy=AtlasTerrain_FloorDiv(gAtlasGeoResult.y,20);
    assert(AtlasStream_Load(map,cx,cy));
    for (s32 y=cy*20-7;y<cy*20+27;y++) for (s32 x=cx*20-7;x<cx*20+28;x++)
    {
        u16 floor,semantic=AtlasTiles_Describe(x,y,&floor),tile=AtlasTerrain_Tile(x,y);
        assert(tile==map[(y-(cy*20-7))*35+x-(cx*20-7)]);
        if (AtlasVegetation_IsTree(floor)) proposals++;
        if (!AtlasVegetation_IsTree(tile)) continue;
        assert(tile&0x400);assert(!(semantic&0xF3));trees++;
        unsigned first=AtlasVegetation_First(tile),part=AtlasVegetation_Part(tile);
        s32 ax=x-(s32)(part%3),ay=y-(s32)(part/3);
        for (i=0;i<9;i++)
        {
            u16 other=AtlasTerrain_Tile(ax+(s32)(i%3),ay+(s32)(i/3));
            assert((other&1023)==first+i && (other&0x400));
        }
        s32 wx=x%ATLAS_WORLD_WIDTH;if(wx<0)wx+=ATLAS_WORLD_WIDTH;
        assert(wx%5!=4 && y-AtlasTerrain_FloorDiv(y,5)*5!=4);
        assert(tile==AtlasTerrain_Tile(x-ATLAS_WORLD_WIDTH,y));
    }
    assert(trees>=9 && proposals>=trees);
    // Move the real ring through forest in both directions, comparing the
    // visible camera stencil with direct rendering after every rebase.
    s32 startX=gAtlasGeoResult.x,startY=gAtlasGeoResult.y,oldCX=cx;
    for (unsigned step=0;step<240;step++)
    {
        s32 x=startX+(step<120?(s32)step:239-(s32)step);
        cx=AtlasTerrain_FloorDiv(x,20);
        if (cx!=oldCX) {assert(AtlasStream_Load(map,cx,cy));oldCX=cx;}
        for (unsigned frame=0;frame<8;frame++) AtlasStream_Tick(cx,cy,x-cx*20,startY-cy*20);
        for (s32 dy=-5;dy<=5;dy++) for (s32 dx=-7;dx<=7;dx++)
        {
            s32 wx=x+dx,wy=startY+dy;
            assert(map[(wy-cy*20+7)*35+wx-cx*20+7]==AtlasTerrain_Tile(wx,wy));
        }
    }
    assert(gAtlasCacheMisses==1 && !gAtlasStreamWaits && gAtlasCacheHits>=10);
    AtlasGeo_Begin(ATLAS_GEO_GROVE);
    for (i=0;i<8000 && !AtlasGeo_Step();i++);
    assert(gAtlasGeoStatus==1);
    unsigned visible=0;
    for (s32 y=-4;y<=4;y++) for (s32 x=-6;x<=6;x++)
        visible+=AtlasVegetation_IsTree(AtlasTerrain_Tile(gAtlasGeoResult.x+x,gAtlasGeoResult.y+y));
    assert(visible>=9);
    printf("PASS complete forest groves: %u tree tiles, %u proposals, direct/ring equivalence, terrain protection, world wrap and legacy no-tree behavior\n",trees,proposals);
    return 0;
}
