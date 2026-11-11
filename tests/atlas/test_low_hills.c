#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include "atlas_vegetation.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
static struct AtlasPlanNode old[ATLAS_PLAN_COUNT];
static void Generate(u32 seed,u16 version){assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
int main(void)
{
    setbuf(stdout,NULL);unsigned raisedTrees=0;const u32 seeds[]={0,1,2731893282u};
    assert(AtlasWorld_SetGeneration(27));
    u16 crown[9],surface[9],halo[25];
    for(unsigned i=0;i<9;i++){crown[i]=ATLAS_NATURE_TREE_FIRST+i;surface[i]=(140<<8)|0x10;}
    for(unsigned i=0;i<25;i++)halo[i]=(140<<8)|0x10;
    assert(AtlasVegetation_HillFootprint(crown,surface) && AtlasVegetation_HillHalo(halo));
    for(unsigned i=0;i<25;i++)
    {
        u16 old=halo[i];halo[i]&=~0x10;assert(!AtlasVegetation_HillHalo(halo));
        halo[i]=old|ATLAS_RIVER;assert(!AtlasVegetation_HillHalo(halo));
        halo[i]=old|0x80;assert(!AtlasVegetation_HillHalo(halo));
        halo[i]=(165<<8)|0x10;assert(!AtlasVegetation_HillHalo(halo));halo[i]=old;
    }
    crown[4]=ATLAS_ALT_TREE_FIRST+4;assert(!AtlasVegetation_HillFootprint(crown,surface));crown[4]=ATLAS_NATURE_TREE_FIRST+4;
    surface[4]=(143<<8)|0x10;assert(!AtlasVegetation_HillFootprint(crown,surface));surface[4]=(140<<8)|0x10;
    assert(AtlasWorld_SetGeneration(26));assert(!AtlasVegetation_HillFootprint(crown,surface));assert(AtlasWorld_SetGeneration(27));
    const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    for(unsigned material=0;material<4;material++)for(unsigned mask=0;mask<256;mask++)
    {
        u16 cells[9];cells[4]=(140<<8)|0x10|(material<<2);
        for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(140<<8)|(material<<2)|((mask&(1u<<i))?0x10:0);
        unsigned floor=material==3?ATLAS_NATURE_SNOW_FIRST:material==2?ATLAS_ROCK_FLOOR_FIRST:material==1?ATLAS_NATURE_SAND_FIRST:ATLAS_NATURE_GRASS_FIRST;
        u16 tile=AtlasTiles_Render(0x3000|floor,cells);
        if(mask==255 || !(mask&4))assert(tile==(0x3000|floor));
        else
        {
            unsigned first=material==3?ATLAS_NATURE_MOUNTAIN_FIRST+47:material==2?ATLAS_ROCK_CLIFF_FIRST:material==1?ATLAS_LOW_SAND_CLIFF_FIRST:ATLAS_NATURE_MOUNTAIN_FIRST;
            assert(tile==(0x3400| (first+gAtlasNatureMaskIndex[mask])));
        }
        cells[1]=(121<<8)|ATLAS_RIVER;assert(!(AtlasTiles_Render(0x3000|floor,cells)&0xC00));
    }
    for(unsigned k=0;k<3;k++)
    {
        Generate(seeds[k],26);memcpy(old,gAtlasPlan,sizeof(old));
        AtlasGeo_Begin(ATLAS_GEO_LOW_HILLS);assert(AtlasGeo_Step()==3);
        Generate(seeds[k],27);assert(!memcmp(old,gAtlasPlan,sizeof(old)));
        unsigned steps=0;for(unsigned retry=0;retry<3;retry++){AtlasGeo_Begin(ATLAS_GEO_LOW_HILLS);while(!AtlasGeo_Step())steps++;if(gAtlasGeoStatus!=2)break;}
        printf("low hills seed=%u status=%u steps=%u at=%d,%d\n",seeds[k],gAtlasGeoStatus,steps,gAtlasGeoResult.x,gAtlasGeoResult.y);assert(gAtlasGeoStatus==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;struct AtlasCell c;AtlasTerrain_Sample(x,y,&c);assert(c.landform==ATLAS_KNOLL && c.elevation<=164 && c.valley==255);
        unsigned walls=0,low=255,high=0;
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
        {
            AtlasTerrain_Sample(x+dx,y+dy,&c);if(c.elevation<low)low=c.elevation;if(c.elevation>high)high=c.elevation;
            walls+=AtlasTiles_IsCliff(AtlasTerrain_Tile(x+dx,y+dy)&1023);
            if(dx>=-1 && dx<=1 && dy>=-1 && dy<=1)assert(!(AtlasTerrain_Tile(x+dx,y+dy)&0xC00));
            assert(AtlasTerrain_Tile(x+dx,y+dy)==AtlasTerrain_Tile(x+dx-ATLAS_WORLD_WIDTH,y+dy));
        }
        assert(walls>=3 && high>low);Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
        for(unsigned retry=0;retry<3;retry++){AtlasGeo_Begin(ATLAS_GEO_WOODED_HILLS);while(!AtlasGeo_Step());if(gAtlasGeoStatus!=2)break;}
        printf("wooded hills seed=%u status=%u\n",seeds[k],gAtlasGeoStatus);assert(gAtlasGeoStatus==1);
        x=gAtlasGeoResult.x;y=gAtlasGeoResult.y;unsigned trees=0;walls=0;
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
        {
            u16 t=AtlasTerrain_Tile(x+dx,y+dy);trees+=AtlasVegetation_IsTree(t);walls+=AtlasTiles_IsCliff(t&1023);
            u16 ignored,semantic=AtlasTiles_Describe(x+dx,y+dy,&ignored);
            if(!AtlasVegetation_IsTree(t) || (semantic&0xF3)!=0x10)continue;
            raisedTrees++;unsigned part=AtlasVegetation_Part(t);s32 ax=x+dx-part%3,ay=y+dy-part/3;
            for(s32 hy=-1;hy<=3;hy++)for(s32 hx=-1;hx<=3;hx++)assert((AtlasTiles_Describe(ax+hx,ay+hy,&ignored)&0xF3)==0x10);
            for(unsigned part2=0;part2<9;part2++)assert((AtlasTerrain_Tile(ax+(s32)(part2%3),ay+(s32)(part2/3))&1023)==AtlasVegetation_First(t)+part2);
        }
        assert(trees>=9 && walls>=3);Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
    }
    printf("valid raised tree tiles in inspection cameras: %u\n",raisedTrees);assert(raisedTrees>0);
    /* A uniform inland savanna isolates local relief from lakes, river banks,
     * trees and mountain geometry. Every walkable patch must reach the edge of the sampled field. */
    static u8 before[4096];
    Generate(1,26);
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i]=(struct AtlasPlanNode){.parent=i,.continent=1,.basin=1,.height=142,.rain=130,.humidity=150,.water=121};
    AtlasPlan_ResetSample();AtlasWater_Reset();
    for(unsigned at=0;at<4096;at++){struct AtlasPlanSample p;before[at]=AtlasTerrain_GroundHeight(10000+at%64,at/64,&p);}
    Generate(1,27);
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i]=(struct AtlasPlanNode){.parent=i,.continent=1,.basin=1,.height=142,.rain=130,.humidity=150,.water=121};
    AtlasPlan_ResetSample();AtlasWater_Reset();
    static u16 tiles[4096];static unsigned queue[4096];static bool8 seen[4096];unsigned free=0,cliffs=0,head=0,tail=0;
    for(unsigned at=0;at<4096;at++)
    {struct AtlasPlanSample p;u8 after=AtlasTerrain_GroundHeight(10000+at%64,at/64,&p);
        assert(after==before[at]+AtlasTerrain_KnollRise(10000+at%64,at/64,&p,before[at]) && after>=before[at] && after-before[at]<=10);
        tiles[at]=AtlasTerrain_Tile(10000+at%64,at/64);free+=!(tiles[at]&0xC00);cliffs+=AtlasTiles_IsCliff(tiles[at]&1023);}
    assert(cliffs>=10);
    for(unsigned at=0;at<4096;at++)if((at%64==0 || at%64==63 || at<64 || at>=4032) && !(tiles[at]&0xC00))
    {queue[tail++]=at;seen[at]=TRUE;}
    while(head<tail)
    {
        unsigned at=queue[head++];static const int offsets[]={1,-1,64,-64};
        for(unsigned d=0;d<4;d++)
        {
            int next=(int)at+offsets[d];if(next<0 || next>=4096 || (d<2 && next/64!=(int)at/64) || seen[next] || (tiles[next]&0xC00))continue;
            seen[next]=TRUE;queue[tail++]=next;
        }
    }
    printf("controlled low hills: %u cliff tiles, %u/%u boundary-accessible walkable tiles\n",cliffs,tail,free);assert(tail==free);
    for(unsigned x=0;x<128;x++)
    {
        struct AtlasPlanSample plan;u8 ground=AtlasTerrain_GroundHeight(x,0,&plan);unsigned rise=AtlasTerrain_KnollRise(x,0,&plan,142+(s32)0);
        assert(ground>=135 && ground<=164 && rise<=10);plan.lake=1;assert(!AtlasTerrain_KnollRise(x,0,&plan,142));plan.lake=0;plan.height=121;assert(!AtlasTerrain_KnollRise(x,0,&plan,121));
    }
    unsigned sand=0;
    for(s32 x=0;x<2048;x++)
    {
        u16 floor,semantic=AtlasTiles_Describe(x,1000,&floor),id=floor&1023;
        if(id>=ATLAS_NATURE_SAND_FIRST && id<ATLAS_NATURE_SAND_FIRST+4){assert(((semantic>>2)&3)==1);sand++;}
    }
    assert(sand);printf("coherent savanna sand samples: %u\n",sand);
    puts("PASS unchanged macro notes/legacy gate, visible lowland height/contours, safe inspection, strict streaming, longitude wrap, lake/sea exclusions and connected hill ramps");
}
