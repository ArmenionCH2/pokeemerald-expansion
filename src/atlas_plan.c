#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "malloc.h"

#ifndef __arm__
// Host contracts use ordinary memory; the GBA uses the scene reservation.
EWRAM_DATA struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT] = {{0}};
#endif
EWRAM_DATA bool8 gAtlasPlanReserved = FALSE;
void AtlasPlan_ReserveHeap(void)
{
#ifdef __arm__
    // Call only after the preceding scene has released its heap-owned UI.
    gAtlasPlanReserved = TRUE;
    InitHeap(gHeap, HEAP_SIZE);
#endif
}
EWRAM_DATA struct AtlasPlanStats gAtlasPlanStats = {0};
EWRAM_DATA u32 gAtlasWorldSeed = 0;
EWRAM_DATA bool8 gAtlasPlanReady = FALSE;
struct PlanScratch {
    u16 heap[ATLAS_PLAN_COUNT], order[ATLAS_PLAN_COUNT];
    u8 level[ATLAS_PLAN_COUNT], rain[ATLAS_PLAN_COUNT];
    u16 heights[256];
};
static EWRAM_DATA u16 sAtlasGenerationVersion = 0, sCoastalTies = 0;
static EWRAM_DATA u8 sSeaCutoff = 0;
static EWRAM_DATA u16 sOceans = 0;
static EWRAM_DATA bool8 sWaterPass = FALSE;
u16 AtlasWorld_Generation(void) {return sAtlasGenerationVersion?sAtlasGenerationVersion:ATLAS_GENERATION_LEGACY;}
bool8 AtlasWorld_SetGeneration(u16 version)
{
    if (version<ATLAS_GENERATION_LEGACY || version>ATLAS_GENERATION_VERSION) return FALSE;
    sAtlasGenerationVersion=version;return TRUE;
}
EWRAM_DATA static struct PlanScratch *sScratch = {0};
EWRAM_DATA static u16 sAt = 0, sHeapSize = 0, sOrderSize = 0, sFlooded = 0, sQueueHead = 0, sQueueTail = 0;
EWRAM_DATA static u8 sStage = 0, sRainPass = 0;
EWRAM_DATA static u16 sWindAt = 0;
EWRAM_DATA static bool8 sDryRow = 0;
EWRAM_DATA static bool8 sLabeling = 0;
EWRAM_DATA static struct AtlasPlanSample sSample = {0};
EWRAM_DATA static s32 sSampleX0 = 0, sSampleX1 = 0, sSampleY0 = 0, sSampleY1 = 0;
EWRAM_DATA static bool8 sSampleValid = 0;

void AtlasPlan_ResetSample(void) { sSampleValid = FALSE; }

