#ifndef GUARD_ATLAS_TILES_H
#define GUARD_ATLAS_TILES_H
#define ATLAS_ROCK_SAND_EDGE_FIRST 352
#define ATLAS_ROCK_CLIFF_FIRST 384
#define ATLAS_ROCK_FLOOR_FIRST 431
#define ATLAS_DUNE_EDGE_FIRST 435
#define ATLAS_DUNE_EDGE_COUNT 47
#define ATLAS_ROCK_GROUND_EDGE_FIRST 482
#define ATLAS_SNOW_GROUND_EDGE_FIRST 497
#define ATLAS_MATERIAL_GROUND_EDGE_COUNT 15
#define ATLAS_NATURE_WATER_FIRST 0x200
#define ATLAS_NATURE_WATER_COUNT 375
#define ATLAS_NATURE_GRASS_FIRST 888
#define ATLAS_NATURE_SAND_FIRST 892
#define ATLAS_NATURE_SNOW_FIRST 896
#define ATLAS_NATURE_SAND_EDGE_FIRST 901
#define ATLAS_NATURE_SAND_EDGE_COUNT 14
#define ATLAS_NATURE_SAND_EDGE_NORTH 887
#define ATLAS_NATURE_MARSH_GRASS 900
#define ATLAS_NATURE_TREE_FIRST 915
#define ATLAS_NATURE_OPTION_COUNT 512
#define ATLAS_NATURE_MOUNTAIN_FIRST 924
#define ATLAS_NATURE_PASS_FIRST 1018
#define ATLAS_NATURE_PASS_COUNT 4
#define ATLAS_NATURE_FLOWER_FIRST 1022
#define ATLAS_NATURE_FLOWER_COUNT 2
struct AtlasCell;
/* Material borders read the prepared cardinal stencil. Water continues the
 * dry material rather than introducing a false green fringe. */
static inline bool8 AtlasTiles_SandEdge(u16 floor, u16 semantic)
{
    u16 id = floor & 1023;
    return !(semantic & 3) && ((semantic >> 2) & 3) == 1
        && id >= ATLAS_NATURE_SAND_FIRST && id < ATLAS_NATURE_SAND_FIRST + 4;
}
static inline bool8 AtlasTiles_GroundEdge(u16 floor,u16 semantic)
{
    u16 id=floor&1023,material=(semantic>>2)&3;
    return AtlasTiles_SandEdge(floor,semantic) || (!(semantic&3)
        && ((material==2 && id>=ATLAS_ROCK_FLOOR_FIRST && id<ATLAS_ROCK_FLOOR_FIRST+4)
         || (material==3 && id>=ATLAS_NATURE_SNOW_FIRST && id<ATLAS_NATURE_SNOW_FIRST+4)));
}
static inline bool8 AtlasTiles_IsCliff(u16 id)
{
    return (id>=ATLAS_NATURE_MOUNTAIN_FIRST && id<ATLAS_NATURE_MOUNTAIN_FIRST+94)
        || (id>=ATLAS_ROCK_CLIFF_FIRST && id<ATLAS_ROCK_CLIFF_FIRST+47);
}
extern const u16 gAtlasPrimaryMetatiles[4096],gAtlasPrimaryAttributes[512];
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
