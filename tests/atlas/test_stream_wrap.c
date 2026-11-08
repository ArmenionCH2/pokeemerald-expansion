/* Include the implementation to inspect coordinate-only rebases without
 * exporting debug hooks or walking forty million tiles per world lap. */
#include "../../src/atlas_stream.c"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>

static u16 map[35 * 34], entries[SIDE * SIDE], semantics[SIDE * SIDE];

static void Check(s32 x, s32 y)
{
    s32 dx, dy, localX = x % 20, localY = y % 20;
    for (dy = -5; dy <= 6; dy++) for (dx = -7; dx <= 8; dx++)
        assert(map[(localY + 7 + dy) * 35 + localX + 7 + dx]
               == AtlasTerrain_Tile(x + dx, y + dy));
    assert(sCenterX == x && sCenterY == y);
    assert(sX == x - HALF && sMapX == x / 20 * 20 - 7);
}

static void Lap(int direction)
{
    unsigned i, frame;
    s32 x = direction > 0 ? ATLAS_WORLD_WIDTH - 10 : 10;
    s32 y = 212940;
    AtlasStream_Reset();
    assert(AtlasStream_Load(map, x / 20, y / 20));
    for (frame = 0; frame < 80; frame++)
        AtlasStream_Tick(x / 20, y / 20, x % 20, y % 20);
    for (i = 0; i < 30; i++)
    {
        assert(AtlasStream_CanStep(direction > 0 ? 4 : 3));
        x = (x + direction + ATLAS_WORLD_WIDTH) % ATLAS_WORLD_WIDTH;
        if (x % 20 == (direction > 0 ? 0 : 19))
            assert(AtlasStream_Load(map, x / 20, y / 20));
        for (frame = 0; frame < 8; frame++)
            AtlasStream_Tick(x / 20, y / 20, x % 20, y % 20);
        Check(x, y);
    }
    /* Equivalent periodic states after additional complete laps. Keep a
     * partially prepared edge so its cursor and ready flags must survive. */
    x = (x + direction + ATLAS_WORLD_WIDTH) % ATLAS_WORLD_WIDTH;
    assert(AtlasStream_Load(map, x / 20, y / 20));
    AtlasStream_Tick(x / 20, y / 20, x % 20, y % 20);
    assert(sEdge != 4);
    for (i = 0; i < 8; i++)
    {
        s32 offset = direction * ATLAS_WORLD_WIDTH;
        u8 edge = sEdge, px = sPX, py = sPY;
        u16 at = sAt;
        s32 left = sLeft, right = sRight;
        memcpy(entries, sEntries, sizeof(entries));
        memcpy(semantics, sSemantics, sizeof(semantics));
        sX += offset; sCenterX += offset; sMapX += offset;
        sLeft += offset; sRight += offset;
        /* Zero work is deliberately requested by calling the same helper
         * used by the field tick: a periodic rebase must not regenerate. */
        CanonicalizeX();
        assert(!memcmp(entries, sEntries, sizeof(entries)));
        assert(!memcmp(semantics, sSemantics, sizeof(semantics)));
        assert(edge == sEdge && at == sAt && px == sPX && py == sPY);
        assert(left == sLeft && right == sRight);
        Check(x, y);
    }
    for (frame = 0; frame < 80; frame++)
        AtlasStream_Tick(x / 20, y / 20, x % 20, y % 20);
    Check(x, y);
    assert(gAtlasCacheMisses == 1 && !gAtlasStreamWaits);
}

int main(void)
{
    assert(AtlasPlan_Begin(0)); while (!AtlasPlan_Step(256));
    Lap(1); Lap(-1);
    puts("PASS: repeated east/west world laps preserve tiles, partial jobs and warm cache without coordinate drift");
    return 0;
}
