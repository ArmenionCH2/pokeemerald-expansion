#ifndef GUARD_ATLAS_PLACE_H
#define GUARD_ATLAS_PLACE_H
#include "atlas_world.h"

#define ATLAS_PLACE_NAME_BYTES 21
enum AtlasPlaceKind { ATLAS_PLACE_OCEAN, ATLAS_PLACE_RIVER, ATLAS_PLACE_LAKE,
    ATLAS_PLACE_COAST, ATLAS_PLACE_MOUNTAINS, ATLAS_PLACE_FOOTHILLS,
    ATLAS_PLACE_FOREST, ATLAS_PLACE_TAIGA, ATLAS_PLACE_TUNDRA,
    ATLAS_PLACE_DESERT, ATLAS_PLACE_WETLAND, ATLAS_PLACE_SAVANNA, ATLAS_PLACE_PLAIN,
    ATLAS_PLACE_PLATEAU, ATLAS_PLACE_VALLEY, ATLAS_PLACE_RIDGE, ATLAS_PLACE_VOLCANO, ATLAS_PLACE_CALDERA, ATLAS_PLACE_FLOODPLAIN, ATLAS_PLACE_CREEK, ATLAS_PLACE_DUNES, ATLAS_PLACE_MESA, ATLAS_PLACE_HEADWATER, ATLAS_PLACE_ROCK_COAST, ATLAS_PLACE_SEA_CLIFF, ATLAS_PLACE_SINKHOLE, ATLAS_PLACE_CANYON, ATLAS_PLACE_BACKSWAMP, ATLAS_PLACE_LEVEE };
struct AtlasPlace { u16 landmass, basin; u8 kind; u16 landmark; };
struct AtlasPlaceTracker
{
    struct AtlasPlace previous, candidate;
    u8 stable, cooldown;
    bool8 initialized, pending;
};
/* Names follow connected macro landmasses and drainage catchments, never
 * chunk IDs. Bounded cones/calderas, mesas and sinkholes use their seeded
 * macro-site identity; generic forests/ridges remain contextual descriptors. */
bool8 AtlasPlace_Identify(s32 x, s32 y, const struct AtlasCell *cell, struct AtlasPlace *out);
void AtlasPlace_SetLandmark(struct AtlasPlace *place,s32 x,s32 y);
bool8 AtlasPlace_Name(const struct AtlasPlace *place, u8 level, char out[ATLAS_PLACE_NAME_BYTES]);
/* Nearby-water bits use 1 << AtlasWaterform. A dry bank keeps its actual
 * landmass/catchment identity while acquiring the shoreline descriptor. */
void AtlasPlace_ApplyShore(struct AtlasPlace *place,u8 watermask);
/* A new parent includes its descendants; unchanged parents are omitted.
 * Call at a fixed cadence. Four observations debounce a boundary; twenty
 * observations separate announcements. No save or exploration mutation. */
u8 AtlasPlace_Update(struct AtlasPlaceTracker *tracker, const struct AtlasPlace *place);
void AtlasLocation_Reset(void);
void AtlasLocation_Tick(void);
u8 *AtlasLocation_PopupName(u8 *dest);
#endif
