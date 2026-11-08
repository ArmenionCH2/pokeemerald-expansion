#include "global.h"
#include "atlas_plan.h"
#include "atlas_world.h"
#include <assert.h>
#include <stdio.h>
static struct AtlasPlanNode saved[ATLAS_PLAN_COUNT];
static void generate(u32 seed, u16 batch)
{
    u8 last = 0;
    assert(AtlasPlan_Begin(seed));
    while (!AtlasPlan_Step(batch)) { u8 p = AtlasPlan_Progress(); assert(p >= last && p <= 100); last = p; }
    assert(AtlasPlan_Progress() == 100);
}
/* Independent original 96-pass climate model: the fast solver must be exact. */
static void check_original_climate(void)
{
    u8 vapor[ATLAS_PLAN_COUNT] = {0}, next[ATLAS_PLAN_COUNT];
    unsigned pass, id;
    for (pass = 0; pass < ATLAS_PLAN_WIDTH; pass++)
    {
        for (id = 0; id < ATLAS_PLAN_COUNT; id++)
        {
            unsigned row = id / ATLAS_PLAN_WIDTH, x = id % ATLAS_PLAN_WIDTH;
            unsigned up = row * ATLAS_PLAN_WIDTH + (x +
                (row < ATLAS_PLAN_HEIGHT / 3 || row >= 2 * ATLAS_PLAN_HEIGHT / 3 ? 1 : ATLAS_PLAN_WIDTH - 1)) % ATLAS_PLAN_WIDTH;
            int rise = (int)gAtlasPlan[id].height - gAtlasPlan[up].height;
            int value = gAtlasPlan[id].height < ATLAS_PLAN_SEA ? 230 : vapor[up] - 3;
            if (gAtlasPlan[id].height >= ATLAS_PLAN_SEA && rise > 0) value -= rise / 2;
            if (value < 12) value = 12;
            next[id] = value;
            if (pass == ATLAS_PLAN_WIDTH - 1)
            {
                int rain = 35 + value / 2;
                if (rise > 0 && gAtlasPlan[id].height >= ATLAS_PLAN_SEA) rain += rise;
                if (row > ATLAS_PLAN_HEIGHT * 2 / 5 && row < ATLAS_PLAN_HEIGHT * 3 / 5) rain += 35;
                if (rain > 255) rain = 255;
                assert(gAtlasPlan[id].humidity == value);
                assert(gAtlasPlan[id].rain == rain);
            }
        }
        memcpy(vapor, next, sizeof(vapor));
    }
}
int main(void)
{
    u32 seeds[] = {0,1,2,0x41544C33,0xFFFFFFFF,0x80000000,123456789};
    u32 i, id, checksum = 0;
    assert(sizeof(gAtlasPlan) == ATLAS_PLAN_BYTES);
    for(i=0;i<sizeof(seeds)/sizeof(seeds[0]);i++)
    {
        generate(seeds[i], 256);
        check_original_climate();
        assert(gAtlasPlanStats.land && gAtlasPlanStats.land < ATLAS_PLAN_COUNT);
        assert(gAtlasPlanStats.continents);
        for(id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            u16 at = id, steps = 0, p = gAtlasPlan[id].parent;
            s32 dx = (s32)(id % ATLAS_PLAN_WIDTH) - p % ATLAS_PLAN_WIDTH, dy = (s32)(id / ATLAS_PLAN_WIDTH) - p / ATLAS_PLAN_WIDTH;
            assert(p < ATLAS_PLAN_COUNT);
            assert(gAtlasPlan[id].water >= gAtlasPlan[id].height);
            assert(gAtlasPlan[p].water <= gAtlasPlan[id].water);
            if(gAtlasPlan[id].height >= ATLAS_PLAN_SEA){
                assert(gAtlasPlan[id].basin & ATLAS_BASIN_MASK);
                if(gAtlasPlan[p].height >= ATLAS_PLAN_SEA)
                    assert((gAtlasPlan[id].basin & ATLAS_BASIN_MASK)==(gAtlasPlan[p].basin & ATLAS_BASIN_MASK));
                else assert((gAtlasPlan[id].basin & ATLAS_BASIN_MASK)==id+1);
                if(gAtlasPlan[id].basin & ATLAS_LAKE_FLAG)assert(gAtlasPlan[id].water>gAtlasPlan[id].height+3);
            }
            assert(p == id || ((dx == 1 || dx == -1 || dx == ATLAS_PLAN_WIDTH - 1 || dx == 1 - ATLAS_PLAN_WIDTH) && !dy) || (!dx && (dy == 1 || dy == -1)));
            while (gAtlasPlan[at].parent != at) { at = gAtlasPlan[at].parent; assert(++steps < ATLAS_PLAN_COUNT); }
            assert(gAtlasPlan[at].height < ATLAS_PLAN_SEA);
            if(gAtlasPlan[id].height >= ATLAS_PLAN_SEA)
            {
                assert(gAtlasPlan[id].continent);
                if(gAtlasPlan[p].height >= ATLAS_PLAN_SEA) assert(gAtlasPlan[p].runoff >= gAtlasPlan[id].runoff);
                if(gAtlasPlan[(id / ATLAS_PLAN_WIDTH) * ATLAS_PLAN_WIDTH + ((id + 1) % ATLAS_PLAN_WIDTH)].height >= ATLAS_PLAN_SEA)
                    assert(gAtlasPlan[(id / ATLAS_PLAN_WIDTH) * ATLAS_PLAN_WIDTH + ((id + 1) % ATLAS_PLAN_WIDTH)].continent == gAtlasPlan[id].continent);
            }
        }
        {
            unsigned channels=0, lakes=0;
            for(id=0;id<ATLAS_PLAN_COUNT;id++){
                if(gAtlasPlan[id].basin & ATLAS_LAKE_FLAG)lakes++;
                if(gAtlasPlan[id].height>=ATLAS_PLAN_SEA && gAtlasPlan[id].runoff>=600){
                    unsigned part;u16 previous=65535;
                    {
                        u8 previousSurface=255;unsigned segment,step;
                        for(segment=0;segment<4;segment++){
                            s32 ax,ay,bx,by;u16 level;
                            AtlasWater_Point(id,segment,&ax,&ay,&level);AtlasWater_Point(id,segment+1,&bx,&by,&level);
                            for(step=0;step<16;step++){
                                s32 x=ax+(bx-ax)*(s32)step/16,y=ay+(by-ay)*(s32)step/16;struct AtlasCell c;
                                x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
                                AtlasTerrain_Sample(x,y,&c);
                                if(c.elevation>previousSurface)fprintf(stderr,"Channel rise seed=%u node=%u segment=%u step=%u old=%u new=%u water=%u\n",seeds[i],id,segment,step,previousSurface,c.elevation,c.waterform);
                                assert(c.elevation<=previousSurface);previousSurface=c.elevation;
                            }
                        }
                    }
                    for(part=0;part<=4;part++){
                        s32 x,y;u16 height;struct AtlasCell cell;
                        AtlasWater_Point(id,part,&x,&y,&height);assert(height<=previous);previous=height;
                        if(part==1||part==2||part==3){
                            x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
                            AtlasTerrain_Sample(x,y,&cell);
                            if(cell.waterform==ATLAS_DRY)fprintf(stderr,"Missing river seed=%u node=%u part=%u x=%d y=%d\n",seeds[i],id,part,x,y);
                            assert(cell.waterform!=ATLAS_DRY);channels++;
                        }
                    }
                }
            }
            assert(channels);printf("Water samples %u; retained lake nodes %u; basins %u; lake components %u\n",channels,lakes,gAtlasPlanStats.basins,gAtlasPlanStats.lakes);
        }
        {
            unsigned row,col,part,pairs=0;
            for(row=0;row<ATLAS_PLAN_HEIGHT-1;row++)for(col=0;col<ATLAS_PLAN_WIDTH;col++)for(part=1;part<4;part++){
                struct AtlasCell a,b;unsigned axis;
                for(axis=0;axis<2;axis++){
                    s32 x=(u32)col*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
                    s32 y=(row*4+part)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))/4-ATLAS_WORLD_HALF_HEIGHT;
                    if(axis){x=(u32)col*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH+part*(ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH)/4;y=row*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;}
                    AtlasTerrain_Sample(x-(axis?0:1),y-(axis?1:0),&a);
                    AtlasTerrain_Sample(x+(axis?0:1),y+(axis?1:0),&b);
                    if(a.waterform==ATLAS_LAKE&&b.waterform==ATLAS_LAKE){assert(a.elevation==b.elevation);pairs++;}
                }
            }
            printf("PASS lake boundary surfaces: %u paired samples\n",pairs);
        }
        if(i) assert(checksum != gAtlasPlanStats.checksum);
        checksum = gAtlasPlanStats.checksum;
        assert(gAtlasPlanStats.habitatChecksum&&gAtlasPlanStats.cold&&gAtlasPlanStats.wet);
        memcpy(saved, gAtlasPlan, sizeof(saved));
        generate(seeds[i], 7);
        assert(checksum == gAtlasPlanStats.checksum);
        assert(!memcmp(saved, gAtlasPlan, sizeof(saved)));
        printf("PASS seed %u checksum %08x: land %u, landmasses %u, major drainage nodes %u\n",seeds[i],checksum,gAtlasPlanStats.land,gAtlasPlanStats.continents,gAtlasPlanStats.rivers);
    }
    {
        struct AtlasHabitat a,b,c;
        AtlasHabitat_Sample(0,140,30,100,ATLAS_DRY,&a);
        AtlasHabitat_Sample(0,140,230,100,ATLAS_DRY,&b);
        assert(a.humidity==b.humidity&&a.moisture<b.moisture);
        AtlasHabitat_Sample(ATLAS_WORLD_HALF_HEIGHT-1,140,230,100,ATLAS_DRY,&c);
        assert(c.temperature<b.temperature&&c.seasonalRange>b.seasonalRange&&c.humidity>b.humidity);
        AtlasHabitat_Sample(0,140,30,100,ATLAS_LAKE,&c);assert(c.moisture==255);
    }
    puts("PASS: drainage adjacency, ocean termination, no cycles, runoff accumulation, wrapped land identity, batch-independent reconstruction");
}
