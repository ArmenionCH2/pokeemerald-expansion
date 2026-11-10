#include "global.h"
#include "atlas_world.h"
#include "atlas_tiles.h"
#include "atlas_vegetation.h"
#include "atlas_volcano.h"
#include "constants/metatile_labels.h"

/* Compact semantics: geographic height, relief/pass, material and water type.
 * The streaming cache owns persistence; direct queries use the same selector. */
void AtlasTiles_Reset(void) {}
u16 AtlasTiles_Water(u8 mask, u8 material, u8 waterform)
{
    /* Full-water graphics are identical across bank materials. Share their
     * native animated role; slot 887 now holds a sand ground border. */
    if (mask == 255) return waterform == ATLAS_SEA ? 746 : 558;
    return ATLAS_NATURE_WATER_FIRST + (waterform == ATLAS_SEA ? 188 : 0) + material * 47 + gAtlasNatureMaskIndex[mask];
}
static u8 Material(const struct AtlasCell *c)
{
    if (c->biome == ATLAS_TUNDRA) return 3;
    if (c->elevation > 205 || c->landform==ATLAS_MESA
     || (c->landform==ATLAS_ROCK_COAST && c->moisture<110)) return 2;
    if (c->landform==ATLAS_DUNE || c->landform==ATLAS_DUNE_CREST || c->biome == ATLAS_DESERT || (c->landform == ATLAS_COAST && c->biome != ATLAS_WETLAND)) return 1;
    return 0;
}
static u16 Describe(s32 x, s32 y, const struct AtlasCell *c, u16 *floor)
{
    u8 relief = AtlasMountain_Relief(x, y, c);
    *floor = AtlasTerrain_BaseTile(x, y, c, !!(relief & 0x80));
    u8 material=Material(c);
    if(c->biome!=ATLAS_TUNDRA && AtlasVolcano_Bare(x,y))material=2;
    return ((u16)c->elevation << 8) | relief | (material << 2) | c->waterform;
}
u16 AtlasTiles_Describe(s32 x, s32 y, u16 *floor)
{
    struct AtlasCell c;
    AtlasTerrain_Sample(x, y, &c);
    return Describe(x, y, &c, floor);
}
static u16 Ground(u16 floor, const u16 *cells)
{
    static const u8 sides[4] = {1, 5, 7, 3}; /* N E S W */
    u8 i, mask = 0, sandMask=0, snowMask=0, green=0, sand=0, snow=0;
    if (!AtlasTiles_GroundEdge(floor, cells[4])) return floor;
    for (i = 0; i < 4; i++)
    {
        u16 neighbor = cells[sides[i]];
        if ((neighbor & 3) || (neighbor & 12)) mask |= 1 << i;
        if(!(neighbor&3) && ((neighbor>>2)&3)==1)sand=1;
        else sandMask|=1<<i;
        if(!(neighbor&3) && ((neighbor>>2)&3)==3)snow=1;
        else snowMask|=1<<i;
        if(!(neighbor&3) && !(neighbor&12))green=1;
    }
    u8 material=(cells[4]>>2)&3;
    /* Preserve established green outlines and rock/sand priority at mixed
     * junctions. Water continues the floor; dry two-material snowlines round. */
    if(material==2 && snow && !sand && !green)
        return (floor&~1023)|(ATLAS_ROCK_SNOW_EDGE_FIRST+snowMask);
    if(material==2 && sand && !green)
        return (floor&~1023)|(ATLAS_ROCK_SAND_EDGE_FIRST+sandMask);
    if (mask == 15) return floor;
    if(material>=2)return (floor&~1023)|((material==3?ATLAS_SNOW_GROUND_EDGE_FIRST:ATLAS_ROCK_GROUND_EDGE_FIRST)+mask);
    return (floor & ~1023) | (mask == 14 ? ATLAS_NATURE_SAND_EDGE_NORTH : ATLAS_NATURE_SAND_EDGE_FIRST + mask);
}
u16 AtlasTiles_Render(u16 floor, const u16 *cells)
{
    if (AtlasVegetation_IsTree(floor)) floor=AtlasVegetation_Result(floor,FALSE);
    static const u8 neighbors[8] = {1,5,7,3,2,8,6,0};
    u16 center = cells[4];
    u8 water = center & 3, tier = (center >> 4) & 7;
    u8 i, mask = 0, material = 0, count[4] = {0};
    if (!water)
    {
        if(AtlasWorld_Generation()>=23 && tier==7)
        {
            if(center&0x80)return Ground(floor,cells);
            for(i=0;i<8;i++)
            {
                u16 next=cells[neighbors[i]];
                /* Actual height contours descend toward the closed floor.
                 * River/lake openings remain owned by hydrology. */
                if((next&3) || (next>>8)/8>=(center>>8)/8)mask|=1<<i;
            }
            if(mask==255)return Ground(floor,cells);
            u16 first=((center>>2)&3)==3?ATLAS_NATURE_MOUNTAIN_FIRST+47:ATLAS_ROCK_CLIFF_FIRST;
            return PACK_METATILE(first+gAtlasNatureMaskIndex[mask])|PACK_COLLISION(1)|PACK_ELEVATION(3);
        }
        if(AtlasWorld_Generation()>=22 && tier==6)
        {
            for(i=0;i<8;i++)
            {
                u16 next=cells[neighbors[i]];
                /* Only the seaward face blocks walking. Inland shoulders stay
                 * traversable and do not acquire a false enclosing cliff. */
                if((next&3)==ATLAS_SEA && (center>>8)>=(next>>8)+8)continue;
                mask|=1<<i;
            }
            if(mask==255)return Ground(floor,cells);
            u16 first=((center>>2)&3)==3?ATLAS_NATURE_MOUNTAIN_FIRST+47:ATLAS_ROCK_CLIFF_FIRST;
            return PACK_METATILE(first+gAtlasNatureMaskIndex[mask])|PACK_COLLISION(1)|PACK_ELEVATION(3);
        }
        if(AtlasWorld_Generation()>=19 && tier>=4)
        {
            if(tier==4)return Ground(floor,cells);
            for(i=0;i<8;i++)
            {
                u16 next=cells[neighbors[i]];
                if(next&3)return Ground(floor,cells);
                unsigned other=(next>>4)&7;
                if((other>=5) || (other<4 && (next>>8)>=(center>>8)))mask|=1<<i;
            }
            if(mask==255)return Ground(floor,cells);
            u16 first=((center>>2)&3)==3?ATLAS_NATURE_MOUNTAIN_FIRST+47:ATLAS_DUNE_EDGE_FIRST;
            return PACK_METATILE(first+gAtlasNatureMaskIndex[mask])|PACK_ELEVATION(3);
        }
        if (center & 0x80)
        {
            u16 id=floor&1023;
            if(id>=ATLAS_ROCK_FLOOR_FIRST && id<ATLAS_ROCK_FLOOR_FIRST+4)return Ground(floor,cells);
            if (id>=ATLAS_NATURE_SAND_FIRST && id<ATLAS_NATURE_SAND_FIRST+4) return Ground(floor,cells);
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
        if (!tier) return Ground(floor,cells);
        for (i = 0; i < 8; i++)
        {
            if (cells[neighbors[i]] & 3) return Ground(floor,cells);
            if (((cells[neighbors[i]] >> 4) & 7) >= tier) mask |= 1 << i;
        }
        if (mask == 255) return Ground(floor,cells);
        u16 first=AtlasWorld_Generation()>=14 && ((center>>2)&3)==2?ATLAS_ROCK_CLIFF_FIRST
            :ATLAS_NATURE_MOUNTAIN_FIRST+(((center>>2)&3)==3?47:0);
        return PACK_METATILE(first+gAtlasNatureMaskIndex[mask])|PACK_COLLISION(1)|PACK_ELEVATION(3);
    }
    for (i = 0; i < 8; i++)
        if (cells[neighbors[i]] & 3) mask |= 1 << i;
        else
        {
            u16 next=cells[neighbors[i]];u8 bank=(next>>2)&3;
            if(AtlasWorld_Generation()>=22 && ((next>>4)&7)==6 && bank!=3)bank=2;
            count[bank]++;
        }
    for (i = 1; i < 4; i++) if (count[i] > count[material]) material = i;
    return PACK_METATILE(AtlasTiles_Water(mask, material, water)) | PACK_COLLISION(1) | PACK_ELEVATION(3);
}
static u16 Tree(s32 x,s32 y,u16 floor)
{
    u16 floors[9],semantics[9];unsigned i;
    u16 part=(floor&1023)-ATLAS_NATURE_TREE_FIRST;
    x-=part%3;y-=part/3;
    for (i=0;i<9;i++) semantics[i]=AtlasTiles_Describe(x+(s32)(i%3),y+(s32)(i/3),&floors[i]);
    return AtlasVegetation_Result(floor,AtlasVegetation_Valid(floors,semantics));
}
u16 AtlasTiles_At(s32 x, s32 y, const struct AtlasCell *center)
{
    u16 cells[9], floor, ignored;
    s32 dx, dy;
    cells[4]=Describe(x,y,center,&floor);
    if (AtlasVegetation_IsTree(floor)) return Tree(x,y,floor);
    for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++)
        if (dx || dy) cells[(dy + 1) * 3 + dx + 1] = AtlasTiles_Describe(x + dx, y + dy, &ignored);
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
        dest[col] = AtlasVegetation_IsTree(floors[col])?Tree(x+col,y,floors[col]):AtlasTiles_Render(floors[col], nine);
    }
}
