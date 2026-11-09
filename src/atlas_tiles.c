#include "global.h"
#include "atlas_world.h"
#include "atlas_tiles.h"
#include "constants/metatile_labels.h"

/* Compact semantics: geographic height, relief/pass, material and water type.
 * The streaming cache owns persistence; direct queries use the same selector. */
void AtlasTiles_Reset(void) {}
u16 AtlasTiles_Water(u8 mask, u8 material, u8 waterform)
{
    return ATLAS_NATURE_WATER_FIRST + (waterform == ATLAS_SEA ? 188 : 0) + material * 47 + gAtlasNatureMaskIndex[mask];
}
static u8 Material(const struct AtlasCell *c)
{
    if (c->biome == ATLAS_TUNDRA) return 3;
    if (c->elevation > 205) return 2;
    if (c->biome == ATLAS_DESERT || c->landform == ATLAS_COAST) return 1;
    return 0;
}
static u16 Describe(s32 x, s32 y, const struct AtlasCell *c, u16 *floor)
{
    u8 relief = AtlasMountain_Relief(x, y, c);
    *floor = AtlasTerrain_BaseTile(x, y, c, !!(relief & 0x80));
    return ((u16)c->elevation << 8) | relief | (Material(c) << 2) | c->waterform;
}
u16 AtlasTiles_Describe(s32 x, s32 y, u16 *floor)
{
    struct AtlasCell c;
    AtlasTerrain_Sample(x, y, &c);
    return Describe(x, y, &c, floor);
}
u16 AtlasTiles_Render(u16 floor, const u16 *cells)
{
    static const u8 neighbors[8] = {1,5,7,3,2,8,6,0};
    u16 center = cells[4];
    u8 water = center & 3, tier = (center >> 4) & 7;
    u8 i, mask = 0, material = 0, count[4] = {0};
    if (!water)
    {
        if (center & 0x80)
        {
            u16 id=floor&1023;
            if (id>=ATLAS_NATURE_SAND_FIRST && id<ATLAS_NATURE_SAND_FIRST+4) return floor;
            /* Mark only a connector crossing a terrace change. Flat valley
             * reservations and broad saddle interiors keep their own floor.
             * Reuse sparse floor variants to avoid an artificial dotted line
             * around every contour. Tall grass retains its ground cover.
             * This reads the prepared neighborhood; no terrain resampling. */
            if (!(floor & 3) || (floor & 1023) == METATILE_General_TallGrass) return floor;
            for (i = 0; i < 8; i++)
                if (!(cells[neighbors[i]] & 3) && ((cells[neighbors[i]] >> 4) & 7) != tier)
                    return PACK_METATILE(ATLAS_NATURE_PASS_FIRST + (((center >> 2) & 3) == 3)
                        + ((floor & 1) << 1)) | PACK_ELEVATION(3);
            return floor;
        }
        if (!tier) return floor;
        for (i = 0; i < 8; i++)
        {
            if (cells[neighbors[i]] & 3) return floor;
            if (((cells[neighbors[i]] >> 4) & 7) >= tier) mask |= 1 << i;
        }
        if (mask == 255) return floor;
        return PACK_METATILE(ATLAS_NATURE_MOUNTAIN_FIRST + (((center >> 2) & 3) == 3 ? 47 : 0) + gAtlasNatureMaskIndex[mask]) | PACK_COLLISION(1) | PACK_ELEVATION(3);
    }
    for (i = 0; i < 8; i++)
        if (cells[neighbors[i]] & 3) mask |= 1 << i;
        else count[(cells[neighbors[i]] >> 2) & 3]++;
    for (i = 1; i < 4; i++) if (count[i] > count[material]) material = i;
    return PACK_METATILE(AtlasTiles_Water(mask, material, water)) | PACK_COLLISION(1) | PACK_ELEVATION(3);
}
u16 AtlasTiles_At(s32 x, s32 y, const struct AtlasCell *center)
{
    u16 cells[9], floor, ignored;
    s32 dx, dy;
    for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++)
        if (!dx && !dy) cells[4] = Describe(x, y, center, &floor);
        else cells[(dy + 1) * 3 + dx + 1] = AtlasTiles_Describe(x + dx, y + dy, &ignored);
    return AtlasTiles_Render(floor, cells);
}
void AtlasTiles_FillRow(u16 *dest, s32 x, s32 y, u16 width)
{
    u16 cells[3][37], floors[35], nine[9], ignored, col;
    s32 dy, dx;
    if (!dest || !width || width > 35) return;
    for (dy = 0; dy < 3; dy++) for (col = 0; col < width + 2; col++)
        cells[dy][col] = AtlasTiles_Describe(x + col - 1, y + dy - 1, dy == 1 && col && col <= width ? &floors[col - 1] : &ignored);
    for (col = 0; col < width; col++)
    {
        for (dy = 0; dy < 3; dy++) for (dx = 0; dx < 3; dx++) nine[dy * 3 + dx] = cells[dy][col + dx];
        dest[col] = AtlasTiles_Render(floors[col], nine);
    }
}
