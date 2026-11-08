#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "global.h"
#include "atlas_actor_identity.h"

static struct AtlasActorSpawnKey Key(u32 seed, s32 x, s32 y, u16 species, u16 version)
{
    struct AtlasActorSpawnKey key;
    assert(AtlasActor_MakeSpawnKey(&key, seed, x, y, species, version));
    return key;
}

static void CheckBirthplaceAndDateline(void)
{
    struct AtlasActorSpawnKey a = Key(123, -1, -20, 25, ATLAS_GENERATION_VERSION);
    struct AtlasActorSpawnKey b = Key(123, ATLAS_WORLD_WIDTH - 1, -20, 25, ATLAS_GENERATION_VERSION);
    struct AtlasActorSpawnKey c = Key(123, 2 * ATLAS_WORLD_WIDTH - 1, -20, 25, ATLAS_GENERATION_VERSION);
    struct AtlasActorSpawnKey invalid = a;
    assert(a.worldX == ATLAS_WORLD_WIDTH - 1 && a.worldY == -20);
    assert(AtlasActor_SpawnKeyEquals(&a, &b));
    assert(AtlasActor_SpawnKeyEquals(&a, &c));
    assert(AtlasActor_SpawnKeyEquals(&a, &a));
    assert(!AtlasActor_SpawnKeyEquals(NULL, &a));
    assert(!AtlasActor_SpawnKeyEquals(&a, NULL));
    assert(!AtlasActor_MakeSpawnKey(NULL, 1, 1, 1, 1, 1));
    assert(!AtlasActor_MakeSpawnKey(&invalid, 123, 1, 1, 0, 3));
    assert(invalid.species == 0 && !AtlasActor_SpawnKeyEquals(&invalid, &invalid));
    assert(!AtlasActor_MakeSpawnKey(&invalid, 123, 1, 1, 25, 0));
    assert(invalid.generationVersion == 0);
}

static void CheckExtremeCoordsAndIsolation(void)
{
    const s32 extremes[] = {INT_MIN, INT_MIN + 1, -ATLAS_WORLD_WIDTH, -1, 0,
                            ATLAS_WORLD_WIDTH, INT_MAX - 1, INT_MAX};
    unsigned int i;
    for (i = 0; i < sizeof(extremes) / sizeof(extremes[0]); i++)
    {
        struct AtlasActorSpawnKey key = Key(123, extremes[i], -ATLAS_WORLD_HALF_HEIGHT, 25, 3);
        s32 expected = (s32)((((long long)extremes[i] % ATLAS_WORLD_WIDTH)
                             + ATLAS_WORLD_WIDTH) % ATLAS_WORLD_WIDTH);
        assert(key.worldX == expected);
        assert(key.worldY == -ATLAS_WORLD_HALF_HEIGHT);
    }
    {
        const s32 invalidY[] = {INT_MIN, -ATLAS_WORLD_HALF_HEIGHT - 1,
                                ATLAS_WORLD_HALF_HEIGHT, INT_MAX};
        struct AtlasActorSpawnKey key = {1, 1, 1, 1, 1};
        for (i = 0; i < sizeof(invalidY) / sizeof(invalidY[0]); i++)
        {
            assert(!AtlasActor_MakeSpawnKey(&key, 123, extremes[i], invalidY[i], 25, 3));
            assert(key.worldSeed == 0 && key.worldX == 0 && key.worldY == 0);
            assert(key.species == 0 && key.generationVersion == 0);
        }
    }
}

static void CheckIdentityFields(void)
{
    struct AtlasActorSpawnKey origin = Key(123, 8, -1, 25, 3);
    struct AtlasActorSpawnKey a = Key(124, 8, -1, 25, 3);
    struct AtlasActorSpawnKey b = Key(123, 8, -2, 25, 3);
    struct AtlasActorSpawnKey c = Key(123, 8, -1, 26, 3);
    struct AtlasActorSpawnKey d = Key(123, 8, -1, 25, 4);
    assert(!AtlasActor_SpawnKeyEquals(&origin, &a));
    assert(!AtlasActor_SpawnKeyEquals(&origin, &b));
    assert(!AtlasActor_SpawnKeyEquals(&origin, &c));
    assert(!AtlasActor_SpawnKeyEquals(&origin, &d));
}