static u32 Mix(u32 n)
{
    n ^= n >> 16; n *= 0x7FEB352D;
    n ^= n >> 15; n *= 0x846CA68B;
    return n ^ (n >> 16);
}
static u16 Neighbor(u16 id, u8 dir)
{
    u16 x = id % ATLAS_PLAN_WIDTH, y = id / ATLAS_PLAN_WIDTH;
    if (dir == 0) x = (x + 1) % ATLAS_PLAN_WIDTH;
    if (dir == 1) x = (x + ATLAS_PLAN_WIDTH - 1) % ATLAS_PLAN_WIDTH;
    if (dir == 2) { if (!y) return id; y--; }
    if (dir == 3) { if (y == ATLAS_PLAN_HEIGHT - 1) return id; y++; }
    return y * ATLAS_PLAN_WIDTH + x;
}
static bool8 Before(u16 a, u16 b)
{
    return sScratch->level[a] < sScratch->level[b]
        || (sScratch->level[a] == sScratch->level[b] && a < b);
}
static void Push(u16 id)
{
    u16 at = sHeapSize++;
    while (at && Before(id, sScratch->heap[(at - 1) / 2]))
    {
        sScratch->heap[at] = sScratch->heap[(at - 1) / 2]; at = (at - 1) / 2;
    }
    sScratch->heap[at] = id;
}
static u16 Pop(void)
{
    u16 result = sScratch->heap[0], value = sScratch->heap[--sHeapSize], at = 0, child;
    while ((child = at * 2 + 1) < sHeapSize)
    {
        if (child + 1 < sHeapSize && Before(sScratch->heap[child + 1], sScratch->heap[child])) child++;
        if (!Before(sScratch->heap[child], value)) break;
        sScratch->heap[at] = sScratch->heap[child]; at = child;
    }
    if (sHeapSize) sScratch->heap[at] = value;
    return result;
}
bool8 AtlasPlan_Begin(u32 seed)
{
    AtlasPlan_ResetSample();
    AtlasStream_Reset();
    if (sScratch) Free(sScratch);
#ifdef __arm__
    sScratch = AllocZeroedUnchecked(sizeof(*sScratch));
#else
    sScratch = AllocZeroed(sizeof(*sScratch));
#endif
    if (!sScratch)
    {
        /* The old plan is no longer restartable after its scratch was freed.
         * Never expose its stale ready/progress state to later consumers. */
        gAtlasPlanReady = FALSE;
        sAt = sHeapSize = sOrderSize = sFlooded = sQueueHead = sQueueTail = 0;
        sStage = sRainPass = 0; sLabeling = FALSE;
        return FALSE;
    }
    memset(gAtlasPlan, 0, sizeof(gAtlasPlan));
    memset(&gAtlasPlanStats, 0, sizeof(gAtlasPlanStats));
    gAtlasWorldSeed = gAtlasPlanStats.seed = seed;
    AtlasTerrain_SetSeed(seed);
    AtlasWater_Reset();
    gAtlasPlanReady = FALSE;
    sSeaCutoff=0;sCoastalTies=0;sOceans=0;sWaterPass=FALSE;
    sAt = sHeapSize = sOrderSize = sFlooded = sQueueHead = sQueueTail = 0;
    sStage = sRainPass = 0; sLabeling = FALSE;
    return TRUE;
}
static u8 Mountain(u16 id)
{
    s32 x = (id % ATLAS_PLAN_WIDTH) * 256, y = (id / ATLAS_PLAN_WIDTH) * 256, best = 0;
    u32 i;
    for (i = 0; i < 12; i++)
    {
        u32 h = Mix(gAtlasWorldSeed ^ (i * 0x9E3779B1));
        s32 cx = (h % ATLAS_PLAN_WIDTH) * 256, cy = ((h >> 8) % ATLAS_PLAN_HEIGHT) * 256;
        s32 dx = x - cx, dy = y - cy, along, across, score;
        if (dx > ATLAS_PLAN_WIDTH * 128) dx -= ATLAS_PLAN_WIDTH * 256;
        if (dx < -ATLAS_PLAN_WIDTH * 128) dx += ATLAS_PLAN_WIDTH * 256;
        /* Slanted bounded range axes, with broad foothills. */
        if (h & 0x80000000) { along = dx; across = dy - dx / 3; }
        else { along = dy; across = dx + dy / 3; }
        if (along < 0) along = -along;
        if (across < 0) across = -across;
        score = 65 - across / 10 - along / 35;
        if (score > best) best = score;
    }
    return best;
}
static void Advance(void) { sStage++; sAt = 0; }
/* Five broad continental cores define the planetary wavelength. Longitude
 * distance is periodic; seeded offsets, aspect ratios and the former height
 * field give irregular coasts. This is a morphological model, not tectonics.
 * Evaluated only during macro creation, never during tile streaming. */
