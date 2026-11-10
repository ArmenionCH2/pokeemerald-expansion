#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_karst.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include "../../src/atlas_place.c"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode nodes[ATLAS_PLAN_COUNT];
static u8 original[1024];
static void Generate(u32 seed,u16 version)
{assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));while(!AtlasPlan_Step(256));}
static void Masks(void)
{
    static const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    assert(AtlasWorld_SetGeneration(23));
    for(unsigned mask=0;mask<256;mask++)
    {
        u16 cells[9];for(unsigned i=0;i<9;i++)cells[i]=(143<<8)|0x70;
        cells[4]=(144<<8)|0x70;
        for(unsigned i=0;i<8;i++)if(mask&(1u<<i))cells[neighbors[i]]=(144<<8)|0x70;
        u16 floor=0x3000|ATLAS_NATURE_GRASS_FIRST,tile=AtlasTiles_Render(floor,cells);
        if(mask==255)assert(tile==floor);
        else {assert(AtlasTiles_IsCliff(tile&1023));assert((tile&0xFC00)==0x3400);}
        cells[4]|=0x80;assert(AtlasTiles_Render(floor,cells)==floor);
        cells[4]&=~0x80;for(unsigned i=0;i<8;i++)cells[neighbors[i]]=ATLAS_RIVER;
        assert(AtlasTiles_Render(floor,cells)==floor);
    }
}
int main(void)
{
    setbuf(stdout,NULL);Masks();const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789};
    for(unsigned k=0;k<6;k++)
    {
        Generate(seeds[k],22);memcpy(nodes,gAtlasPlan,sizeof(nodes));
        struct AtlasKarst site,a,b;assert(!AtlasKarst_Site(0,&site));
        Generate(seeds[k],23);assert(!memcmp(nodes,gAtlasPlan,sizeof(nodes)));
        unsigned first=ATLAS_PLAN_COUNT,sites=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)if(AtlasKarst_Site(id,&site))
        {
            if(first==ATLAS_PLAN_COUNT)first=id;
            sites++;
            assert(site.radius>=512 && site.radius<=1280 && site.cut>=16 && site.cut<=31);
            assert(AtlasKarst_Sample(site.x,site.y,&a) && a.cut==site.cut);
            assert(AtlasKarst_Sample(site.x+ATLAS_WORLD_WIDTH,site.y,&b) && b.cut==a.cut && b.relief==a.relief);
            assert(!AtlasKarst_Sample(site.x,site.y+site.radius+128,&b));
            AtlasKarst_Sample(site.x+ATLAS_WORLD_WIDTH/2,site.y,&b);
            assert(AtlasKarst_Sample(site.x,site.y,&b) && b.cut==a.cut && b.relief==a.relief);
        }
        printf("karst seed=%u sites=%u\n",seeds[k],sites);assert(sites && sites<64);
        assert(AtlasKarst_Site(first,&site));
        Generate(seeds[k],22);unsigned count=0;
        for(s32 d=-site.radius-128;d<=site.radius+128;d+=4)
        {struct AtlasPlanSample plan;assert(count<1024);original[count++]=AtlasTerrain_GroundHeight(site.x,site.y+d,&plan);}
        Generate(seeds[k],23);unsigned at=0,cut=0;
        for(s32 d=-site.radius-128;d<=site.radius+128;d+=4)
        {
            struct AtlasPlanSample plan;u8 height=AtlasTerrain_GroundHeight(site.x,site.y+d,&plan);
            unsigned expected=original[at++];
            if(AtlasKarst_Sample(site.x,site.y+d,&a))
            {unsigned room=expected>ATLAS_PLAN_SEA+8?expected-ATLAS_PLAN_SEA-8:0;expected-=a.cut>room?room:a.cut;}
            assert(height==expected && height>=ATLAS_PLAN_SEA+8);cut+=height<original[at-1];
            struct AtlasCell cell;AtlasTerrain_Sample(site.x,site.y+d,&cell);
            if(cell.landform==ATLAS_SINKHOLE)
            {assert(cell.elevation==height && !cell.waterform);assert(!(AtlasTerrain_Tile(site.x,site.y+d)&0xC00));}
        }
        assert(cut);
        AtlasGeo_Begin(ATLAS_GEO_SINKHOLE);unsigned steps;
        for(steps=0;steps<ATLAS_GEO_SEARCH_STEPS && !AtlasGeo_Step();steps++);
        printf("karst TP status=%u steps=%u\n",gAtlasGeoStatus,steps);assert(gAtlasGeoStatus==1);
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;unsigned walls=0;
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
        {
            u16 tile=AtlasTerrain_Tile(x+dx,y+dy);walls+=AtlasTiles_IsCliff(tile&1023);
            if(dx>=-1 && dx<=1 && dy>=-1 && dy<=1)assert(!(tile&0xC00));
        }
        assert(walls);struct AtlasCell cell;struct AtlasPlace place;AtlasTerrain_Sample(x,y,&cell);
        assert(AtlasPlace_Identify(x,y,&cell,&place) && place.kind==ATLAS_PLACE_SINKHOLE);
        puts("PASS bounded shared karst depression, sea-floor clearance, unchanged macro notes, periodic/cache-stable profile, walkable axial approach, actual rock walls and named safe inspector");
    }
}
