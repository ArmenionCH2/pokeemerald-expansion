#include "global.h"
#include "atlas_pokemon.h"
#include "constants/event_objects.h"
#include "constants/species.h"

/* Graphics here are genuine Emerald object-event graphics, NOT battle icons.
 * Restrict wild scenery to species whose existing sprites fit one map tile
 * (Poochyena is 32x32). Do not promote unmapped species to lookalikes. */
#define ATLAS_POKEMON_SPAWN_PER_MILLE 10

static u32 Mix(u32 value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    return value ^ (value >> 16);
}

static u32 PositionHash(s32 x, s32 y, u32 seed)
{
    x %= ATLAS_WORLD_WIDTH;
    if (x < 0)
        x += ATLAS_WORLD_WIDTH;
    return Mix(seed ^ (u32)x * 0x9E3779B1u ^ (u32)y * 0x85EBCA77u);
}

bool8 AtlasPokemon_IsCandidate(s32 x, s32 y, u32 seed)
{
    /* Latitude is bounded; longitude remains cyclic. Do not propose
     * scenery actors beyond either pole, even with a synthetic cell. */
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
        return FALSE;
    return Mix(PositionHash(x, y, seed) ^ 0xA341316Cu) % 1000 < ATLAS_POKEMON_SPAWN_PER_MILLE;
}

bool8 AtlasPokemon_Select(s32 x, s32 y, u32 seed, const struct AtlasCell *cell, struct AtlasPokemonSpawn *out)
{
    u32 hash, roll;
    u8 rarity;
    u16 species = SPECIES_NONE;
    u8 graphicsId = 0;

    if (out == NULL)
        return FALSE;
    out->species = SPECIES_NONE;
    out->graphicsId = 0;
    out->rarity = ATLAS_POKEMON_COMMON;
    if (cell == NULL || y < -ATLAS_WORLD_HALF_HEIGHT
     || y >= ATLAS_WORLD_HALF_HEIGHT || cell->biome > ATLAS_WETLAND
     || cell->waterform > ATLAS_LAKE || cell->landform > ATLAS_COAST
     || cell->waterform != ATLAS_DRY)
        return FALSE;

    hash = PositionHash(x, y, seed);
    if (Mix(hash ^ 0xA341316Cu) % 1000 >= ATLAS_POKEMON_SPAWN_PER_MILLE)
        return FALSE;
    roll = Mix(hash ^ 0xC8013EA4u) % 1000;
    rarity = roll < 700 ? ATLAS_POKEMON_COMMON
           : roll < 920 ? ATLAS_POKEMON_UNCOMMON
           : roll < 990 ? ATLAS_POKEMON_RARE : ATLAS_POKEMON_VERY_RARE;

    /* Roster is intentionally limited to verified native overworld art.
     * Don't represent tropical creatures in arctic tundra or place aquatic
     * species on dry ground. The final biome taxonomy is still research. */
    if (cell->temperature < 75 || cell->elevation > 195)
        return FALSE;
    if (cell->landform == ATLAS_COAST)
    {
        if (rarity == ATLAS_POKEMON_COMMON)
            species = SPECIES_WINGULL, graphicsId = OBJ_EVENT_GFX_WINGULL;
        else if (rarity == ATLAS_POKEMON_UNCOMMON && cell->moisture >= 105)
            species = SPECIES_AZURILL, graphicsId = OBJ_EVENT_GFX_AZURILL;
    }
    else if (cell->biome == ATLAS_DESERT || cell->moisture < 70)
    {
        /* Unmapped desert species: no misleading substitutes. */
        return FALSE;
    }
    else if (cell->biome == ATLAS_WETLAND || cell->moisture >= 180)
    {
        if (rarity == ATLAS_POKEMON_COMMON)
            species = SPECIES_AZURILL, graphicsId = OBJ_EVENT_GFX_AZURILL;
        else if (rarity == ATLAS_POKEMON_UNCOMMON)
            species = SPECIES_AZUMARILL, graphicsId = OBJ_EVENT_GFX_AZUMARILL;
        else if (rarity == ATLAS_POKEMON_RARE && cell->temperature >= 135)
            species = SPECIES_KECLEON, graphicsId = OBJ_EVENT_GFX_KECLEON;
    }
    else if (cell->biome == ATLAS_JUNGLE || cell->biome == ATLAS_FOREST)
    {
        if (rarity == ATLAS_POKEMON_COMMON)
            species = SPECIES_ZIGZAGOON, graphicsId = OBJ_EVENT_GFX_ZIGZAGOON_2;
        else if (rarity == ATLAS_POKEMON_UNCOMMON)
            species = SPECIES_SKITTY, graphicsId = OBJ_EVENT_GFX_SKITTY;
        else if (rarity == ATLAS_POKEMON_RARE && cell->temperature >= 140)
            species = SPECIES_KECLEON, graphicsId = OBJ_EVENT_GFX_KECLEON;
        else if (rarity == ATLAS_POKEMON_VERY_RARE && cell->temperature < 185)
            species = SPECIES_PIKACHU, graphicsId = OBJ_EVENT_GFX_PIKACHU;
    }
    else if (cell->biome == ATLAS_SAVANNA || cell->temperature > 190)
    {
        if (rarity == ATLAS_POKEMON_COMMON)
            species = SPECIES_POOCHYENA, graphicsId = OBJ_EVENT_GFX_POOCHYENA;
        else if (rarity == ATLAS_POKEMON_UNCOMMON)
            species = SPECIES_ZIGZAGOON, graphicsId = OBJ_EVENT_GFX_ZIGZAGOON_2;
        else if (rarity == ATLAS_POKEMON_RARE)
            species = SPECIES_SKITTY, graphicsId = OBJ_EVENT_GFX_SKITTY;
    }
    else
    {
        if (rarity == ATLAS_POKEMON_COMMON)
            species = SPECIES_ZIGZAGOON, graphicsId = OBJ_EVENT_GFX_ZIGZAGOON_2;
        else if (rarity == ATLAS_POKEMON_UNCOMMON)
            species = SPECIES_POOCHYENA, graphicsId = OBJ_EVENT_GFX_POOCHYENA;
        else if (rarity == ATLAS_POKEMON_RARE)
            species = SPECIES_SKITTY, graphicsId = OBJ_EVENT_GFX_SKITTY;
        else if (rarity == ATLAS_POKEMON_VERY_RARE && cell->moisture >= 110)
            species = SPECIES_PIKACHU, graphicsId = OBJ_EVENT_GFX_PIKACHU;
    }
    if (species == SPECIES_NONE)
        return FALSE;
    out->species = species;
    out->graphicsId = graphicsId;
    out->rarity = rarity;
    return TRUE;
}
