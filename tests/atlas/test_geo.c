#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_region.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    setbuf(stdout,NULL);
    u32 seeds[2]={0,4220706268u};
    unsigned seed,type,n,i;
    for(seed=0;seed<2;seed++)
    {
        assert(AtlasPlan_Begin(seeds[seed]));while(!AtlasPlan_Step(256));
        for(type=0;type<ATLAS_GEO_COUNT;type++)
        {
            if(type==ATLAS_GEO_VOLCANO || type==ATLAS_GEO_RETURN || type==ATLAS_GEO_GROVE || type==ATLAS_GEO_ISLAND || type==ATLAS_GEO_ESTUARY)continue;
            AtlasGeo_Begin(type);
            for(n=0;n<20000 && !AtlasGeo_Step();n++);
            printf("TP seed=%u type=%u steps=%u status=%u x=%d y=%d\n",seeds[seed],type,n,gAtlasGeoStatus,gAtlasGeoResult.x,gAtlasGeoResult.y);
            assert(n<8000 && gAtlasGeoStatus==1);
            for(i=0;i<9;i++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+(s32)(i%3)-1,gAtlasGeoResult.y+(s32)(i/3)-1)&0xC00));
            if(type==ATLAS_GEO_LAND)assert(gAtlasGeoResult.landmass);
            if(type==ATLAS_GEO_SAND_EDGE)
            {
                u16 id=AtlasTerrain_Tile(gAtlasGeoResult.x,gAtlasGeoResult.y)&1023;
                assert(id==ATLAS_NATURE_SAND_EDGE_NORTH || (id>=ATLAS_NATURE_SAND_EDGE_FIRST && id<ATLAS_NATURE_SAND_EDGE_FIRST+ATLAS_NATURE_SAND_EDGE_COUNT));
            }
            if(type>=ATLAS_GEO_FOREST && type<=ATLAS_GEO_WETLAND)
            {
                struct AtlasCell c;AtlasTerrain_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&c);
                u8 target=type==ATLAS_GEO_FOREST?ATLAS_FOREST:type==ATLAS_GEO_DESERT?ATLAS_DESERT:type==ATLAS_GEO_TUNDRA?ATLAS_TUNDRA:ATLAS_WETLAND;
                assert(c.waterform==ATLAS_DRY && c.biome==target);
            }
            if(type==ATLAS_GEO_PLATEAU) assert(AtlasRegion_At(gAtlasGeoResult.x,gAtlasGeoResult.y)==ATLAS_REGION_PLATEAU);
            if(type==ATLAS_GEO_TRIBUTARY)assert(AtlasWater_IsTributary(gAtlasGeoResult.node));
            if(type==ATLAS_GEO_MOUTH)
            {
                unsigned sea=0,river=0;struct AtlasCell c;
                for(s32 yy=-4;yy<=4;yy++)for(s32 xx=-6;xx<=6;xx++)
                {AtlasTerrain_Sample(gAtlasGeoResult.x+xx,gAtlasGeoResult.y+yy,&c);sea+=c.waterform==ATLAS_SEA;river+=c.waterform==ATLAS_RIVER;}
                assert(sea && river);
                assert(gAtlasPlan[gAtlasPlan[gAtlasGeoResult.node].parent].height<ATLAS_PLAN_SEA);
            }
            if(type==ATLAS_GEO_MOUNTAIN || type==ATLAS_GEO_RIDGE || type==ATLAS_GEO_VALLEY || type==ATLAS_GEO_FOOTHILLS)
            {
                unsigned walls=0;struct AtlasCell c;
                for(s32 yy=-4;yy<=4;yy++)for(s32 xx=-6;xx<=6;xx++)
                {u16 id=AtlasTerrain_Tile(gAtlasGeoResult.x+xx,gAtlasGeoResult.y+yy)&1023;if(id>=924 && id<1018)walls++;}
                assert(walls);AtlasTerrain_Sample(gAtlasGeoResult.x,gAtlasGeoResult.y,&c);
                if(type==ATLAS_GEO_FOOTHILLS){assert(c.elevation>=168 && c.elevation<=189);assert((AtlasMountain_Relief(gAtlasGeoResult.x,gAtlasGeoResult.y,&c)&0x70)==0x10);}
                else assert(c.elevation>175);
                if(type==ATLAS_GEO_RIDGE)assert((AtlasMountain_Relief(gAtlasGeoResult.x,gAtlasGeoResult.y,&c)&0x70)==0x30);
                if(type==ATLAS_GEO_VALLEY)assert(c.valley<=1);
            }
            if(type==ATLAS_GEO_LAKE || type==ATLAS_GEO_RIVER || type==ATLAS_GEO_COAST || type==ATLAS_GEO_VALLEY || type==ATLAS_GEO_TRIBUTARY)
            {
                s32 dx,dy;bool8 found=FALSE;struct AtlasCell c;
                for(dy=-16;dy<=16;dy++)for(dx=-16;dx<=16;dx++)
                {AtlasTerrain_Sample(gAtlasGeoResult.x+dx,gAtlasGeoResult.y+dy,&c);if(c.waterform==(type==2?ATLAS_LAKE:(type==3 || type==ATLAS_GEO_VALLEY || type==ATLAS_GEO_TRIBUTARY)?ATLAS_RIVER:ATLAS_SEA))found=TRUE;}
                assert(found);
            }
        }
        AtlasGeo_Begin(ATLAS_GEO_VOLCANO);assert(AtlasGeo_Step()==3);
        AtlasGeo_Begin(ATLAS_GEO_GROVE);assert(AtlasGeo_Step()==3);
        for(i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i].parent=0xFFFF;
        for(type=ATLAS_GEO_RIVER;type<=ATLAS_GEO_VALLEY;type+=ATLAS_GEO_VALLEY-ATLAS_GEO_RIVER)
        {
            AtlasGeo_Begin(type);
            for(n=0;n<1000 && !AtlasGeo_Step();n++);
            assert(n<1000 && gAtlasGeoStatus==2);
        }
    }
    puts("PASS: real typed destinations, safe 3x3 landings, nearby matching water and unsupported volcano");
    return 0;
}
