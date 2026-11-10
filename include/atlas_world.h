#ifndef GUARD_ATLAS_WORLD_H
#define GUARD_ATLAS_WORLD_H
#define ATLAS_CHUNK_SIZE 20
#define ATLAS_WORLD_WIDTH 40075000
#define ATLAS_WORLD_HALF_HEIGHT 10001500
#define ATLAS_WORLD_SIZE ATLAS_WORLD_WIDTH
#define ATLAS_GENERATION_VERSION 26
#define ATLAS_GENERATION_OCEAN_BALANCE 4
#define ATLAS_GENERATION_LEGACY 3
#define ATLAS_SAVE_MAGIC 0xA756
enum AtlasBiome { ATLAS_OCEAN, ATLAS_JUNGLE, ATLAS_FOREST, ATLAS_GRASSLAND, ATLAS_SAVANNA, ATLAS_DESERT, ATLAS_TAIGA, ATLAS_TUNDRA, ATLAS_WETLAND };
enum AtlasWaterform { ATLAS_DRY, ATLAS_SEA, ATLAS_RIVER, ATLAS_LAKE };
enum AtlasLandform { ATLAS_PLAIN, ATLAS_HILL, ATLAS_MOUNTAIN, ATLAS_COAST, ATLAS_DUNE, ATLAS_DUNE_CREST, ATLAS_MESA, ATLAS_ROCK_COAST, ATLAS_SINKHOLE, ATLAS_CANYON, ATLAS_BACKSWAMP, ATLAS_LEVEE };
struct AtlasCell { u8 elevation, temperature, moisture, biome, landform, humidity, seasonalRange, waterform, valley; };
void AtlasTerrain_Sample(s32 x, s32 y, struct AtlasCell *cell);
u16 AtlasTerrain_Tile(s32 x, s32 y);
bool8 AtlasTerrain_Fill(u16 *map, s32 width, s32 height, s32 chunkX, s32 chunkY);
bool8 AtlasTerrain_FindSpawn(s32 *x, s32 *y);
bool8 AtlasTerrain_RepairLanding(s32 *x,s32 *y);
s32 AtlasTerrain_FloorDiv(s32 n, s32 d);
bool8 AtlasWorld_IsActive(void);
bool8 AtlasWorld_SaveValid(void);
u16 AtlasWorld_Generation(void);
/* Select only before AtlasPlan_Begin, never during live traversal. Fresh
 * worlds use version 26; saved versions 3 through 25 retain their terrain rules. */
bool8 AtlasWorld_SetGeneration(u16 version);
bool8 AtlasWorld_RestoreGeneration(void);
const struct MapHeader *AtlasWorld_Header(const struct MapHeader *base);
void AtlasWorld_Fill(void);
void AtlasWorld_Tick(void);
void AtlasWorld_GetPosition(s32 *x, s32 *y);
void AtlasWildlife_Tick(void);
void AtlasWorld_Move(u8 direction);
void AtlasWorld_NewGame(void);
bool8 AtlasStream_Load(u16 *map, s32 x, s32 y);
void AtlasStream_Tick(s32 x, s32 y, s32 localX, s32 localY);
void AtlasStream_Reset(void);
/* Read an already rendered ring entry; never generate, schedule or mark a visit. */
bool8 AtlasStream_ReadSemantics(s32 x,s32 y,u16 *out);
extern u32 gAtlasCacheHits, gAtlasCacheMisses;
u8 AtlasMountain_Relief(s32 x, s32 y, const struct AtlasCell *cell);
void AtlasMountain_Center(s32 x, s32 y, s32 *cx, s32 *cy);
u8 AtlasTerrain_ReliefNoise(s32 x, s32 y, bool8 broad);
bool8 AtlasMountain_Probe(s32 x, s32 y, u8 kind);
bool8 AtlasMountain_Foothill(s32 x, s32 y, const struct AtlasCell *cell);
u16 AtlasTiles_Describe(s32 x, s32 y, u16 *floor);
u16 AtlasTiles_Render(u16 floor, const u16 *cells);
bool8 AtlasWater_Prewarm(s32 x, s32 y);
bool8 AtlasStream_CanStep(u8 direction);
extern u32 gAtlasStreamWaits;
#endif
