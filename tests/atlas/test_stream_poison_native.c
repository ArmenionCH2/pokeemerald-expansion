/* Direct-native AtlasStream_Tick head-of-line failure regression.
 * Synthetic prewarm stub only: not a real-water, ROM or emulator test. */
#include "global.h"
#include "atlas_world.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/atlas_stream.c"

u16 AtlasWorld_Generation(void){return ATLAS_GENERATION_VERSION;}

static u16 map[35 * 34];
static unsigned row_descriptions[SIDE];
static unsigned descriptions, attempts, failed;
static s32 poisoned_x, poisoned_y;
static bool8 tracking, blocked;

u16 AtlasTiles_Describe(s32 x, s32 y, u16 *floor)
{
    assert(floor != NULL);
    descriptions++;
    if (tracking)
    {
        s32 row = y - sY;
        assert(x == poisoned_x);
        assert(row >= 0 && row < SIDE);
        assert(row_descriptions[row]++ == 0);
    }
    *floor = 0x0101;
    return 0; /* Flat floor: no neighbor stencil needed. */
}

u16 AtlasTiles_Render(u16 floor, const u16 *cells)
{
    (void)cells;
    return floor;
}

bool8 AtlasWater_Prewarm(s32 x, s32 y)
{
    assert(Index(x, y) >= 0);
    assert(x == poisoned_x);
    attempts++;
    if (blocked && x == poisoned_x && y == poisoned_y)
    {
        failed++;
        return FALSE;
    }
    return TRUE;
}

int main(void)
{
    unsigned i, j, before;
    s32 old_x, old_y;
    AtlasStream_Reset();
    assert(AtlasStream_Load(map, 50, 50));
    assert(descriptions == SIDE * SIDE);
    assert(gAtlasCacheMisses == 1);
    old_x = sX;
    old_y = sY;
    poisoned_x = old_x + SIDE;
    poisoned_y = old_y;
    tracking = TRUE;
    blocked = TRUE;

    /* One eastward tile exposes a 49-cell column. Only its first cell fails. */
    for (i = 0; i < 128; i++)
    {
        before = attempts;
        AtlasStream_Tick(50, 50, 11, 10);
        assert(attempts - before == 8);
        assert(attempts == (i + 1) * 8);
        assert(failed == attempts);
        assert(descriptions == SIDE * SIDE);
        assert(sCenterX == old_x + HALF + 1);
        assert(sCenterY == old_y + HALF);
        assert(sX == old_x + 1 && sY == old_y);
        assert(sLeft == sX && sRight == poisoned_x);
        assert(sTop == sY && sBottom == sY + SIDE);
        assert(sEdge == 1 && sAt == 0);
        assert(!(sEntries[Index(poisoned_x, poisoned_y)] & SAMPLED));
    }
    assert(attempts == 1024 && failed == 1024);

    blocked = FALSE;
    for (i = 0; i < 7; i++)
    {
        before = attempts;
        AtlasStream_Tick(50, 50, 11, 10);
        assert(attempts - before <= 8);
    }
    assert(attempts == 1024 + SIDE);
    assert(failed == 1024);
    assert(descriptions == SIDE * SIDE + SIDE);
    assert(sLeft == sX && sRight == sX + SIDE);
    assert(sTop == sY && sBottom == sY + SIDE);
    assert(sEdge == 4 && sAt == 0);
    for (j = 0; j < SIDE; j++)
    {
        assert(row_descriptions[j] == 1);
        assert(sEntries[Index(poisoned_x, old_y + (s32)j)] & SAMPLED);
    }
    for (j = 1; j < SIDE - 1; j++)
        assert(IsReady(poisoned_x - 1, old_y + (s32)j));

    for (i = 0; i < 16; i++)
        AtlasStream_Tick(50, 50, 11, 10);
    assert(attempts == 1024 + SIDE);
    assert(descriptions == SIDE * SIDE + SIDE);
    assert(gAtlasStreamWaits == 0);
    printf("PASS: 128 stalled ticks, 1024 failed prewarms, "
           "49 unique samples after 7 recovery ticks, 16 idle ticks\n");
    return 0;
}
