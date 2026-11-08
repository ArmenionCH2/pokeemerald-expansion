/* Host regression: compile against the real atlas_plan.c, not a cache model.
 * Input coordinates are valid native world coordinates only. */
#include "global.h"
#include "atlas_plan.h"
#include "atlas_world.h"
#include <assert.h>
#include <stdio.h>

static u32 sRand = 1;
static u32 Next(void)
{
    sRand ^= sRand << 13;
    sRand ^= sRand >> 17;
    sRand ^= sRand << 5;
    return sRand;
}
static s32 ClampY(s32 y)
{
    if (y < -ATLAS_WORLD_HALF_HEIGHT) return -ATLAS_WORLD_HALF_HEIGHT;
    if (y >= ATLAS_WORLD_HALF_HEIGHT) return ATLAS_WORLD_HALF_HEIGHT - 1;
    return y;
}
static s32 WrapX(s64 x)
{
    x %= ATLAS_WORLD_WIDTH;
    if (x < 0) x += ATLAS_WORLD_WIDTH;
    return (s32)x;
}
static void FillPlan(u32 seed)
{
    u32 i;
    for (i = 0; i < ATLAS_PLAN_COUNT; i++)
    {
        struct AtlasPlanNode *n = &gAtlasPlan[i];
        u32 v = (i * 2654435761u) ^ (seed * 2246822519u);
        v ^= v >> 16; v *= 0x7FEB352Du; v ^= v >> 15;
        n->height = (u8)(v % 224u);
        n->rain = (u8)(v >> 8);
        n->humidity = (u8)(v >> 16);
        n->water = n->height;
        n->basin = 0;
        if ((v & 7u) == 0u)
        {
            n->water = (u8)(n->height + 1u + ((v >> 5) & 15u));
            n->basin = ATLAS_LAKE_FLAG | (u16)(i + 1u);
        }
    }
    AtlasPlan_ResetSample(); /* Mandatory after changing plan backing data. */
}
static void Equal(const struct AtlasPlanSample *a, const struct AtlasPlanSample *b)
{
    assert(a->height == b->height);
    assert(a->rain == b->rain);
    assert(a->humidity == b->humidity);
    assert(a->water == b->water);
    assert(a->lake == b->lake);
}
static void CompareWarmCold(s32 oldX, s32 oldY, s32 x, s32 y)
{
    struct AtlasPlanSample cold, warm, discarded;
    assert(oldX >= 0 && oldX < ATLAS_WORLD_WIDTH);
    assert(x >= 0 && x < ATLAS_WORLD_WIDTH);
    assert(oldY >= -ATLAS_WORLD_HALF_HEIGHT && oldY < ATLAS_WORLD_HALF_HEIGHT);
    assert(y >= -ATLAS_WORLD_HALF_HEIGHT && y < ATLAS_WORLD_HALF_HEIGHT);
    AtlasPlan_ResetSample();
    AtlasPlan_SampleDetail(x, y, &cold);
    AtlasPlan_ResetSample();
    AtlasPlan_SampleDetail(oldX, oldY, &discarded);
    AtlasPlan_SampleDetail(x, y, &warm);
    Equal(&cold, &warm);
}
int main(void)
{
    static const s32 edgeX[] = {0, 1, 2, ATLAS_WORLD_WIDTH / 2,
        ATLAS_WORLD_WIDTH - 3, ATLAS_WORLD_WIDTH - 2, ATLAS_WORLD_WIDTH - 1};
    static const s32 edgeY[] = {-ATLAS_WORLD_HALF_HEIGHT,
        -ATLAS_WORLD_HALF_HEIGHT + 1, -1, 0, 1,
        ATLAS_WORLD_HALF_HEIGHT - 2, ATLAS_WORLD_HALF_HEIGHT - 1};
    static const u32 seeds[] = {0u, 1u, 0x41544C33u, 0xFFFFFFFFu};
    u32 s, i, j, checks = 0;
    for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++)
    {
        FillPlan(seeds[s]);
        for (i = 0; i < sizeof(edgeX) / sizeof(edgeX[0]); i++)
            for (j = 0; j < sizeof(edgeY) / sizeof(edgeY[0]); j++)
            {
                CompareWarmCold(edgeX[i], edgeY[j], edgeX[i], edgeY[j]); checks++;
                CompareWarmCold(WrapX((s64)edgeX[i] - 1), ClampY(edgeY[j] - 1), edgeX[i], edgeY[j]); checks++;
                CompareWarmCold(WrapX((s64)edgeX[i] + 1), ClampY(edgeY[j] + 1), edgeX[i], edgeY[j]); checks++;
            }
        sRand = seeds[s] ^ 0x9E3779B9u;
        if (!sRand) sRand = 1;
        for (i = 0; i < 25000u; i++)
        {
            s32 x = (s32)(Next() % ATLAS_WORLD_WIDTH);
            s32 y = (s32)(Next() % (2u * ATLAS_WORLD_HALF_HEIGHT)) - ATLAS_WORLD_HALF_HEIGHT;
            s32 oldX, oldY;
            if ((i & 3u) == 0u) { oldX = WrapX((s64)x + (s32)(Next() % 5u) - 2); oldY = ClampY(y + (s32)(Next() % 5u) - 2); }
            else if ((i & 3u) == 1u) { oldX = WrapX((s64)x + ATLAS_WORLD_WIDTH / 2); oldY = ClampY(-y); }
            else if ((i & 3u) == 2u) { oldX = 0; oldY = -ATLAS_WORLD_HALF_HEIGHT; }
            else { oldX = ATLAS_WORLD_WIDTH - 1; oldY = ATLAS_WORLD_HALF_HEIGHT - 1; }
            CompareWarmCold(oldX, oldY, x, y);
            checks++;
        }
    }
    printf("PASS actual native sampler warm/cold equality: %u coordinate pairs, four synthetic plan fixtures\n", checks);
    return 0;
}
