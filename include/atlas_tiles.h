#ifndef GUARD_ATLAS_TILES_H
#define GUARD_ATLAS_TILES_H
#define ATLAS_NATURE_WATER_FIRST 0x200
#define ATLAS_NATURE_WATER_COUNT 376
#define ATLAS_NATURE_GRASS_FIRST 888
#define ATLAS_NATURE_SAND_FIRST 892
#define ATLAS_NATURE_SNOW_FIRST 896
#define ATLAS_NATURE_CLIFF_FIRST 900
#define ATLAS_NATURE_TREE_FIRST 915
#define ATLAS_NATURE_OPTION_COUNT 510
#define ATLAS_NATURE_MOUNTAIN_FIRST 924
#define ATLAS_NATURE_PASS_FIRST 1018
#define ATLAS_NATURE_PASS_COUNT 4
struct AtlasCell;
extern const u32 gAtlasNatureTiles[4096];
extern const u16 gAtlasNaturePalettes[16][16];
extern const u16 gAtlasNatureMetatiles[4096], gAtlasNatureAttributes[512];
extern const u8 gAtlasNatureMaskIndex[256];
u16 AtlasTiles_Water(u8 mask, u8 material, u8 waterform);
u16 AtlasTiles_At(s32 x, s32 y, const struct AtlasCell *center);
u16 AtlasTerrain_BaseTile(s32 x, s32 y, const struct AtlasCell *cell, bool8 clearCover);
void AtlasTiles_Reset(void);
void AtlasTiles_FillRow(u16 *dest, s32 x, s32 y, u16 width);
#endif
