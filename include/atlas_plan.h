#ifndef GUARD_ATLAS_PLAN_H
#define GUARD_ATLAS_PLAN_H
#define ATLAS_PLAN_WIDTH 96
#define ATLAS_PLAN_HEIGHT 48
#define ATLAS_PLAN_COUNT (ATLAS_PLAN_WIDTH * ATLAS_PLAN_HEIGHT)
#define ATLAS_PLAN_SEA 121
#define ATLAS_PLAN_STAGES 11
#define ATLAS_BASIN_MASK 0x1FFF
#define ATLAS_LAKE_FLAG 0x8000
#define ATLAS_OCEAN_COMPONENT 0x8000
#define ATLAS_PLAN_BYTES (ATLAS_PLAN_COUNT * 12)
/* Version 4 also labels connected seas in continent, tagged with the high bit.
 * Land labels retain their original untagged identity. No extra node storage. */
struct AtlasPlanNode { u16 parent, runoff, continent, basin; u8 height, rain, humidity, water; };
struct AtlasPlanStats { u32 seed, checksum; u16 land, continents, rivers, lakes, basins, cold, wet; u32 habitatChecksum; };
struct AtlasPlanSample { u8 height, rain, humidity, water, lake; };
struct AtlasHabitat { u8 temperature, humidity, moisture, seasonalRange; };
#ifdef __arm__
#include "malloc.h"
#define ATLAS_RUNTIME_HEAP_SIZE (HEAP_SIZE - ATLAS_PLAN_BYTES)
/* A scene-owned reservation in the existing heap, not additional EWRAM.
 * The original full heap remains available outside Atlas scenes. */
#define gAtlasPlan (*(struct AtlasPlanNode (*)[ATLAS_PLAN_COUNT])(gHeap + ATLAS_RUNTIME_HEAP_SIZE))
#else
extern struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
#endif
extern bool8 gAtlasPlanReserved;
void AtlasPlan_ReserveHeap(void);
extern struct AtlasPlanStats gAtlasPlanStats;
extern u32 gAtlasWorldSeed;
extern bool8 gAtlasPlanReady;
bool8 AtlasPlan_Begin(u32 seed);
bool8 AtlasPlan_ImportHeights(const u8 *heights);
bool8 AtlasPlan_Step(u16 work);
u8 AtlasPlan_Stage(void);
u8 AtlasPlan_Progress(void);
void AtlasPlan_Sample(s32 x, s32 y, u8 *height, u8 *rain);
void AtlasPlan_SampleDetail(s32 x, s32 y, struct AtlasPlanSample *sample);
void AtlasPlan_ResetSample(void);
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *plan);
void AtlasHabitat_Sample(s32 y, u8 elevation, u8 rain, u8 vapor, u8 waterform, struct AtlasHabitat *out);
void AtlasWater_Reset(void);
bool8 AtlasWater_IsTributary(u16 node);
u8 AtlasWater_Sample(s32 x, s32 y, u8 terrainHeight, const struct AtlasPlanSample *plan, u8 *surface, u8 *valley);
u8 AtlasWater_MouthStep(u16 node,s32 *x,s32 *y,s32 *dx,s32 *dy);
void AtlasWater_Point(u16 node, u8 part, s32 *x, s32 *y, u16 *height);
void AtlasTerrain_SetSeed(u32 seed);
void CB2_AtlasCreateWorld(void);
void CB2_AtlasRestoreWorld(void);
void AtlasWorld_FinishNewGame(void);
u32 AtlasWorld_ReadSeed(void);
#endif
