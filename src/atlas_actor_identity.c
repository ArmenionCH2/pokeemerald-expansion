#include "global.h"
#include "atlas_actor_identity.h"

bool8 AtlasActor_MakeSpawnKey(struct AtlasActorSpawnKey *out, u32 seed,
                             s32 worldX, s32 worldY, u16 species, u16 version)
{
    if (out == NULL)
        return FALSE;

    /* Do not leave a previous valid key in a reused output on failure. */
    out->worldSeed = 0;
    out->worldX = 0;
    out->worldY = 0;
    out->species = 0;
    out->generationVersion = 0;
    if (species == 0 || version == 0
     || worldY < -ATLAS_WORLD_HALF_HEIGHT || worldY >= ATLAS_WORLD_HALF_HEIGHT)
        return FALSE;

    /* Reduce before adding: even INT_MIN and INT_MAX cannot overflow. */
    worldX %= ATLAS_WORLD_WIDTH;
    if (worldX < 0)
        worldX += ATLAS_WORLD_WIDTH;
    out->worldSeed = seed;
    out->worldX = worldX;
    out->worldY = worldY;
    out->species = species;
    out->generationVersion = version;
    return TRUE;
}

bool8 AtlasActor_MakeChunkSpawnKey(struct AtlasActorSpawnKey *out, u32 seed,
                                  s32 chunkX, s32 chunkY, s32 localX, s32 localY,
                                  u16 species, u16 version)
{
    s64 worldY;
    s32 worldX;
    if (out == NULL)
        return FALSE;
    if (localX < 0 || localX >= ATLAS_CHUNK_SIZE
     || localY < 0 || localY >= ATLAS_CHUNK_SIZE)
        return AtlasActor_MakeSpawnKey(out, seed, 0, 0, 0, 0);

    /* Chunk X may be a negative or multi-wrap alias. Multiplying before
     * canonicalization can overflow even when the tile itself is valid. */
    chunkX %= ATLAS_WORLD_WIDTH / ATLAS_CHUNK_SIZE;
    if (chunkX < 0)
        chunkX += ATLAS_WORLD_WIDTH / ATLAS_CHUNK_SIZE;
    worldX = chunkX * ATLAS_CHUNK_SIZE + localX;

    /* Signed latitude has no wrap; reject out-of-world birthplaces rather
     * than creating an identity the actor roster must later refuse. */
    worldY = (s64)chunkY * ATLAS_CHUNK_SIZE + localY;
    if (worldY < -ATLAS_WORLD_HALF_HEIGHT || worldY >= ATLAS_WORLD_HALF_HEIGHT)
        return AtlasActor_MakeSpawnKey(out, seed, 0, 0, 0, 0);
    return AtlasActor_MakeSpawnKey(out, seed, worldX, (s32)worldY, species, version);
}

bool8 AtlasActor_SpawnKeyEquals(const struct AtlasActorSpawnKey *a,
                               const struct AtlasActorSpawnKey *b)
{
    return a != NULL && b != NULL
        && a->species != 0 && b->species != 0
        && a->generationVersion != 0 && b->generationVersion != 0
        && a->worldSeed == b->worldSeed
        && a->worldX == b->worldX
        && a->worldY == b->worldY
        && a->species == b->species
        && a->generationVersion == b->generationVersion;
}

u32 AtlasActor_NextGeneration(u32 current)
{
    current++;
    return current == 0 ? 1 : current;
}

bool8 AtlasActor_HandleMatches(struct AtlasActorHandle reference,
                               struct AtlasActorHandle current, u16 capacity)
{
    return reference.generation != 0 && current.generation != 0
        && reference.slot < capacity && current.slot < capacity
        && reference.slot == current.slot
        && reference.generation == current.generation;
}