static u8 ContinentalHeight(u16 id,u8 detail)
{
    s32 x=id%ATLAS_PLAN_WIDTH,y=id/ATLAS_PLAN_WIDTH,best=-100000;
    unsigned i;
    for (i=0;i<5;i++)
    {
        u32 h=Mix(gAtlasWorldSeed^(0x434F4E54u+i*0x9E3779B1u));
        s32 cx=(i*ATLAS_PLAN_WIDTH/5+(h%11)+ATLAS_PLAN_WIDTH-5)%ATLAS_PLAN_WIDTH;
        s32 cy=8+(h>>8)%32,rx=11+(h>>16)%10,ry=8+(h>>24)%8;
        s32 dx=x-cx,dy=y-cy,score;
        if (dx>ATLAS_PLAN_WIDTH/2) dx-=ATLAS_PLAN_WIDTH;
        if (dx< -ATLAS_PLAN_WIDTH/2) dx+=ATLAS_PLAN_WIDTH;
        // Slant the axes slightly so outlines are not aligned with the grid.
        dx+=dy*((h&1)?1:-1)/3;
        score=220-(dx*dx*4096/(rx*rx)+dy*dy*4096/(ry*ry))/32;
        if (score>best) best=score;
    }
    best+=(detail-128)/3;
    return best<0?0:best>255?255:best;
}
bool8 AtlasPlan_ImportHeights(const u8 *heights)
{
    u16 i, land = 0;
    if (!sScratch || sStage || sAt || !heights) return FALSE;
    for (i = 0; i < ATLAS_PLAN_COUNT; i++)
        if (heights[i] >= ATLAS_PLAN_SEA) land++;
    /* Stage 3 has an exceptional forced-ocean correction on all-land worlds.
     * Their earlier climate cannot be recovered from final heights alone. */
    if (land == ATLAS_PLAN_COUNT) return FALSE;
    for (i = 0; i < ATLAS_PLAN_COUNT; i++)
    {
        gAtlasPlan[i].height = heights[i];
        gAtlasPlan[i].rain = 96;
        gAtlasPlan[i].parent = 0xFFFF;
    }
    gAtlasPlanStats.land = land;
    sStage = 2;
    return TRUE;
}
bool8 AtlasPlan_Step(u16 work)
{
    if (gAtlasPlanReady) return TRUE;
    if (!sScratch) return FALSE;
    while (work--)
    {
        struct AtlasPlanNode *n = &gAtlasPlan[sAt];
        if (sStage == 0)
        {
            struct AtlasCell c;
            s32 x = (u32)(sAt % ATLAS_PLAN_WIDTH) * ATLAS_WORLD_WIDTH / ATLAS_PLAN_WIDTH;
            s32 y = (sAt / ATLAS_PLAN_WIDTH) * (2 * ATLAS_WORLD_HALF_HEIGHT / (ATLAS_PLAN_HEIGHT - 1)) - ATLAS_WORLD_HALF_HEIGHT;
            if (y >= ATLAS_WORLD_HALF_HEIGHT) y = ATLAS_WORLD_HALF_HEIGHT - 1;
            AtlasTerrain_Sample(x, y, &c);
            n->height = c.elevation; n->rain = 96; n->parent = 0xFFFF;
            if (AtlasWorld_Generation()>=5) n->height=ContinentalHeight(sAt,c.elevation);
            if (AtlasWorld_Generation()>=4) sScratch->heights[n->height]++;
            if (++sAt == ATLAS_PLAN_COUNT)
            {
                if (AtlasWorld_Generation()>=4)
                {
                    unsigned h,count=0,target=ATLAS_PLAN_COUNT*292u/1000;
                    for (h=255;h>0;h--)
                    {
                        if (count+sScratch->heights[h]>=target) break;
                        count+=sScratch->heights[h];
                    }
                    sSeaCutoff=h;sCoastalTies=target-count;
                }
                Advance();
            }
        }
        else if (sStage == 1)
        {
            u16 id=sAt;
            s32 e;
            if (AtlasWorld_Generation()>=4)
            {
                // Coprime traversal distributes quantile ties across the globe,
                // rather than assigning equal-height shoreline cells north-first.
                id=((u32)sAt*2053+gAtlasWorldSeed%ATLAS_PLAN_COUNT)%ATLAS_PLAN_COUNT;
                n=&gAtlasPlan[id];e=(s32)n->height-sSeaCutoff+ATLAS_PLAN_SEA;
                if (n->height==sSeaCutoff)
                {
                    if (sCoastalTies) sCoastalTies--;
                    else e=ATLAS_PLAN_SEA-1;
                }
                if (e<0) e=0;
            }
            else e=n->height;
            if (e >= ATLAS_PLAN_SEA) { e += Mountain(id); gAtlasPlanStats.land++; }
            if (e > 255) e = 255;
            n->height = e;
            if (++sAt == ATLAS_PLAN_COUNT) Advance();
        }
        else if (sStage == 2)
        {
            u16 row = sAt / ATLAS_PLAN_WIDTH, upstream;
            u8 upDir = row < ATLAS_PLAN_HEIGHT / 3 || row >= 2 * ATLAS_PLAN_HEIGHT / 3 ? 0 : 1;
            s32 rain, rise, precipitation;
            /* Ocean cells reset vapor to 230. Walking downwind from one ocean
             * solves the periodic row exactly; an all-land row settles at 12.
             * This is the same fixed point as WIDTH Jacobi sweeps, in O(WIDTH). */
            if (sAt % ATLAS_PLAN_WIDTH == 0)
            {
                u16 x;
                sDryRow = TRUE;
                sWindAt = row * ATLAS_PLAN_WIDTH;
                for (x = 0; x < ATLAS_PLAN_WIDTH; x++)
                    if (gAtlasPlan[row * ATLAS_PLAN_WIDTH + x].height < ATLAS_PLAN_SEA)
                    {
                        sWindAt = row * ATLAS_PLAN_WIDTH + x;
                        sDryRow = FALSE;
                        break;
                    }
            }
            n = &gAtlasPlan[sWindAt];
            upstream = Neighbor(sWindAt, upDir);
            rise = (s32)n->height - gAtlasPlan[upstream].height;
            rain = n->height < ATLAS_PLAN_SEA ? 230 : gAtlasPlan[upstream].humidity - 3;
            if (n->height >= ATLAS_PLAN_SEA && rise > 0) rain -= rise / 2;
            if (rain < 12 || sDryRow) rain = 12;
            precipitation = 35 + rain / 2;
            if (rise > 0 && n->height >= ATLAS_PLAN_SEA) precipitation += rise;
            if (row > ATLAS_PLAN_HEIGHT * 2 / 5 && row < ATLAS_PLAN_HEIGHT * 3 / 5) precipitation += 35;
            if (precipitation > 255) precipitation = 255;
            n->rain = precipitation;
            n->humidity = rain;
            sWindAt = Neighbor(sWindAt, upDir ^ 1);
            if (++sAt % ATLAS_PLAN_WIDTH == 0) sRainPass++;
            if (sAt == ATLAS_PLAN_COUNT) Advance();
        }

        else if (sStage == 3)
        {
            sScratch->level[sAt] = n->height < ATLAS_PLAN_SEA ? ATLAS_PLAN_SEA : n->height;
            if (n->height < ATLAS_PLAN_SEA) { n->parent = sAt; Push(sAt); }
            if (++sAt == ATLAS_PLAN_COUNT)
            {
                /* Extremely land-heavy seeds still have a deterministic ocean. */
                if (!sHeapSize) { gAtlasPlan[0].height = 0; gAtlasPlan[0].parent = 0; sScratch->level[0] = ATLAS_PLAN_SEA; Push(0); }
                Advance();
            }
        }
        else if (sStage == 4)
        {
            u16 id = Pop();
            u8 d;
            sScratch->order[sOrderSize++] = id; sFlooded++;
            for (d = 0; d < 4; d++)
            {
                u16 other = Neighbor(id, d);
                if (gAtlasPlan[other].parent != 0xFFFF) continue;
                gAtlasPlan[other].parent = id;
                sScratch->level[other] = gAtlasPlan[other].height > sScratch->level[id] ? gAtlasPlan[other].height : sScratch->level[id];
                Push(other);
            }
            if (!sHeapSize) Advance();
        }
        else if (sStage == 5)
        {
            u16 id = sScratch->order[ATLAS_PLAN_COUNT - 1 - sAt], parent = gAtlasPlan[id].parent;
            u32 runoff = gAtlasPlan[id].runoff;
            if (gAtlasPlan[id].height >= ATLAS_PLAN_SEA)
            {
                runoff += gAtlasPlan[id].rain > 60 ? gAtlasPlan[id].rain - 60 : 1;
                if (runoff > 65535) runoff = 65535;
                gAtlasPlan[id].runoff = runoff;
                if (parent != id)
                {
                    runoff += gAtlasPlan[parent].runoff;
                    gAtlasPlan[parent].runoff = runoff > 65535 ? 65535 : runoff;
                }
                if (gAtlasPlan[id].runoff > 600) gAtlasPlanStats.rivers++;

            }
            if (++sAt == ATLAS_PLAN_COUNT) Advance();
        }
        else if (sStage == 6)
        {
            u16 id = sScratch->order[sAt], parent = gAtlasPlan[id].parent;
            struct AtlasPlanNode *node = &gAtlasPlan[id];
            node->water = sScratch->level[id];
            if (node->height >= ATLAS_PLAN_SEA)
            {
                if (gAtlasPlan[parent].height < ATLAS_PLAN_SEA)
                {
                    node->basin = id + 1; gAtlasPlanStats.basins++;
                }
                else node->basin = gAtlasPlan[parent].basin & ATLAS_BASIN_MASK;
                /* Retain wet depressions; a spill level alone does not create a lake. */
                if (node->water > node->height + 3 && node->runoff > 180 && node->rain > 65)
                    node->basin |= ATLAS_LAKE_FLAG;
            }
            if (++sAt == ATLAS_PLAN_COUNT) { memset(sScratch->rain, 0, sizeof(sScratch->rain)); Advance(); }
        }
        else if (sStage == 7)
        {
            if (sLabeling)
            {
                u16 id = sScratch->heap[sQueueHead++]; u8 d;
                for (d = 0; d < 4; d++)
                {
                    u16 other = Neighbor(id, d);
                    if ((sWaterPass ? gAtlasPlan[other].height >= ATLAS_PLAN_SEA : gAtlasPlan[other].height < ATLAS_PLAN_SEA)
                     || gAtlasPlan[other].continent) continue;
                    gAtlasPlan[other].continent = sWaterPass ? ATLAS_OCEAN_COMPONENT | sOceans : gAtlasPlanStats.continents;
                    sScratch->heap[sQueueTail++] = other;
                }
                if (sQueueHead == sQueueTail) sLabeling = FALSE;
            }
            else
            {
                if ((sWaterPass ? n->height < ATLAS_PLAN_SEA : n->height >= ATLAS_PLAN_SEA) && !n->continent)
                {
                    n->continent = sWaterPass ? ATLAS_OCEAN_COMPONENT | ++sOceans : ++gAtlasPlanStats.continents;
                    sQueueHead = 0; sQueueTail = 1; sScratch->heap[0] = sAt; sLabeling = TRUE;
                }
                if (++sAt == ATLAS_PLAN_COUNT)
                {
                    if (sLabeling) sAt--;
                    else if (AtlasWorld_Generation()>=4 && !sWaterPass) {sWaterPass=TRUE;sAt=0;}
                    else Advance();
                }
            }
        }
        else if (sStage == 8)
        {
            if (sLabeling)
            {
                u16 id = sScratch->heap[sQueueHead++]; u8 d;
                for (d = 0; d < 4; d++)
                {
                    u16 other = Neighbor(id, d);
                    if (!(gAtlasPlan[other].basin & ATLAS_LAKE_FLAG) || sScratch->rain[other]
                        || gAtlasPlan[other].water != gAtlasPlan[id].water) continue;
                    sScratch->rain[other] = 1; sScratch->heap[sQueueTail++] = other;
                }
                if (sQueueHead == sQueueTail) sLabeling = FALSE;
            }
            else
            {
                if ((n->basin & ATLAS_LAKE_FLAG) && !sScratch->rain[sAt])
                {
                    gAtlasPlanStats.lakes++; sScratch->rain[sAt] = 1;
                    sQueueHead = 0; sQueueTail = 1; sScratch->heap[0] = sAt; sLabeling = TRUE;
                }
                if (++sAt == ATLAS_PLAN_COUNT) { if (sLabeling) sAt--; else Advance(); }
            }
        }
        else if (sStage == 9)
        {
            struct AtlasHabitat habitat;
            s32 y = (sAt / ATLAS_PLAN_WIDTH) * (2 * ATLAS_WORLD_HALF_HEIGHT / (ATLAS_PLAN_HEIGHT - 1)) - ATLAS_WORLD_HALF_HEIGHT;
            u8 waterform = n->height < ATLAS_PLAN_SEA ? ATLAS_SEA : (n->basin & ATLAS_LAKE_FLAG) ? ATLAS_LAKE : ATLAS_DRY;
            AtlasHabitat_Sample(y, n->height, n->rain, n->humidity, waterform, &habitat);
            if (n->height >= ATLAS_PLAN_SEA)
            {
                if (habitat.temperature < 70) gAtlasPlanStats.cold++;
                if (habitat.moisture > 155) gAtlasPlanStats.wet++;
            }
            gAtlasPlanStats.habitatChecksum = Mix(gAtlasPlanStats.habitatChecksum
                ^ ((u32)habitat.temperature << 24) ^ ((u32)habitat.humidity << 16)
                ^ ((u32)habitat.moisture << 8) ^ habitat.seasonalRange);
            if (++sAt == ATLAS_PLAN_COUNT) Advance();
        }
        else
        {
            u32 h = gAtlasPlanStats.checksum;
            h = Mix(h ^ ((u32)n->height << 24) ^ ((u32)n->rain << 16) ^ n->parent);
            gAtlasPlanStats.checksum = Mix(h ^ n->runoff ^ ((u32)n->continent << 16));
            gAtlasPlanStats.checksum = Mix(gAtlasPlanStats.checksum ^ n->basin ^ ((u32)n->water << 16) ^ ((u32)n->humidity << 24));
            if (++sAt == ATLAS_PLAN_COUNT)
            {
                Free(sScratch); sScratch = NULL; gAtlasPlanReady = TRUE; sStage = ATLAS_PLAN_STAGES;
                return TRUE;
            }
        }
    }
    return FALSE;
}
u8 AtlasPlan_Stage(void) { return sStage; }
u8 AtlasPlan_Progress(void)
{
    if (sStage >= ATLAS_PLAN_STAGES) return 100;
    if (sStage == 0) return (u32)sAt * 7 / ATLAS_PLAN_COUNT;
    if (sStage == 1) return 7 + (u32)sAt * 7 / ATLAS_PLAN_COUNT;
    if (sStage == 2) return 14 + sRainPass * 48 / ATLAS_PLAN_HEIGHT;
    return 62 + (sStage - 3) * 4 + (u32)sAt * 4 / ATLAS_PLAN_COUNT;
}
static u8 Blend(u32 a, u32 b, u32 c, u32 d, u32 fx, u32 fy)
{
    return ((a * (256 - fx) + b * fx) * (256 - fy)
        + (c * (256 - fx) + d * fx) * fy) >> 16;
}
static void SampleDetail(s32 x, s32 y, struct AtlasPlanSample *sample)
{
    u32 sx = (u32)x * ATLAS_PLAN_WIDTH, sy = (u32)(y + ATLAS_WORLD_HALF_HEIGHT) * (ATLAS_PLAN_HEIGHT - 1);
    u32 col = sx / ATLAS_WORLD_WIDTH, row = sy / (2 * ATLAS_WORLD_HALF_HEIGHT);
    u32 fx = (sx % ATLAS_WORLD_WIDTH) / (ATLAS_WORLD_WIDTH / 256);
    u32 fy = (sy % (2 * ATLAS_WORLD_HALF_HEIGHT)) / (2 * ATLAS_WORLD_HALF_HEIGHT / 256);
    u32 next = (col + 1) % ATLAS_PLAN_WIDTH, lower = row < ATLAS_PLAN_HEIGHT - 1 ? row + 1 : row;
    struct AtlasPlanNode *a = &gAtlasPlan[row * ATLAS_PLAN_WIDTH + col], *b = &gAtlasPlan[row * ATLAS_PLAN_WIDTH + next];
    struct AtlasPlanNode *c = &gAtlasPlan[lower * ATLAS_PLAN_WIDTH + col], *d = &gAtlasPlan[lower * ATLAS_PLAN_WIDTH + next];
    struct AtlasPlanNode *corners[4];
    u16 ids[4]; u8 k, lake = 0, chosenLevel = ATLAS_PLAN_SEA;
    u32 nearest = 0xFFFFFFFF; s32 lakeFloor = 255;
    if (fx > 255) fx = 255;
    if (fy > 255) fy = 255;
    /* Exact interval of coordinates with identical integer interpolation
     * inputs. Lake fitting also depends only on these inputs, not raw x/y. */
    sSampleX0 = (col * ATLAS_WORLD_WIDTH + fx * (ATLAS_WORLD_WIDTH / 256) + ATLAS_PLAN_WIDTH - 1) / ATLAS_PLAN_WIDTH;
    sSampleX1 = (col * ATLAS_WORLD_WIDTH + (fx == 255 ? ATLAS_WORLD_WIDTH : (fx + 1) * (ATLAS_WORLD_WIDTH / 256)) + ATLAS_PLAN_WIDTH - 1) / ATLAS_PLAN_WIDTH;
    sSampleY0 = (row * (2 * ATLAS_WORLD_HALF_HEIGHT) + fy * (2 * ATLAS_WORLD_HALF_HEIGHT / 256) + ATLAS_PLAN_HEIGHT - 2) / (ATLAS_PLAN_HEIGHT - 1) - ATLAS_WORLD_HALF_HEIGHT;
    sSampleY1 = (row * (2 * ATLAS_WORLD_HALF_HEIGHT) + (fy == 255 ? 2 * ATLAS_WORLD_HALF_HEIGHT : (fy + 1) * (2 * ATLAS_WORLD_HALF_HEIGHT / 256)) + ATLAS_PLAN_HEIGHT - 2) / (ATLAS_PLAN_HEIGHT - 1) - ATLAS_WORLD_HALF_HEIGHT;
    sample->height = Blend(a->height,b->height,c->height,d->height,fx,fy);
    sample->rain = Blend(a->rain,b->rain,c->rain,d->rain,fx,fy);
    sample->humidity = Blend(a->humidity,b->humidity,c->humidity,d->humidity,fx,fy);
    corners[0]=a;corners[1]=b;corners[2]=c;corners[3]=d;
    ids[0]=row*ATLAS_PLAN_WIDTH+col;ids[1]=row*ATLAS_PLAN_WIDTH+next;
    ids[2]=lower*ATLAS_PLAN_WIDTH+col;ids[3]=lower*ATLAS_PLAN_WIDTH+next;
    /* Flat basin patches; different spill levels cannot overlap as water. */
    for(k=0;k<4;k++)
    {
        struct AtlasPlanNode *n=corners[k];
        s32 dx=(s32)fx-(k%2?256:0),dy=(s32)fy-(k/2?256:0);
        s32 depth=(s32)n->water-n->height, radius,rx,ry,floor;
        u16 other;u32 distance;
        if(!(n->basin&ATLAS_LAKE_FLAG))continue;
        radius=64+depth*3;if(radius>112)radius=112;
        rx=ry=radius;
        other=Neighbor(ids[k],dx<0?1:0);
        if((gAtlasPlan[other].basin&ATLAS_LAKE_FLAG)&&gAtlasPlan[other].water==n->water)rx=144;
        other=Neighbor(ids[k],dy<0?2:3);
        if((gAtlasPlan[other].basin&ATLAS_LAKE_FLAG)&&gAtlasPlan[other].water==n->water)ry=144;
        if(dx>rx*5/4||dx<-rx*5/4||dy>ry*5/4||dy<-ry*5/4)continue;
        distance=(u32)(dx*dx)*65536/(rx*rx)+(u32)(dy*dy)*65536/(ry*ry);
        if(distance>98304)continue;
        if(distance<65536)
        {
            floor=n->water-depth+(u32)depth*distance/65536;
            if(!lake||distance<nearest){chosenLevel=n->water;nearest=distance;}
            if(!lake||floor<lakeFloor)lakeFloor=floor;
            lake=1;
        }
        else if(!lake)
        {
            floor=n->water+1;
            /* Taper the fitted bank into the unmodified surrounding relief. */
            floor=(floor*(98304-distance)+(u32)sample->height*(distance-65536))/32768;
            if(distance<nearest){lakeFloor=floor;chosenLevel=n->water;nearest=distance;}
        }
    }
    if(lakeFloor<255 && lakeFloor>sample->height)sample->height=lakeFloor;
    sample->water=chosenLevel;sample->lake=lake?1:nearest!=0xFFFFFFFF?2:0;

}
void AtlasPlan_SampleDetail(s32 x, s32 y, struct AtlasPlanSample *sample)
{
    if (!sSampleValid || x < sSampleX0 || x >= sSampleX1 || y < sSampleY0 || y >= sSampleY1)
    {
        SampleDetail(x, y, &sSample);
        sSampleValid = TRUE;
    }
    *sample = sSample;
}
void AtlasPlan_Sample(s32 x, s32 y, u8 *height, u8 *rain)
{
    struct AtlasPlanSample s;
    AtlasPlan_SampleDetail(x,y,&s); *height = s.height; *rain = s.rain;
}