static void CheckSlotReuse(void)
{
    struct AtlasActorHandle old = {1, 0};
    struct AtlasActorHandle live = old;
    struct AtlasActorHandle other = {1, 1};
    assert(AtlasActor_HandleMatches(old, live, 3));
    assert(!AtlasActor_HandleMatches(old, other, 3));
    assert(!AtlasActor_HandleMatches(old, live, 0));
    live.generation = AtlasActor_NextGeneration(live.generation);
    assert(live.generation == 2);
    assert(!AtlasActor_HandleMatches(old, live, 3));
    assert(AtlasActor_HandleMatches(live, live, 3));
    assert(AtlasActor_NextGeneration(0) == 1);
    assert(AtlasActor_NextGeneration(0xFFFFFFFFu) == 1);
    live.generation = 0;
    assert(!AtlasActor_HandleMatches(live, live, 3));
    old.slot = 3;
    assert(!AtlasActor_HandleMatches(old, old, 3));
}

static void CheckChunkSpawnBinding(void)
{
    const s32 chunks = ATLAS_WORLD_WIDTH / ATLAS_CHUNK_SIZE;
    const s32 aliases[] = {-1, chunks - 1, 2 * chunks - 1, -chunks - 1, INT_MIN, INT_MAX};
    struct AtlasActorSpawnKey key, expected;
    unsigned int i;

    /* An object in local tile (19,19) of chunk (-1,-1) is born at
     * longitude width-1 and latitude -1, not at a negative longitude. */
    expected = Key(123, ATLAS_WORLD_WIDTH - 1, -1, 25, ATLAS_GENERATION_VERSION);
    for (i = 0; i < 4; i++)
    {
        assert(AtlasActor_MakeChunkSpawnKey(&key, 123, aliases[i], -1,
                                            19, 19, 25, ATLAS_GENERATION_VERSION));
        assert(AtlasActor_SpawnKeyEquals(&key, &expected));
    }
    /* Extreme chunk X values must normalize before multiplication. */
    for (i = 4; i < sizeof(aliases) / sizeof(aliases[0]); i++)
    {
        s32 canonical = aliases[i] % chunks;
        if (canonical < 0)
            canonical += chunks;
        assert(AtlasActor_MakeChunkSpawnKey(&key, 123, aliases[i], -2,
                                            0, 0, 25, ATLAS_GENERATION_VERSION));
        assert(key.worldX == canonical * ATLAS_CHUNK_SIZE && key.worldY == -40);
    }
    /* Reject polar-invalid birthplaces, even when representable as s32. */
    assert(AtlasActor_MakeChunkSpawnKey(&key, 123, -1,
                                        -ATLAS_WORLD_HALF_HEIGHT / 20,
                                        19, 0, 25, ATLAS_GENERATION_VERSION));
    assert(key.worldX == ATLAS_WORLD_WIDTH - 1 && key.worldY == -ATLAS_WORLD_HALF_HEIGHT);
    assert(AtlasActor_MakeChunkSpawnKey(&key, 123, 0,
                                        ATLAS_WORLD_HALF_HEIGHT / 20 - 1,
                                        0, 19, 25, ATLAS_GENERATION_VERSION));
    assert(key.worldY == ATLAS_WORLD_HALF_HEIGHT - 1);
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, -1,
                                         -ATLAS_WORLD_HALF_HEIGHT / 20 - 1,
                                         19, 19, 25, ATLAS_GENERATION_VERSION));
    assert(key.species == 0 && key.generationVersion == 0);
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0,
                                         ATLAS_WORLD_HALF_HEIGHT / 20,
                                         0, 0, 25, ATLAS_GENERATION_VERSION));
    assert(key.species == 0 && key.generationVersion == 0);
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0, INT_MAX / 20,
                                         0, 7, 25, ATLAS_GENERATION_VERSION));
    assert(key.species == 0);
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, -1, INT_MIN / 20 - 1,
                                         19, 12, 25, ATLAS_GENERATION_VERSION));
    assert(key.species == 0);
    /* Without the widened polar check, huge chunk Y can narrow into a
     * seemingly valid world latitude (INT_MAX*20+19 wraps to -1). */
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, -1, INT_MAX,
                                         19, 19, 25, ATLAS_GENERATION_VERSION));
    assert(key.species == 0);
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, -1, INT_MIN,
                                         19, 0, 25, ATLAS_GENERATION_VERSION));
    assert(key.species == 0);
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0, 0, -1, 0, 25, 3));
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0, 0, 20, 0, 25, 3));
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0, 0, 0, -1, 25, 3));
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0, 0, 0, 20, 25, 3));
    assert(!AtlasActor_MakeChunkSpawnKey(&key, 123, 0, 0, 0, 0, 0, 3));
    assert(key.species == 0 && key.worldX == 0 && key.worldY == 0);
    assert(!AtlasActor_MakeChunkSpawnKey(NULL, 123, 0, 0, 0, 0, 25, 3));
}

