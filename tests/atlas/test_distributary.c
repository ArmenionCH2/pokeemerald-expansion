#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode prior[ATLAS_PLAN_COUNT];
static struct CreekLine lines[128];
static unsigned saved;
int main(void)
{
    setbuf(stdout,NULL);assert(sizeof(struct WaterBucket)==68);
    const u32 seeds[]={0,1,0xffffffffu,0xdeadbeefu,4220706268u,123456789,217046931};
    for(unsigned s=0;s<7;s++)
    {
        assert(AtlasWorld_SetGeneration(25)&&AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));memcpy(prior,gAtlasPlan,sizeof(prior));
        assert(AtlasWorld_SetGeneration(26)&&AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));assert(!memcmp(prior,gAtlasPlan,sizeof(prior)));
        saved=0;unsigned accepted=0;
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)if(AtlasWater_IsEstuary(id))
        {
            s32 x,y,dx,dy;unsigned status,steps=0;
            do{status=AtlasWater_MouthStep(id,&x,&y,&dx,&dy);assert(++steps<256);}while(!status);
            if(status!=1)continue;
            struct WaterBucket fit=sBuckets[sActive];steps=0;
            while(!DistributaryStep(&fit,id))assert(++steps<=34);
            if(!fit.deltaValid)continue;
            accepted++;
            for(unsigned side=0;side<2;side++)
            {
                struct CreekLine line;assert(DistributaryLine(&fit,id,side,&line));
                assert(saved<128);lines[saved++]=line;
                for(unsigned at=0;at<=64;at++)
                {
                    s32 sx=line.ax+(s64)(line.bx-line.ax)*at/64,sy=line.ay+(s64)(line.by-line.ay)*at/64;
                    struct AtlasCell c;AtlasTerrain_Sample(sx,sy,&c);
                    if(!c.waterform || c.elevation!=ATLAS_PLAN_SEA)printf("BAD seed=%u node=%u side=%u at=%u %d,%d water=%u height=%u\n",seeds[s],id,side,at,sx,sy,c.waterform,c.elevation);
                    assert(c.waterform && c.elevation==ATLAS_PLAN_SEA);
                }
            }
        }
        assert(accepted);
        AtlasGeo_Begin(ATLAS_GEO_DISTRIBUTARY);unsigned steps=0;
        while(!AtlasGeo_Step())assert(++steps<ATLAS_GEO_SEARCH_STEPS);
        printf("TP seed=%u status=%u steps=%u x=%d y=%d\n",seeds[s],gAtlasGeoStatus,steps,gAtlasGeoResult.x,gAtlasGeoResult.y);
        assert(gAtlasGeoStatus==1);
        for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)assert(!(AtlasTerrain_Tile(gAtlasGeoResult.x+x,gAtlasGeoResult.y+y)&0xC00));
        assert(AtlasWater_DistributaryNear(gAtlasGeoResult.x,gAtlasGeoResult.y,4));
        assert(AtlasWorld_SetGeneration(25)&&AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));
        unsigned added=0;
        for(unsigned i=0;i<saved;i++)for(unsigned at=8;at<=48;at+=8)
        {
            struct CreekLine *line=&lines[i];struct AtlasCell old;
            s32 x=line->ax+(s64)(line->bx-line->ax)*at/64,y=line->ay+(s64)(line->by-line->ay)*at/64;
            AtlasTerrain_Sample(x,y,&old);if(!old.waterform)added++;
            assert(!AtlasWater_DistributaryNear(x,y,4));
        }
        assert(added);AtlasGeo_Begin(ATLAS_GEO_DISTRIBUTARY);assert(AtlasGeo_Step()==3);
        printf("PASS seed=%u: %u two-branch mouths, %u new water samples, connected level centerlines, safe inspector, unchanged macro notes and old-world gate\n",seeds[s],accepted,added);
    }
}
