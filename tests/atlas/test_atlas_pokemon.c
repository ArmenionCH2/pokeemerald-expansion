#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "atlas_pokemon.h"
#include "constants/species.h"
#include "constants/event_objects.h"

static struct AtlasCell Cell(u8 biome, u8 temperature, u8 moisture, u8 waterform, u8 landform, u8 elevation)
{
    struct AtlasCell cell = {0};
    cell.biome = biome;
    cell.temperature = temperature;
    cell.moisture = moisture;
    cell.waterform = waterform;
    cell.landform = landform;
    cell.elevation = elevation;
    return cell;
}

static void CheckStableAndWrap(void)
{
    struct AtlasCell cell = Cell(ATLAS_GRASSLAND, 140, 120, ATLAS_DRY, ATLAS_PLAIN, 130);
    struct AtlasPokemonSpawn a, b;
    s32 y, x;
    int seen = 0, different = 0;
    for (y = -40; y <= 40; y++)
    for (x = -40; x <= 40; x++)
    {
        bool8 first = AtlasPokemon_Select(x, y, 0xBEEFu, &cell, &a);
        bool8 again = AtlasPokemon_Select(x + ATLAS_WORLD_WIDTH, y, 0xBEEFu, &cell, &b);
        assert(first == again);
        assert(a.species == b.species && a.graphicsId == b.graphicsId && a.rarity == b.rarity);
        assert(first == AtlasPokemon_IsCandidate(x, y, 0xBEEFu));
        if (first)
        {
            seen++;
            assert(a.graphicsId == (OBJ_EVENT_MON | a.species) && a.graphicsId > 255);
            assert(a.species != SPECIES_NONE && a.graphicsId != 0);
            assert(a.rarity < 4);
        }
        if (AtlasPokemon_IsCandidate(x, y, 0xBEEFu) != AtlasPokemon_IsCandidate(x, y, 0xFACEu))
            different++;
    }
    assert(seen > 35 && seen < 115);
    assert(different > 50);
    assert(!AtlasPokemon_Select(0, 0, 1, NULL, &a));
    assert(!AtlasPokemon_Select(0, 0, 1, &cell, NULL));
    assert(a.species == SPECIES_NONE);
}

