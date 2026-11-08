#include "global.h"
#include "atlas_world.h"
#include "atlas_state.h"

static bool8 Coordinates(s32 *x, s32 y)
{
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
        return FALSE;
    *x %= ATLAS_WORLD_WIDTH;
    if (*x < 0) *x += ATLAS_WORLD_WIDTH;
    return TRUE;
}
static s32 ChunkY(s32 y) { return y >= 0 ? y / 20 : (y - 19) / 20; }
static u16 TileIndex(s32 x, s32 y) { return (y - ChunkY(y) * 20) * 20 + x % 20; }
void AtlasState_Init(struct AtlasSaveState *state, u32 seed)
{
    memset(state, 0, sizeof(*state));
    state->magic = ATLAS_STATE_MAGIC;
    state->version = ATLAS_STATE_VERSION;
    state->seed = seed;
}
bool8 AtlasState_Valid(const struct AtlasSaveState *state, u32 seed)
{
    u32 i, j;
    if (!state || state->magic != ATLAS_STATE_MAGIC || state->version != ATLAS_STATE_VERSION
     || state->seed != seed || state->count > ATLAS_JOURNAL_CHUNKS || state->frames >= 60)
        return FALSE;
    for (i = 0; i < state->count; i++)
    {
        const struct AtlasVisitedChunk *chunk = &state->visited[i];
        if (chunk->x < 0 || chunk->x >= ATLAS_WORLD_WIDTH / 20
         || chunk->y < -ATLAS_WORLD_HALF_HEIGHT / 20 || chunk->y >= ATLAS_WORLD_HALF_HEIGHT / 20)
            return FALSE;
        for (j = 0; j < i; j++)
            if (state->visited[j].x == chunk->x && state->visited[j].y == chunk->y)
                return FALSE;
    }
    return TRUE;
}
bool8 AtlasState_Visit(struct AtlasSaveState *state, s32 x, s32 y)
{
    u32 i;
    u16 bit;
    s32 cx, cy;
    if (!state || state->count > ATLAS_JOURNAL_CHUNKS || !Coordinates(&x, y)) return FALSE;
    cx = x / 20; cy = ChunkY(y); bit = TileIndex(x, y);
    for (i = 0; i < state->count; i++)
        if (state->visited[i].x == cx && state->visited[i].y == cy) break;
    if (i == state->count)
    {
        if (i == ATLAS_JOURNAL_CHUNKS) return FALSE;
        state->visited[i].x = cx; state->visited[i].y = cy;
        memset(state->visited[i].tiles, 0, ATLAS_VISIT_BYTES);
        state->count++;
    }
    state->visited[i].tiles[bit / 8] |= 1 << (bit % 8);
    return TRUE;
}
bool8 AtlasState_Explored(const struct AtlasSaveState *state, s32 x, s32 y)
{
    u32 i;
    u16 bit;
    if (!state || state->count > ATLAS_JOURNAL_CHUNKS || !Coordinates(&x, y)) return FALSE;
    bit = TileIndex(x, y);
    for (i = 0; i < state->count; i++)
        if (state->visited[i].x == x / 20 && state->visited[i].y == ChunkY(y))
            return !!(state->visited[i].tiles[bit / 8] & (1 << (bit % 8)));
    return FALSE;
}
void AtlasState_Tick(struct AtlasSaveState *state)
{
    if (++state->frames == 60)
    {
        state->frames = 0;
        if (state->seconds != 0xFFFFFFFFu) state->seconds++;
    }
}
u8 AtlasState_Weather(const struct AtlasSaveState *state, s32 x, s32 y, u8 temperature, u8 humidity)
{
    u32 hash;
    if (!Coordinates(&x, y)) return ATLAS_WEATHER_CLEAR;
    /* Regional conditions advance with saved simulation time, not render order.
     * This is a coarse weather model; baseline biomes remain untouched. */
    hash = state->seed ^ (u32)(x / 1000) * 0x9E3779B1u
         ^ (u32)AtlasTerrain_FloorDiv(y, 1000) * 0x85EBCA77u ^ (state->seconds / 3600);
    hash ^= hash >> 16; hash *= 0x7FEB352Du; hash ^= hash >> 15;
    if (humidity >= 120 && hash % 256 < humidity / 2)
        return temperature < 80 ? ATLAS_WEATHER_SNOW : ATLAS_WEATHER_RAIN;
    if (humidity >= 180 && (hash >> 8) % 8 == 0) return ATLAS_WEATHER_FOG;
    return ATLAS_WEATHER_CLEAR;
}
