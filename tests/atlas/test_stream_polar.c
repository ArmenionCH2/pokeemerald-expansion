/* Host regression: compile the real native streamer with deterministic,
 * allocation-free geography mocks; inspect READY apron vs physical latitude. */
#include "global.h"
#include "atlas_world.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "../../src/atlas_stream.c"

static u16 map[35 * 34];
static unsigned descriptions, prewarms;

u16 AtlasTiles_Describe(s32 x, s32 y, u16 *floor)
{
    (void)x; (void)y;
    descriptions++;
    *floor = 0x0101;
    return 0; /* Flat floor: no semantic-neighbor dependency. */
}
u16 AtlasTiles_Render(u16 floor, const u16 *cells)
{
    (void)cells;
    return floor;
}
bool8 AtlasWater_Prewarm(s32 x, s32 y)
{
    (void)x; (void)y;
    prewarms++;
    return TRUE;
}

static void Settle(s32 chunkY, s32 localY)
{
    unsigned frame;
    AtlasStream_Reset();
    descriptions = prewarms = 0;
    assert(AtlasStream_Load(map, 0, chunkY));
    for (frame = 0; frame < 100; frame++)
        AtlasStream_Tick(0, chunkY, 10, localY);
    assert(sCenterY == chunkY * 20 + localY);
    assert(sCenterX == 10);
    assert(gAtlasCacheMisses == 1);
    assert(gAtlasStreamWaits == 0);
    assert(sEdge == 4); /* All scheduled frontiers settled. */
}

static void CheckPolar(s32 chunkY, s32 localY, u8 outward, u8 inward)
{
    s32 i, stripY;
    unsigned oldWaits;
    Settle(chunkY, localY);
    stripY = sCenterY + (outward == 1 ? 9 : -9);
    /* Prove the exact off-world strip that the old gate would accept is READY. */
    for (i = -10; i <= 10; i++)
        assert(IsReady(sCenterX + i, stripY));
    assert(stripY < -ATLAS_WORLD_HALF_HEIGHT || stripY >= ATLAS_WORLD_HALF_HEIGHT);
    oldWaits = gAtlasStreamWaits;
    assert(!AtlasStream_CanStep(outward));
    assert(gAtlasStreamWaits == oldWaits); /* Not a preparation stall. */
    assert(AtlasStream_CanStep(inward));
    assert(AtlasStream_CanStep(3));
    assert(AtlasStream_CanStep(4));
    assert(AtlasStream_CanStep(0));
    assert(gAtlasCacheMisses == 1);
}

/* Direct-native malformed saved chunk admission, not a ROM/save proof.
 * Before the Tick guard, extreme x/y can overflow or enter an unbounded
 * Shift loop. All rejected inputs must leave the warm ring untouched. */
static void CheckMalformedTick(void)
{
    s32 centerX, centerY, originX, originY;
    unsigned oldDescriptions, oldPrewarms;
    Settle(0, 10);
    centerX = sCenterX;
    centerY = sCenterY;
    originX = sX;
    originY = sY;
    oldDescriptions = descriptions;
    oldPrewarms = prewarms;

    AtlasStream_Tick(INT32_MAX, INT32_MAX, 10, 10);
    AtlasStream_Tick(INT32_MIN, INT32_MIN, 10, 10);
    AtlasStream_Tick(INT32_MAX, 0, 10, 10);
    AtlasStream_Tick(INT32_MIN, 0, 10, 10);
    AtlasStream_Tick(0, 0, INT32_MAX, 10);
    AtlasStream_Tick(0, 0, INT32_MIN, 10);
    AtlasStream_Tick(0, 0, 10, INT32_MAX);
    AtlasStream_Tick(0, 0, 10, INT32_MIN);
    AtlasStream_Tick(0, ATLAS_WORLD_HALF_HEIGHT / 20 - 1, 10, 10);
    AtlasStream_Tick(0, -ATLAS_WORLD_HALF_HEIGHT / 20, 10, 10);
    assert(sCenterX == centerX && sCenterY == centerY);
    assert(sX == originX && sY == originY);
    assert(descriptions == oldDescriptions && prewarms == oldPrewarms);
    assert(gAtlasCacheMisses == 1);

    /* Equivalent whole-world longitude laps remain harmless. */
    AtlasStream_Tick(ATLAS_WORLD_WIDTH / 20, 0, 10, 10);
    AtlasStream_Tick(-ATLAS_WORLD_WIDTH / 20, 0, 10, 10);
    assert(sCenterX == centerX && sCenterY == centerY);

    /* A legitimate nearby step must still advance the native ring. */
    AtlasStream_Tick(0, 0, 11, 10);
    assert(sCenterX == centerX + 1 && sCenterY == centerY);
    puts("PASS: native Tick rejects malformed/out-of-sync chunks without freezing valid steps");
}

int main(void)
{
    AtlasStream_Reset();
    assert(AtlasStream_CanStep(1)); /* Existing inactive behavior. */
    assert(AtlasStream_CanStep(2));
    CheckPolar(ATLAS_WORLD_HALF_HEIGHT / 20 - 1, 19, 1, 2);
    CheckPolar(-ATLAS_WORLD_HALF_HEIGHT / 20, 0, 2, 1);
    Settle(ATLAS_WORLD_HALF_HEIGHT / 20 - 1, 18);
    assert(AtlasStream_CanStep(1)); /* One tile inside the south pole. */
    Settle(-ATLAS_WORLD_HALF_HEIGHT / 20, 1);
    assert(AtlasStream_CanStep(2)); /* One tile inside the north pole. */
    Settle(0, 10);
    assert(AtlasStream_CanStep(1) && AtlasStream_CanStep(2));
    CheckMalformedTick();
    puts("PASS: native streamer refuses fully READY off-world polar apron while preserving inward/interior steps");
    return 0;
}
