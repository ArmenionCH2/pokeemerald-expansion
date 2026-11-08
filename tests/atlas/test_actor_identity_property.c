/* Deterministic full-width coordinate property test for the native Atlas
 * birthplace contract. Host-only: no ROM-side state or runtime allocation. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "global.h"
#include "atlas_actor_identity.h"

static u32 Next(u32 *state)
{
    u32 x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static s32 Signed32(u32 value)
{
    /* Avoid implementation-defined out-of-range unsigned-to-signed casts. */
    return (s32)(value <= INT_MAX ? (s64)value : (s64)value - 4294967296LL);
}

int main(void)
{
    const s64 width = ATLAS_WORLD_WIDTH;
    const s64 chunkCount = ATLAS_WORLD_WIDTH / ATLAS_CHUNK_SIZE;
    u32 state = 0x9E3779B9u;
    unsigned i, accepted = 0, rejected = 0;

    assert(width % ATLAS_CHUNK_SIZE == 0);
    for (i = 0; i < 8192; i++)
    {
        const s32 chunkX = Signed32(Next(&state));
        s32 chunkY = Signed32(Next(&state));
        const s32 localX = (s32)(Next(&state) % ATLAS_CHUNK_SIZE);
        const s32 localY = (s32)(Next(&state) % ATLAS_CHUNK_SIZE);
        const u32 seed = Next(&state);
        const u16 species = (u16)(1 + Next(&state) % 65535u);
        const u16 version = (u16)(1 + Next(&state) % 65535u);
        struct AtlasActorSpawnKey key = {1, 1, 1, 1, 1};
        struct AtlasActorSpawnKey alias, direct;
        s64 expectedX, expectedY;
        bool8 ok;

        /* Exercise legal planetary latitudes and malformed/out-of-world chunks. */
        if (i & 1)
            chunkY = Signed32(Next(&state) % 220000001u) - 110000000;
        expectedX = ((s64)chunkX * ATLAS_CHUNK_SIZE + localX) % width;
        if (expectedX < 0)
            expectedX += width;
        expectedY = (s64)chunkY * ATLAS_CHUNK_SIZE + localY;
        ok = AtlasActor_MakeChunkSpawnKey(&key, seed, chunkX, chunkY,
                                           localX, localY, species, version);
        if (expectedY < -ATLAS_WORLD_HALF_HEIGHT || expectedY >= ATLAS_WORLD_HALF_HEIGHT)
        {
            assert(!ok);
            assert(key.worldSeed == 0 && key.worldX == 0 && key.worldY == 0);
            assert(key.species == 0 && key.generationVersion == 0);
            rejected++;
            continue;
        }
        assert(ok);
        assert(key.worldSeed == seed && key.worldX == (s32)expectedX);
        assert(key.worldY == (s32)expectedY);
        assert(key.species == species && key.generationVersion == version);
        assert(AtlasActor_MakeSpawnKey(&direct, seed, (s32)expectedX,
                                       (s32)expectedY, species, version));
        assert(AtlasActor_SpawnKeyEquals(&key, &direct));

        /* Same birthplace via a different legal negative chunk alias. */
        assert(AtlasActor_MakeChunkSpawnKey(&alias, seed,
                   (s32)(expectedX / ATLAS_CHUNK_SIZE - chunkCount), chunkY,
                   localX, localY, species, version));
        assert(AtlasActor_SpawnKeyEquals(&key, &alias));
        accepted++;
    }
    assert(accepted > 0 && rejected > 0);
    assert(accepted + rejected == 8192);
    printf("PASS: 8192 native birthplace properties (%u accepted, %u rejected)\n",
           accepted, rejected);
    return 0;
}
