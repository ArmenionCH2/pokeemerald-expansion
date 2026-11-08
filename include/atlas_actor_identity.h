#ifndef GUARD_ATLAS_ACTOR_IDENTITY_H
#define GUARD_ATLAS_ACTOR_IDENTITY_H

#include "atlas_world.h"

/* Immutable birthplace identity, independent of map-local event IDs, sprite
 * slots and the actor's current roaming position. This is a contract only:
 * no live actor table or persistent state is allocated by this module. */
struct AtlasActorSpawnKey
{
    u32 worldSeed;
    s32 worldX;
    s32 worldY;
    u16 species;
    u16 generationVersion;
};

/* A temporary pool reference. Increment the slot's generation whenever an
 * actor is removed/replaced, including streaming rebases. Never save handles. */
struct AtlasActorHandle
{
    u32 generation;
    u16 slot;
};

/* Birthplace latitude is bounded to the physical world. Invalid inputs
 * fail closed and clear any previously valid output; longitude wraps. */
bool8 AtlasActor_MakeSpawnKey(struct AtlasActorSpawnKey *out, u32 seed,
                             s32 worldX, s32 worldY, u16 species, u16 version);
/* Convert an existing 20x20 map object's local tile into its immutable
 * world birthplace. Normalize chunk longitude before multiplication, and
 * reject latitude outside the physical world without wrapping or truncation.
 * This is a cold-path helper, not a live actor allocation or field hook. */
bool8 AtlasActor_MakeChunkSpawnKey(struct AtlasActorSpawnKey *out, u32 seed,
                                  s32 chunkX, s32 chunkY, s32 localX, s32 localY,
                                  u16 species, u16 version);
bool8 AtlasActor_SpawnKeyEquals(const struct AtlasActorSpawnKey *a,
                               const struct AtlasActorSpawnKey *b);
u32 AtlasActor_NextGeneration(u32 current);
bool8 AtlasActor_HandleMatches(struct AtlasActorHandle reference,
                               struct AtlasActorHandle current, u16 capacity);

/* Field object-event binding is intentionally not declared until a native
 * owner and lifecycle are integrated and verified. */

#endif
