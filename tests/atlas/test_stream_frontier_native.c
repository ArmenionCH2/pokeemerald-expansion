/* Direct-native host regression for stream frontier recovery after prewarm
 * failure. Compiles the actual atlas_stream.c, not a copied Job/Advance body.
 * This proves only the bounded host/mock contract, not ROM or GBA behavior. */
#include "global.h"
#include "atlas_world.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/atlas_stream.c"

static u16 map[35 * 34];
static unsigned char described[SIDE * SIDE];
static unsigned long long total_new, total_failures;
static unsigned attempts, failures, successful, descriptions;
static bool8 tracking;

u16 AtlasTiles_Describe(s32 x, s32 y, u16 *floor)
{
    assert(floor != NULL);
    descriptions++;
    if (tracking)
    {
        s32 dx = x - sX, dy = y - sY;
        assert(dx >= 0 && dx < SIDE && dy >= 0 && dy < SIDE);
        assert(described[dy * SIDE + dx] == 0);
        described[dy * SIDE + dx]++;
        successful++;
    }
    *floor = 0x0101;
    return 0; /* Flat floor avoids semantic-neighbor render dependency. */
}
u16 AtlasTiles_Render(u16 floor, const u16 *cells)
{
    (void)cells;
    return floor;
}
bool8 AtlasWater_Prewarm(s32 x, s32 y)
{
    assert(Index(x, y) >= 0);
    attempts++;
    if (attempts <= 8)
    {
        failures++;
        return FALSE;
    }
    return TRUE;
}

static unsigned rng = 0x4eeb3719u;
static unsigned Next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void Check(s32 dx, s32 dy)
{
    s32 oldX, oldY, targetX, targetY, chunkX, chunkY;
    s32 left, right, top, bottom, x, y;
    unsigned expected, frames = 0, initial_descriptions;
    assert(dx >= -40 && dx <= 40 && dy >= -40 && dy <= 40);
    memset(described, 0, sizeof(described));
    tracking = FALSE;
    attempts = failures = successful = descriptions = 0;
    AtlasStream_Reset();
    assert(AtlasStream_Load(map, 50, 50));
    assert(descriptions == SIDE * SIDE);
    assert(gAtlasCacheMisses == 1);
    oldX = sX;
    oldY = sY;
    targetX = sCenterX + dx;
    targetY = sCenterY + dy;
    chunkX = targetX / 20;
    chunkY = targetY / 20;
    tracking = TRUE;
    AtlasStream_Tick(chunkX, chunkY, targetX % 20, targetY % 20);
    assert(sCenterX == targetX && sCenterY == targetY);
    assert(sX == targetX - HALF && sY == targetY - HALF);
    left = sX > oldX ? sX : oldX;
    right = sX + SIDE < oldX + SIDE ? sX + SIDE : oldX + SIDE;
    top = sY > oldY ? sY : oldY;
    bottom = sY + SIDE < oldY + SIDE ? sY + SIDE : oldY + SIDE;
    assert(right > left && bottom > top);
    expected = SIDE * SIDE - (unsigned)((right - left) * (bottom - top));
    assert(sLeft == left && sRight == right && sTop == top && sBottom == bottom);
    if (expected)
    {
        /* First eight prewarm attempts must fail without consuming a job. */
        assert(attempts == 8 && failures == 8 && successful == 0);
        assert(sAt == 0 && sEdge != 4);
    }
    else
        assert(attempts == 0 && failures == 0 && successful == 0);
    initial_descriptions = descriptions;
    while (sLeft != sX || sRight != sX + SIDE ||
           sTop != sY || sBottom != sY + SIDE || sEdge != 4)
    {
        assert(frames++ < SIDE * SIDE);
        AtlasStream_Tick(chunkX, chunkY, targetX % 20, targetY % 20);
    }
    assert(successful == expected);
    assert(descriptions == initial_descriptions + expected);
    assert(attempts == expected + (expected ? 8u : 0u));
    assert(failures == (expected ? 8u : 0u));
    assert(gAtlasStreamWaits == 0);
    for (y = 0; y < SIDE; y++)
        for (x = 0; x < SIDE; x++)
        {
            s32 wx = sX + x, wy = sY + y;
            bool8 retained = wx >= oldX && wx < oldX + SIDE &&
                             wy >= oldY && wy < oldY + SIDE;
            assert(described[y * SIDE + x] == (retained ? 0 : 1));
            assert(sEntries[Index(wx, wy)] & SAMPLED);
            if (x > 0 && x < SIDE - 1 && y > 0 && y < SIDE - 1)
                assert(IsReady(wx, wy));
        }
    total_new += expected;
    total_failures += failures;
}

int main(void)
{
    unsigned i;
    Check(0, 0);
    Check(40, 40);
    Check(-40, -40);
    Check(40, -40);
    Check(-40, 40);
    for (i = 0; i < 400; i++)
        Check((s32)(Next() % 81) - 40, (s32)(Next() % 81) - 40);
    printf("PASS: 405 direct-native frontier cases, %llu uniquely sampled new cells, "
           "%llu bounded prewarm retries; all rings complete\n",
           total_new, total_failures);
    return 0;
}
