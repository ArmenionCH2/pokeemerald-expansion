#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_place.h"

static bool8 Equal(const struct AtlasPlace *a, const struct AtlasPlace *b)
{
    return a->landmass == b->landmass && a->basin == b->basin && a->kind == b->kind;
}

bool8 AtlasPlace_Identify(s32 x, s32 y, const struct AtlasCell *cell, struct AtlasPlace *out)
{
    u32 sx, sy, col, row, fx, fy, best = 0xFFFFFFFF;
    u16 chosen = ATLAS_PLAN_COUNT;
    unsigned i;
    if (out == NULL) return FALSE;
    memset(out, 0, sizeof(*out));
    if (!gAtlasPlanReady || cell == NULL || y < -ATLAS_WORLD_HALF_HEIGHT
     || y >= ATLAS_WORLD_HALF_HEIGHT || cell->waterform > ATLAS_LAKE
     || cell->biome > ATLAS_WETLAND || cell->landform > ATLAS_COAST) return FALSE;
    x %= ATLAS_WORLD_WIDTH; if (x < 0) x += ATLAS_WORLD_WIDTH;
    sx = (u32)x * ATLAS_PLAN_WIDTH;
    sy = (u32)(y + ATLAS_WORLD_HALF_HEIGHT) * (ATLAS_PLAN_HEIGHT - 1);
    col = sx / ATLAS_WORLD_WIDTH; row = sy / (2 * ATLAS_WORLD_HALF_HEIGHT);
    fx = (sx % ATLAS_WORLD_WIDTH) / (ATLAS_WORLD_WIDTH / 256);
    fy = (sy % (2 * ATLAS_WORLD_HALF_HEIGHT)) / (2 * ATLAS_WORLD_HALF_HEIGHT / 256);
    if (fx > 255) fx = 255;
    if (fy > 255) fy = 255;
    for (i = 0; i < 4; i++)
    {
        u32 cx = (col + (i & 1)) % ATLAS_PLAN_WIDTH;
        u32 cy = row + (i >> 1);
        s32 dx = (i & 1) ? 256 - fx : fx, dy = (i >> 1) ? 256 - fy : fy;
        u32 distance = dx * dx + dy * dy;
        u16 id;
        if (cy >= ATLAS_PLAN_HEIGHT) cy = ATLAS_PLAN_HEIGHT - 1;
        id = cy * ATLAS_PLAN_WIDTH + cx;
        if (!gAtlasPlan[id].continent || distance >= best
         || !!(gAtlasPlan[id].continent & ATLAS_OCEAN_COMPONENT) != (cell->waterform == ATLAS_SEA)) continue;
        chosen = id; best = distance;
    }
    if (chosen != ATLAS_PLAN_COUNT)
    {
        out->landmass = gAtlasPlan[chosen].continent;
        if (cell->waterform!=ATLAS_SEA) out->basin = gAtlasPlan[chosen].basin & ATLAS_BASIN_MASK;
    }
    if (cell->waterform==ATLAS_SEA) return TRUE;
    out->kind = cell->waterform == ATLAS_RIVER ? ATLAS_PLACE_RIVER
              : cell->waterform == ATLAS_LAKE ? ATLAS_PLACE_LAKE
              : cell->landform == ATLAS_COAST ? ATLAS_PLACE_COAST
              : cell->landform == ATLAS_MOUNTAIN ? ATLAS_PLACE_MOUNTAINS
              : cell->landform == ATLAS_HILL ? ATLAS_PLACE_FOOTHILLS
              : cell->biome == ATLAS_TUNDRA ? ATLAS_PLACE_TUNDRA
              : cell->biome == ATLAS_TAIGA ? ATLAS_PLACE_TAIGA
              : cell->biome == ATLAS_DESERT ? ATLAS_PLACE_DESERT
              : cell->biome == ATLAS_WETLAND ? ATLAS_PLACE_WETLAND
              : cell->biome == ATLAS_SAVANNA ? ATLAS_PLACE_SAVANNA
              : cell->biome == ATLAS_FOREST || cell->biome == ATLAS_JUNGLE ? ATLAS_PLACE_FOREST
              : ATLAS_PLACE_PLAIN;
    return TRUE;
}

