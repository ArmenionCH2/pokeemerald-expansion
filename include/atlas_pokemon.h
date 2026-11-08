#ifndef GUARD_ATLAS_POKEMON_H
#define GUARD_ATLAS_POKEMON_H

#include "atlas_world.h"

enum AtlasPokemonRarity
{
    ATLAS_POKEMON_COMMON,
    ATLAS_POKEMON_UNCOMMON,
    ATLAS_POKEMON_RARE,
    ATLAS_POKEMON_VERY_RARE
};

struct AtlasPokemonSpawn
{
    u16 species;
    u8 graphicsId;
    u8 rarity;
};

/* Fast deterministic prefilter: sample expensive geographic data only for
 * candidate coordinates. X wraps at Atlas's dateline. */
bool8 AtlasPokemon_IsCandidate(s32 x, s32 y, u32 seed);
bool8 AtlasPokemon_Select(s32 x, s32 y, u32 seed, const struct AtlasCell *cell, struct AtlasPokemonSpawn *out);

#endif
