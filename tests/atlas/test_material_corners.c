#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>
#include "stream_walk.h"
static bool8 Corner(u16 tile,unsigned first){unsigned id=tile&1023;return id>=first && id<first+ATLAS_GROUND_NOTCH_COUNT;}
int main(void)
{
    setbuf(stdout,NULL);assert(AtlasWorld_SetGeneration(26));
    const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    for(unsigned material=2;material<=3;material++)
    {
        unsigned first=material==2?ATLAS_ROCK_NOTCH_FIRST:ATLAS_SNOW_NOTCH_FIRST;
        unsigned floorId=material==2?ATLAS_ROCK_FLOOR_FIRST:ATLAS_NATURE_SNOW_FIRST;
        unsigned edge=material==2?ATLAS_ROCK_GROUND_EDGE_FIRST:ATLAS_SNOW_GROUND_EDGE_FIRST,roles[31]={0};
        for(unsigned mask=0;mask<256;mask++)
        {
            u16 cells[9];cells[4]=(150<<8)|(material<<2);
            for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(150<<8)|((mask&(1u<<i))?(material<<2):0);
            unsigned notches=0;
            for(unsigned i=0;i<4;i++)notches+=(mask&(1u<<i)) && (mask&(1u<<((i+1)%4))) && !(mask&(1u<<(i+4)));
            for(unsigned variant=0;variant<4;variant++)
            {
                u16 floor=0x3000|(floorId+variant),tile=AtlasTiles_Render(floor,cells);
                assert((tile&0xFC00)==0x3000 && !!Corner(tile,first)==!!notches);
                if(Corner(tile,first)){roles[(tile&1023)-first]++;assert(gAtlasPrimaryAttributes[tile&1023]==0x1000);}
                else {unsigned m=mask&15;assert((tile&1023)==(m==15?(floor&1023):edge+m));}
            }
            for(unsigned form=ATLAS_SEA;form<=ATLAS_LAKE;form++)
            {
                for(unsigned i=0;i<8;i++)cells[neighbors[i]]=(121<<8)|form;
                assert(AtlasTiles_Render(0x3000|floorId,cells)==(0x3000|floorId));
            }
        }
        for(unsigned i=0;i<31;i++)assert(roles[i]);
    }
    const u32 seeds[]={0,1,2731893282u};
    for(unsigned k=0;k<3;k++)for(unsigned type=ATLAS_GEO_ROCK_CORNER;type<=ATLAS_GEO_SNOW_CORNER;type++)
    {
        assert(AtlasPlan_Begin(seeds[k]));while(!AtlasPlan_Step(256));unsigned status=2,steps=0;
        for(unsigned retry=0;retry<3 && status==2;retry++)
        {AtlasGeo_Begin(type);while(!AtlasGeo_Step())steps++;status=gAtlasGeoStatus;}
        printf("material corner seed=%u type=%u status=%u steps=%u at=%d,%d\n",seeds[k],type,status,steps,gAtlasGeoResult.x,gAtlasGeoResult.y);assert(status==1);
        unsigned first=type==ATLAS_GEO_ROCK_CORNER?ATLAS_ROCK_NOTCH_FIRST:ATLAS_SNOW_NOTCH_FIRST;
        s32 x=gAtlasGeoResult.x,y=gAtlasGeoResult.y;unsigned visible=0;
        for(s32 dy=-3;dy<=3;dy++)for(s32 dx=-5;dx<=5;dx++)if(dx<-1 || dx>1 || dy<-1 || dy>1)visible+=Corner(AtlasTerrain_Tile(x+dx,y+dy),first);
        assert(visible);
        for(s32 dy=-1;dy<=1;dy++)for(s32 dx=-1;dx<=1;dx++)assert(!(AtlasTerrain_Tile(x+dx,y+dy)&0xC00));
        Walk(x-100,y,FALSE);Walk(x,y-100,TRUE);
    }
    puts("PASS all 256 rock/snow neighborhoods and 62 roles, unchanged movement/water priority and six safe visible seeded inspections with strict streaming");
}
