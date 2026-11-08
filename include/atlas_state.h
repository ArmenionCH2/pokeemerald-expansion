#ifndef GUARD_ATLAS_STATE_H
#define GUARD_ATLAS_STATE_H

/* Fixed save budget; never evict a visited chunk to make another look unknown.
 * Unknown format versions are rejected, never silently reset on Continue. */
#define ATLAS_JOURNAL_CHUNKS 24
#define ATLAS_VISIT_BYTES 50
#define ATLAS_STATE_MAGIC 0x41544C53u
#define ATLAS_STATE_VERSION 1
struct AtlasVisitedChunk
{
    s32 x, y;
    u8 tiles[ATLAS_VISIT_BYTES];
};
struct AtlasSaveState
{
    u32 magic, seed, seconds;
    u16 version;
    u8 frames, count;
    struct AtlasVisitedChunk visited[ATLAS_JOURNAL_CHUNKS];
};
enum AtlasWeather { ATLAS_WEATHER_CLEAR, ATLAS_WEATHER_RAIN, ATLAS_WEATHER_SNOW, ATLAS_WEATHER_FOG };
void AtlasState_Init(struct AtlasSaveState *state, u32 seed);
bool8 AtlasState_Valid(const struct AtlasSaveState *state, u32 seed);
bool8 AtlasState_Visit(struct AtlasSaveState *state, s32 x, s32 y);
bool8 AtlasState_Explored(const struct AtlasSaveState *state, s32 x, s32 y);
void AtlasState_Tick(struct AtlasSaveState *state);
u8 AtlasState_Weather(const struct AtlasSaveState *state, s32 x, s32 y, u8 temperature, u8 humidity);
void CB2_AtlasMap(void);
#endif
