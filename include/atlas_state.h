#ifndef GUARD_ATLAS_STATE_H
#define GUARD_ATLAS_STATE_H

/* Fixed save budget; never evict a visited chunk to make another look unknown.
 * Unknown format versions are rejected, never silently reset on Continue. */
#define ATLAS_JOURNAL_CHUNKS 24
#define ATLAS_VISIT_BYTES 50
#define ATLAS_JOURNAL_BYTES 1440
#define ATLAS_JOURNAL_MAX_CHUNKS (ATLAS_JOURNAL_BYTES / 11)
#define ATLAS_STATE_MAGIC 0x41544C53u
#define ATLAS_STATE_VERSION 2
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
    union
    {
        struct AtlasVisitedChunk visited[ATLAS_JOURNAL_CHUNKS]; // version 1 recovery layout
        u8 journal[ATLAS_JOURNAL_BYTES]; // version 2 lossless row/column runs or bitmap
    };
};
enum AtlasWeather { ATLAS_WEATHER_CLEAR, ATLAS_WEATHER_RAIN, ATLAS_WEATHER_SNOW, ATLAS_WEATHER_FOG };
void AtlasState_Init(struct AtlasSaveState *state, u32 seed);
bool8 AtlasState_Valid(const struct AtlasSaveState *state, u32 seed);
/* In-place upgrade of this port's validated version 1 only. Unknown versions
 * fail without mutation. Every legacy tile survives; allocation size stays 1456. */
bool8 AtlasState_Upgrade(struct AtlasSaveState *state);
bool8 AtlasState_GetChunk(const struct AtlasSaveState *state, u8 index, struct AtlasVisitedChunk *out);
u16 AtlasState_UsedBytes(const struct AtlasSaveState *state);
bool8 AtlasState_Visit(struct AtlasSaveState *state, s32 x, s32 y);
bool8 AtlasState_Explored(const struct AtlasSaveState *state, s32 x, s32 y);
void AtlasState_Tick(struct AtlasSaveState *state);
u8 AtlasState_Weather(const struct AtlasSaveState *state, s32 x, s32 y, u8 temperature, u8 humidity);
void CB2_AtlasMap(void);
#endif