/* Mirror the three fixed native wildlife slots: invalidating a roster
 * must invalidate every previously issued handle, even if local IDs and
 * native sprite slots are reused on the next chunk. */
static void CheckThreeSlotRosterReplacements(void)
{
    u32 generation[3] = {0, 0, 0};
    struct AtlasActorHandle old[3];
    struct AtlasActorHandle current[3];
    unsigned epoch, slot, other;

    for (slot = 0; slot < 3; slot++)
    {
        old[slot].slot = slot;
        old[slot].generation = 0;
    }
    for (epoch = 0; epoch < 128; epoch++)
    {
        for (slot = 0; slot < 3; slot++)
        {
            generation[slot] = AtlasActor_NextGeneration(generation[slot]);
            current[slot].slot = slot;
            current[slot].generation = generation[slot];
            assert(AtlasActor_HandleMatches(current[slot], current[slot], 3));
            assert(!AtlasActor_HandleMatches(old[slot], current[slot], 3));
            for (other = 0; other < 3; other++)
                if (other != slot)
                    assert(!AtlasActor_HandleMatches(current[slot],
                                                     (struct AtlasActorHandle){generation[slot], other}, 3));
            old[slot] = current[slot];
        }
    }
    /* Native generations never issue zero, including at u32 rollover. */
    assert(AtlasActor_NextGeneration(0xFFFFFFFFu) == 1);
}

/* Cross-product of the extreme signed chunk coordinates with interior tile
 * offsets. The latitude oracle uses 64-bit arithmetic so that a future
 * 32-bit overflow cannot make a rejected birthplace silently alias another
 * actor. No production allocation or world scan is involved. */
static void CheckChunkCoordinateCartesianBoundaries(void)
{
    const s32 wrapChunks = ATLAS_WORLD_WIDTH / ATLAS_CHUNK_SIZE;
    const s32 chunkXs[] = {INT_MIN, INT_MIN + 1, -wrapChunks - 1, -1,
                          0, 1, wrapChunks - 1, INT_MAX - 1, INT_MAX};
    const s32 chunkYs[] = {INT_MIN, INT_MIN / 20 - 1, INT_MIN / 20,
                          -1, 0, 1, INT_MAX / 20, INT_MAX / 20 + 1, INT_MAX};
    const s32 locals[] = {0, 11, 12, 19};
    unsigned int x, y, lx, ly, checked = 0, rejected = 0;

    for (x = 0; x < sizeof(chunkXs) / sizeof(chunkXs[0]); x++)
    for (y = 0; y < sizeof(chunkYs) / sizeof(chunkYs[0]); y++)
    for (lx = 0; lx < sizeof(locals) / sizeof(locals[0]); lx++)
    for (ly = 0; ly < sizeof(locals) / sizeof(locals[0]); ly++)
    {
        const s64 latitude = (s64)chunkYs[y] * ATLAS_CHUNK_SIZE + locals[ly];
        s32 canonicalChunkX = chunkXs[x] % wrapChunks;
        struct AtlasActorSpawnKey key = {9, 9, 9, 9, 9};
        struct AtlasActorSpawnKey direct;
        bool8 ok;
        if (canonicalChunkX < 0)
            canonicalChunkX += wrapChunks;
        ok = AtlasActor_MakeChunkSpawnKey(&key, 0, chunkXs[x], chunkYs[y],
                                          locals[lx], locals[ly], 25, 3);
        assert(!!ok == (latitude >= -ATLAS_WORLD_HALF_HEIGHT
                     && latitude < ATLAS_WORLD_HALF_HEIGHT));
        if (ok)
        {
            s32 expectedX = canonicalChunkX * ATLAS_CHUNK_SIZE + locals[lx];
            assert(key.worldSeed == 0 && key.worldX == expectedX);
            assert(key.worldY == (s32)latitude);
            assert(key.species == 25 && key.generationVersion == 3);
            assert(AtlasActor_MakeSpawnKey(&direct, 0, expectedX, (s32)latitude, 25, 3));
            assert(AtlasActor_SpawnKeyEquals(&key, &direct));
        }
        else
        {
            assert(key.worldSeed == 0 && key.worldX == 0 && key.worldY == 0);
            assert(key.species == 0 && key.generationVersion == 0);
            rejected++;
        }
        checked++;
    }
    assert(checked == 1296 && rejected > 0 && rejected < checked);
    printf("PASS %u native actor chunk-coordinate combinations (%u rejected out-of-range)\n",
           checked, rejected);
}