void AtlasPlace_ApplyShore(struct AtlasPlace *place,u8 watermask)
{
    if (!place || place->kind==ATLAS_PLACE_OCEAN) return;
    if (watermask&(1<<ATLAS_LAKE)) place->kind=ATLAS_PLACE_LAKE;
    else if (watermask&(1<<ATLAS_RIVER)) place->kind=ATLAS_PLACE_RIVER;
    else if (watermask&(1<<ATLAS_SEA)) place->kind=ATLAS_PLACE_COAST;
}

static void Append(char *out, const char *text)
{
    unsigned at = 0;
    while (out[at]) at++;
    while (*text && at + 1 < ATLAS_PLACE_NAME_BYTES) out[at++] = *text++;
    out[at] = 0;
}

bool8 AtlasPlace_Name(const struct AtlasPlace *place, u8 level, char out[ATLAS_PLACE_NAME_BYTES])
{
    static const char *const first[] = {"Al","Bel","Cor","Dar","El","Fal","Gal","Har","Is","Kel","Lor","Mar","Nor","Or","Pel","Syl"};
    static const char *const middle[] = {"a","e","i","o","u","an","en","in","or","el","ar","ir","al","on","un","es"};
    static const char *const last[] = {"dor","len","mir","ran","sil","tor","ven","wyn","bar","del","fin","gar","hel","ith","jor","kas","lin","mor","nar","ond","por","quil","rin","sar","tal","ul","val","wer","xel","yor","zan","den"};
    static const char *const feature[] = {"Ocean","Riverbank","Lake shore","Coast","Highlands","Foothills","Forest","Taiga","Tundra","Desert","Wetland","Savanna","Plains","Plateau","Valley","Ridge","Volcanic Cone","Caldera","Floodplain","Creekbank"};
    u32 code, id, i, count = 0;
    if (out == NULL) return FALSE;
    out[0] = 0;
    if (place == NULL || level > 2 || place->kind > ATLAS_PLACE_CREEK
     || (place->landmass & ~ATLAS_OCEAN_COMPONENT) > ATLAS_PLAN_COUNT || place->basin > ATLAS_PLAN_COUNT) return FALSE;
    if (level == 2) {Append(out, feature[place->kind]);return TRUE;}
    id = level == 0 ? place->landmass & ~ATLAS_OCEAN_COMPONENT : place->basin;
    if (!id)
    {
        if (level == 1) return FALSE;
        Append(out, place->kind == ATLAS_PLACE_OCEAN ? "Open Ocean" : "Coastal Islets");
        return TRUE;
    }
    /* An odd multiplier permutes all 8192 IDs: distinct plan component IDs
     * receive distinct roots for a given seed and naming domain. */
    code = (id * 4051u + (gAtlasWorldSeed ^ (gAtlasWorldSeed >> 16)) + level * 997u) & 8191;
    Append(out, first[code & 15]); Append(out, middle[(code >> 4) & 15]); Append(out, last[code >> 8]);
    if (level == 1) Append(out, " Basin");
    else
    {
        for (i = 0; i < ATLAS_PLAN_COUNT; i++) if (gAtlasPlan[i].continent == place->landmass) count++;
        if (place->landmass & ATLAS_OCEAN_COMPONENT) Append(out,count>=128?" Ocean":" Sea");
        else Append(out, count >= 32 ? " Continent" : " Island");
    }
    return TRUE;
}

u8 AtlasPlace_Update(struct AtlasPlaceTracker *tracker, const struct AtlasPlace *place)
{
    u8 mask;
    if (tracker == NULL || place == NULL) return 0;
    if (tracker->cooldown) tracker->cooldown--;
    if (!tracker->pending || !Equal(&tracker->candidate, place))
    {
        tracker->candidate = *place; tracker->stable = 1; tracker->pending = TRUE;
        return 0;
    }
    if (tracker->stable < 4) tracker->stable++;
    if (tracker->stable < 4 || tracker->cooldown) return 0;
    mask = !tracker->initialized || tracker->previous.landmass != place->landmass ? 7
         : tracker->previous.basin != place->basin ? 6
         : tracker->previous.kind != place->kind ? 4 : 0;
    if (mask)
    {
        tracker->previous = *place; tracker->initialized = TRUE; tracker->cooldown = 20;
    }
    return mask;
}
