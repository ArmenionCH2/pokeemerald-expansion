#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "global.h"
#include "atlas_pokemon.h"
#include "constants/species.h"

static void CheckBoundaryAndInvalidInputs(void)
{
    static const s32 xs[] = { INT_MIN, INT_MIN + 1, -ATLAS_WORLD_WIDTH, -1, 0, 1, ATLAS_WORLD_WIDTH - 1, ATLAS_WORLD_WIDTH, INT_MAX - 1, INT_MAX };
    static const s32 ys[] = { INT_MIN, -10001500, -1, 0, 1, 10001500, INT_MAX };
    static const u32 seeds[] = { 0u, 1u, 0xDEADBEEFu, 0xFFFFFFFFu };
    struct AtlasCell cell = {0};
    struct AtlasPokemonSpawn a, b;
    unsigned int i, j, k;
    cell.biome = ATLAS_GRASSLAND;
    cell.temperature = 140;
    cell.moisture = 120;
    cell.waterform = ATLAS_DRY;
    cell.landform = ATLAS_PLAIN;
    cell.elevation = 130;
    for (i = 0; i < sizeof(xs)/sizeof(xs[0]); i++)
    for (j = 0; j < sizeof(ys)/sizeof(ys[0]); j++)
    for (k = 0; k < sizeof(seeds)/sizeof(seeds[0]); k++)
    {
        s32 normalized = xs[i] % ATLAS_WORLD_WIDTH;
        bool8 left, right;
        if (normalized < 0) normalized += ATLAS_WORLD_WIDTH;
        assert(AtlasPokemon_IsCandidate(xs[i], ys[j], seeds[k]) == AtlasPokemon_IsCandidate(normalized, ys[j], seeds[k]));
        left = AtlasPokemon_Select(xs[i], ys[j], seeds[k], &cell, &a);
        right = AtlasPokemon_Select(normalized, ys[j], seeds[k], &cell, &b);
        assert(left == right);
        assert(a.species == b.species && a.graphicsId == b.graphicsId && a.rarity == b.rarity);
    }
    cell.biome = ATLAS_WETLAND + 1;
    a.species = 0xFFFF;
    assert(!AtlasPokemon_Select(1, 1, 1, &cell, &a));
    assert(a.species == SPECIES_NONE && a.graphicsId == 0);
    cell.biome = ATLAS_GRASSLAND;
    cell.waterform = ATLAS_LAKE + 1;
    assert(!AtlasPokemon_Select(1, 1, 1, &cell, &a));
    cell.waterform = ATLAS_DRY;
    cell.landform = ATLAS_KNOLL + 1;
    assert(!AtlasPokemon_Select(1, 1, 1, &cell, &a));
    cell.landform = ATLAS_PLAIN;
    unsigned duneSpawns=0;
    for(s32 x=0;x<10000;x++)
    {
        cell.landform=ATLAS_PLAIN;
        bool8 selected=AtlasPokemon_Select(x,1,1,&cell,&a);
        for(unsigned form=ATLAS_DUNE;form<=ATLAS_MESA;form++)
        {
            cell.landform=form;
            assert(selected==AtlasPokemon_Select(x,1,1,&cell,&b));
            assert(a.species==b.species && a.graphicsId==b.graphicsId && a.rarity==b.rarity);
        }
        duneSpawns+=selected;
    }
    assert(duneSpawns>0);
    for(s32 x=0;x<10000;x++)
    {
        cell.landform=ATLAS_COAST;
        bool8 selected=AtlasPokemon_Select(x,1,1,&cell,&a);
        cell.landform=ATLAS_ROCK_COAST;
        assert(selected==AtlasPokemon_Select(x,1,1,&cell,&b));
        assert(a.species==b.species && a.graphicsId==b.graphicsId && a.rarity==b.rarity);
    }
    cell.landform=ATLAS_PLAIN;
    assert(!AtlasPokemon_Select(1, 1, 1, NULL, &a));
    assert(a.species == SPECIES_NONE && a.graphicsId == 0);
    assert(!AtlasPokemon_Select(1, 1, 1, &cell, NULL));
}

static void CheckPolarAdmission(void)
{
    static const s32 rejected[] = {
        INT_MIN, -ATLAS_WORLD_HALF_HEIGHT - 1,
        ATLAS_WORLD_HALF_HEIGHT, INT_MAX
    };
    static const s32 accepted[] = {
        -ATLAS_WORLD_HALF_HEIGHT, ATLAS_WORLD_HALF_HEIGHT - 1
    };
    struct AtlasCell cell = {0};
    struct AtlasPokemonSpawn spawn;
    unsigned int i;
    s32 x;
    unsigned int validCandidates = 0;

    cell.biome = ATLAS_GRASSLAND;
    cell.temperature = 140;
    cell.moisture = 120;
    cell.waterform = ATLAS_DRY;
    cell.landform = ATLAS_PLAIN;
    cell.elevation = 130;

    /* A climate record cannot authorize an actor outside the world's poles. */
    for (i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++)
    for (x = -256; x <= 256; x++)
    {
        spawn.species = 0xFFFF;
        spawn.graphicsId = 255;
        assert(!AtlasPokemon_IsCandidate(x, rejected[i], 0xA756u));
        assert(!AtlasPokemon_Select(x, rejected[i], 0xA756u, &cell, &spawn));
        assert(spawn.species == SPECIES_NONE && spawn.graphicsId == 0);
    }

    /* Inclusive south pole and last northern row still use ordinary hashing. */
    for (i = 0; i < sizeof(accepted) / sizeof(accepted[0]); i++)
    for (x = 0; x < 10000; x++)
    {
        bool8 candidate = AtlasPokemon_IsCandidate(x, accepted[i], 0xA756u);
        bool8 selected = AtlasPokemon_Select(x, accepted[i], 0xA756u, &cell, &spawn);
        assert(candidate == selected);
        if (candidate)
            validCandidates++;
    }
    assert(validCandidates > 0);
}

int main(void)
{
    CheckBoundaryAndInvalidInputs();
    CheckPolarAdmission();
    puts("PASS native climate selector full signed coordinate boundaries, dateline normalization, invalid cell and null handling");
    return 0;
}