/* Every birthplace in a 3x3 neighborhood straddling longitude zero and
 * signed latitude zero must be unique. Reconstructing from an aliased chunk
 * or directly from world tiles must reproduce the exact same identity. */
static void CheckSeamNeighborhoodSpawnKeys(void)
{
    const s32 chunkXs[] = {-1, 0, 1};
    const s32 chunkYs[] = {-2, -1, 0};
    const s32 locals[] = {0, 1, 18, 19};
    const u32 seeds[] = {0, 0xC0FFEEu};
    struct AtlasActorSpawnKey keys[288];
    unsigned seed, x, y, lx, ly, previous, checked = 0;

    for (seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); seed++)
    for (x = 0; x < sizeof(chunkXs) / sizeof(chunkXs[0]); x++)
    for (y = 0; y < sizeof(chunkYs) / sizeof(chunkYs[0]); y++)
    for (lx = 0; lx < sizeof(locals) / sizeof(locals[0]); lx++)
    for (ly = 0; ly < sizeof(locals) / sizeof(locals[0]); ly++)
    {
        struct AtlasActorSpawnKey wrapped, direct;
        assert(checked < sizeof(keys) / sizeof(keys[0]));
        assert(AtlasActor_MakeChunkSpawnKey(&keys[checked], seeds[seed],
                                            chunkXs[x], chunkYs[y], locals[lx], locals[ly],
                                            25, ATLAS_GENERATION_VERSION));
        assert(AtlasActor_MakeChunkSpawnKey(&wrapped, seeds[seed],
                                            chunkXs[x] + ATLAS_WORLD_WIDTH / ATLAS_CHUNK_SIZE,
                                            chunkYs[y], locals[lx], locals[ly],
                                            25, ATLAS_GENERATION_VERSION));
        assert(AtlasActor_MakeSpawnKey(&direct, seeds[seed],
                                       chunkXs[x] * ATLAS_CHUNK_SIZE + locals[lx],
                                       chunkYs[y] * ATLAS_CHUNK_SIZE + locals[ly],
                                       25, ATLAS_GENERATION_VERSION));
        assert(AtlasActor_SpawnKeyEquals(&keys[checked], &wrapped));
        assert(AtlasActor_SpawnKeyEquals(&keys[checked], &direct));
        for (previous = 0; previous < checked; previous++)
            assert(!AtlasActor_SpawnKeyEquals(&keys[previous], &keys[checked]));
        checked++;
    }
    assert(checked == sizeof(keys) / sizeof(keys[0]));
    printf("PASS: %u distinct native birthplace keys across dateline and negative latitude, with canonical replay\n", checked);
}

int main(void)
{
    CheckBirthplaceAndDateline();
    CheckExtremeCoordsAndIsolation();
    CheckIdentityFields();
    CheckSlotReuse();
    CheckChunkSpawnBinding();
    CheckThreeSlotRosterReplacements();
    CheckChunkCoordinateCartesianBoundaries();
    CheckSeamNeighborhoodSpawnKeys();
    puts("PASS Atlas actor birthplace identity and generation-checked transient handles");
    return 0;
}
