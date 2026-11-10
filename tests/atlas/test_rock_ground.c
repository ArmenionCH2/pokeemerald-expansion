#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_volcano.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode old[ATLAS_PLAN_COUNT];
static u16 reference[256];
static void Generate(u32 seed,u16 version)
{
    assert(AtlasWorld_SetGeneration(version) && AtlasPlan_Begin(seed));
    while(!AtlasPlan_Step(256));
}
static u16 Cliff(unsigned mask)
{
    static const unsigned neighbors[]={1,5,7,3,2,8,6,0};
    u16 cells[9]={0};cells[4]=0xD238;
    for(unsigned i=0;i<8;i++)if(mask&(1u<<i))cells[neighbors[i]]=0xD238;
    return AtlasTiles_Render(0x3000|ATLAS_ROCK_FLOOR_FIRST,cells);
}
int main(void)
{
    setbuf(stdout,NULL);
    for(unsigned seed=0;seed<2;seed++)
    {
        Generate(seed,13);memcpy(old,gAtlasPlan,sizeof(old));
        for(unsigned mask=0;mask<256;mask++)reference[mask]=Cliff(mask);
        struct AtlasVolcano site;unsigned id;
        for(id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            if(!AtlasVolcano_Site(id,&site))continue;
            struct AtlasCell candidate;AtlasTerrain_Sample(site.x,site.y,&candidate);
            if(candidate.biome!=ATLAS_TUNDRA)break;
        }
        assert(id<ATLAS_PLAN_COUNT);
        struct AtlasCell before,after;
        AtlasTerrain_Sample(site.x,site.y,&before);
        Generate(seed,14);assert(!memcmp(old,gAtlasPlan,sizeof(old)));
        AtlasTerrain_Sample(site.x,site.y,&after);assert(!memcmp(&before,&after,sizeof(before)));
        assert(AtlasVolcano_Bare(site.x,site.y));
        u16 floor=AtlasTerrain_BaseTile(site.x,site.y,&after,FALSE);
        assert((floor&1023)>=ATLAS_ROCK_FLOOR_FIRST && (floor&1023)<ATLAS_ROCK_FLOOR_FIRST+4);
        assert(!(floor&0xC00));
        for(unsigned mask=0;mask<256;mask++)
        {
            u16 tile=Cliff(mask),tileId=tile&1023;
            assert((tile&0xFC00)==(reference[mask]&0xFC00));
            if(mask==255)assert(tile==floor || tileId==ATLAS_ROCK_FLOOR_FIRST);
            else assert(tileId==ATLAS_ROCK_CLIFF_FIRST+gAtlasNatureMaskIndex[mask]);
        }
        /* Snow keeps its own ground even in the exposed rock envelope. */
        after.biome=ATLAS_TUNDRA;after.elevation=230;
        floor=AtlasTerrain_BaseTile(site.x,site.y,&after,TRUE);
        assert((floor&1023)>=ATLAS_NATURE_SNOW_FIRST && (floor&1023)<ATLAS_NATURE_SNOW_FIRST+4);
        assert(!AtlasVolcano_Bare(site.x+site.radius,site.y));
        s32 x=site.x,y=site.y+site.radius-32*(site.radius-site.radius/4)/site.uplift;
        u16 map[35*34];AtlasStream_Reset();
        assert(AtlasStream_Load(map,AtlasTerrain_FloorDiv(x,20),AtlasTerrain_FloorDiv(y,20)));
        for(unsigned i=0;i<480;i++)
        {
            s32 wx=x+(s32)i/8,wy=y;
            AtlasStream_Tick(AtlasTerrain_FloorDiv(wx,20),AtlasTerrain_FloorDiv(wy,20),wx%20,wy-AtlasTerrain_FloorDiv(wy,20)*20);
            u16 semantic;assert(AtlasStream_ReadSemantics(wx,wy,&semantic));
            u16 direct=AtlasTiles_Describe(wx,wy,&floor);assert(semantic==direct);
        }
        printf("PASS generation14 seed=%u: native rock floor, all 256 cliff topologies preserve collision/elevation, snow priority, unchanged climate/geography, streamed/direct cone semantics\n",seed);
    }
}