static void CheckClimateAndGraphics(void)
{
    const struct AtlasCell grass = Cell(ATLAS_GRASSLAND, 140, 120, ATLAS_DRY, ATLAS_PLAIN, 130);
    const struct AtlasCell dry = Cell(ATLAS_DESERT, 200, 28, ATLAS_DRY, ATLAS_PLAIN, 130);
    const struct AtlasCell cold = Cell(ATLAS_TUNDRA, 40, 180, ATLAS_DRY, ATLAS_PLAIN, 130);
    const struct AtlasCell water = Cell(ATLAS_WETLAND, 150, 230, ATLAS_RIVER, ATLAS_PLAIN, 125);
    const struct AtlasCell coast = Cell(ATLAS_GRASSLAND, 180, 130, ATLAS_DRY, ATLAS_COAST, 125);
    const struct AtlasCell wet = Cell(ATLAS_WETLAND, 170, 210, ATLAS_DRY, ATLAS_PLAIN, 130);
    const struct AtlasCell high = Cell(ATLAS_GRASSLAND, 140, 120, ATLAS_DRY, ATLAS_MOUNTAIN, 210);
    struct AtlasPokemonSpawn result;
    u32 common = 0, uncommon = 0, rare = 0, veryRare = 0, coastCount = 0, wetCount = 0;
    s32 x;
    for (x = 0; x < 200000; x++)
    {
        s32 px = x % 1000, py = x / 1000;
        assert(AtlasPokemon_Select(px, py, 123, &dry, &result) == AtlasPokemon_IsCandidate(px, py, 123));
        assert(AtlasPokemon_Select(px, py, 123, &cold, &result) == AtlasPokemon_IsCandidate(px, py, 123));
        assert(!AtlasPokemon_Select(px, py, 123, &water, &result));
        assert(AtlasPokemon_Select(px, py, 123, &high, &result) == AtlasPokemon_IsCandidate(px, py, 123));
        if (AtlasPokemon_Select(px, py, 123, &grass, &result))
        {
            if (result.rarity == ATLAS_POKEMON_COMMON)
                common++, assert(result.species == SPECIES_ZIGZAGOON && result.graphicsId == (OBJ_EVENT_MON | SPECIES_ZIGZAGOON));
            else if (result.rarity == ATLAS_POKEMON_UNCOMMON)
                uncommon++, assert(result.species == SPECIES_POOCHYENA && result.graphicsId == (OBJ_EVENT_MON | SPECIES_POOCHYENA));
            else if (result.rarity == ATLAS_POKEMON_RARE)
                rare++, assert(result.species == SPECIES_SKITTY && result.graphicsId == (OBJ_EVENT_MON | SPECIES_SKITTY));
            else
                veryRare++, assert(result.species == SPECIES_PIKACHU && result.graphicsId == (OBJ_EVENT_MON | SPECIES_PIKACHU));
        }
        if (AtlasPokemon_Select(px, py, 123, &coast, &result))
        {
            coastCount++;
            assert(result.species == SPECIES_WINGULL || result.species == SPECIES_AZURILL);
        }
        if (AtlasPokemon_Select(px, py, 123, &wet, &result))
        {
            wetCount++;
            assert(result.species == SPECIES_AZURILL || result.species == SPECIES_AZUMARILL || result.species == SPECIES_KECLEON);
        }
    }
    assert(common > 1250 && common < 1550);
    assert(uncommon > 390 && uncommon < 520);
    assert(rare > 90 && rare < 185);
    assert(veryRare > 7 && veryRare < 45);
    assert(coastCount > 1500 && wetCount > 1700);
    printf("PASS climate roster: grass=%u/%u/%u/%u coast=%u wetland=%u\n",
           common, uncommon, rare, veryRare, coastCount, wetCount);
}

static void CheckExpansionHabitats(void)
{
    static const u16 rosters[3][4] = {
        {SPECIES_SANDSHREW, SPECIES_TRAPINCH, SPECIES_CACNEA, SPECIES_BALTOY},
        {SPECIES_SWINUB, SPECIES_SNORUNT, SPECIES_DELIBIRD, SPECIES_SNEASEL},
        {SPECIES_GEODUDE, SPECIES_ARON, SPECIES_MACHOP, SPECIES_SKARMORY}
    };
    const struct AtlasCell habitats[] = {
        Cell(ATLAS_DESERT, 200, 28, ATLAS_DRY, ATLAS_PLAIN, 130),
        Cell(ATLAS_TUNDRA, 40, 100, ATLAS_DRY, ATLAS_MOUNTAIN, 210),
        Cell(ATLAS_GRASSLAND, 100, 100, ATLAS_DRY, ATLAS_MOUNTAIN, 210)
    };
    unsigned h, x;
    for (h = 0; h < 3; h++)
    {
        unsigned seen = 0;
        for (x = 0; x < 200000; x++)
        {
            struct AtlasPokemonSpawn spawn, wrapped;
            if (!AtlasPokemon_Select(x, 0, 123, &habitats[h], &spawn)) continue;
            assert(spawn.species == rosters[h][spawn.rarity]);
            assert(spawn.graphicsId == (OBJ_EVENT_MON | spawn.species));
            assert(spawn.graphicsId > 255);
            assert(AtlasPokemon_Select(x + ATLAS_WORLD_WIDTH, 0, 123, &habitats[h], &wrapped));
            assert(wrapped.species == spawn.species && wrapped.graphicsId == spawn.graphicsId);
            seen |= 1u << spawn.rarity;
        }
        assert(seen == 15); /* Every rarity is reachable in each habitat. */
    }
    puts("PASS twelve cold/desert/highland species, cold precedence, full-width graphics and seam replay");
}
int main(void)
{
    CheckStableAndWrap();
    CheckClimateAndGraphics();
    CheckExpansionHabitats();
    puts("PASS Atlas Expansion species graphics, climate gates, rarity, seam and deterministic seeds");
    return 0;
}
