#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "constants/metatile_labels.h"

/* Geographic generation 1 for box 3, not the missing historical generator. */
struct NoiseCache {
    s32 x0, x1, y0, y1;
    s32 lastX, lastY;
    u32 rawX, rawY, remX, remY;
    u16 a, b, c, d;
    bool8 valid;
};
EWRAM_DATA static struct NoiseCache sNoise[9] = {{0}};
void AtlasTerrain_SetSeed(u32 seed)
{
    gAtlasWorldSeed = seed;
    memset(sNoise, 0, sizeof(sNoise));
    AtlasTiles_Reset();
    AtlasPlan_ResetSample();
}
static const s32 sScales[9] = {2000000, 350000, 160000, 14000, 600000, 1200, 96, 128, 1024};
s32 AtlasTerrain_FloorDiv(s32 n, s32 d)
{
    s32 q = n / d;
    return n % d < 0 ? q - 1 : q;
}
static s32 Wrap(s32 n)
{
    n %= ATLAS_WORLD_WIDTH;
    return n < 0 ? n + ATLAS_WORLD_WIDTH : n;
}
static u32 Hash(s32 x, s32 y, u32 salt)
{
    u32 h = gAtlasWorldSeed ^ salt ^ (u32)x * 0x9E3779B1u ^ (u32)y * 0x85EBCA77u;
    h ^= h >> 16; h *= 0x7FEB352Du;
    h ^= h >> 15; h *= 0x846CA68Bu;
    return h ^ (h >> 16);
}
static u32 Smooth(u32 f) { return (f * f * (768 - 2 * f)) >> 16; }
static s32 Noise(s32 x, s32 y, u32 channel)
{
    struct NoiseCache *n = &sNoise[channel];
    s32 scale = sScales[channel];
    u32 fx, fy, top, bottom;
    if (!n->valid || x < n->x0 || x >= n->x1 || y < n->y0 || y >= n->y1)
    {
        s32 gx = x / scale, gy = AtlasTerrain_FloorDiv(y, scale), next;
        n->x0 = gx * scale; n->x1 = n->x0 + scale;
        if (n->x1 > ATLAS_WORLD_WIDTH) n->x1 = ATLAS_WORLD_WIDTH;
        n->y0 = gy * scale; n->y1 = n->y0 + scale;
        next = n->x1 == ATLAS_WORLD_WIDTH ? 0 : gx + 1;
        n->a = Hash(gx, gy, channel + 1) & 255;
        n->b = Hash(next, gy, channel + 1) & 255;
        n->c = Hash(gx, gy + 1, channel + 1) & 255;
        n->d = Hash(next, gy + 1, channel + 1) & 255;
        n->lastX = -2; n->lastY = -0x7FFFFFFF;
        n->rawX = n->rawY = n->remX = n->remY = 0;
        n->valid = TRUE;
    }
    if (x == n->lastX + 1)
    {
        n->remX += 256;
        while (n->remX >= (u32)(n->x1 - n->x0)) { n->rawX++; n->remX -= n->x1 - n->x0; }
    }
    else if (x != n->lastX)
    {
        u32 v = (u32)(x - n->x0) * 256;
        n->rawX = v / (u32)(n->x1 - n->x0); n->remX = v - n->rawX * (n->x1 - n->x0);
    }
    if (y == n->lastY + 1)
    {
        n->remY += 256;
        while (n->remY >= (u32)scale) { n->rawY++; n->remY -= scale; }
    }
    else if (y != n->lastY)
    {
        u32 v = (u32)(y - n->y0) * 256;
        n->rawY = v / (u32)scale; n->remY = v - n->rawY * scale;
    }
    n->lastX = x; n->lastY = y;
    fx = Smooth(n->rawX); fy = Smooth(n->rawY);
    top = (n->a * (256 - fx) + n->b * fx) >> 8;
    bottom = (n->c * (256 - fx) + n->d * fx) >> 8;
    return (top * (256 - fy) + bottom * fy) >> 8;
}
static u8 Clamp(s32 n) { return n < 0 ? 0 : n > 255 ? 255 : n; }
u8 AtlasTerrain_ReliefNoise(s32 x,s32 y,bool8 broad)
{
    return Noise(Wrap(x),y,broad?8:7);
}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *plan)
{
    x=Wrap(x);
    if(y< -ATLAS_WORLD_HALF_HEIGHT)y= -ATLAS_WORLD_HALF_HEIGHT;
    if(y>=ATLAS_WORLD_HALF_HEIGHT)y=ATLAS_WORLD_HALF_HEIGHT-1;
    AtlasPlan_SampleDetail(x,y,plan);
    return plan->lake?plan->height:Clamp(plan->height+(Noise(x,y,3)-128)/16);
}
void AtlasTerrain_Sample(s32 x, s32 y, struct AtlasCell *cell)
{
    s32 e, rain, vapor, ridge, t, m;
    struct AtlasHabitat habitat;
    u8 water = ATLAS_DRY, surface = 0;
    if (!cell) return;
    cell->valley = 255;
    x = Wrap(x);
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
    {
        memset(cell, 0, sizeof(*cell)); cell->moisture = cell->humidity = 255;
        cell->biome = ATLAS_OCEAN; cell->waterform = ATLAS_SEA; return;
    }
    if (gAtlasPlanReady)
    {
        struct AtlasPlanSample plan;
        AtlasPlan_SampleDetail(x,y,&plan);
        e = plan.lake ? plan.height : Clamp(plan.height + (Noise(x,y,3)-128)/16);
        rain = Clamp(plan.rain + (Noise(x, y, 4) - 128) / 8); vapor = plan.humidity;
        water = AtlasWater_Sample(x, y, e, &plan, &surface, &cell->valley);
        if (water != ATLAS_DRY) e = surface;
    }
    else
    {
        e = (Noise(x, y, 0) * 3 + Noise(x, y, 1)) / 4;
        ridge = Noise(x, y, 2) - 128;
        if (ridge < 0) ridge = -ridge;
        ridge = 255 - ridge * 2;
        if (e > 132 && ridge > 140) e += (ridge - 140) * (e - 132) / 150;
        e = Clamp(e + (Noise(x, y, 3) - 128) / 12);
        rain = vapor = Noise(x, y, 4);
        if (e < ATLAS_PLAN_SEA) water = ATLAS_SEA;
    }
    AtlasHabitat_Sample(y, e, rain, vapor, water, &habitat);
    t = habitat.temperature; m = habitat.moisture;
    cell->elevation = e; cell->temperature = t; cell->moisture = m;
    cell->humidity = habitat.humidity; cell->seasonalRange = habitat.seasonalRange; cell->waterform = water;
    cell->landform = e < 128 && e >= 121 ? ATLAS_COAST : e > 185 ? ATLAS_MOUNTAIN : e > 155 ? ATLAS_HILL : ATLAS_PLAIN;
    if (water == ATLAS_SEA) cell->biome = ATLAS_OCEAN;
    else if (water != ATLAS_DRY) cell->biome = ATLAS_WETLAND;
    else if (t < 45) cell->biome = ATLAS_TUNDRA;
    else if (t < 85) cell->biome = ATLAS_TAIGA;
    else if (e < 132 && m > 160) cell->biome = ATLAS_WETLAND;
    else if (t > 160 && m < 98) cell->biome = ATLAS_DESERT;
    else if (t > 175 && m > 155) cell->biome = ATLAS_JUNGLE;
    else if (m > 132) cell->biome = ATLAS_FOREST;
    else if (t > 150 && m < 125) cell->biome = ATLAS_SAVANNA;
    else cell->biome = ATLAS_GRASSLAND;
}

u16 AtlasTerrain_BaseTile(s32 x, s32 y, const struct AtlasCell *c, bool8 clearCover)
{
    u16 id;
    u32 detail;
    u8 variant;
    if (c->waterform != ATLAS_DRY)
        return PACK_METATILE(AtlasTiles_Water(255, 0, c->waterform)) | PACK_COLLISION(1) | PACK_ELEVATION(3);
    x = Wrap(x);
    /* Stateless coordinate detail; no new lattice, cache or geographic query.
     * The source's three tuft variants share the same flat grass/snow border. */
    detail = Hash(x, y, 0x47524E44);
    variant = (detail & 15) < 4 ? detail & 3 : 0;
    id = ATLAS_NATURE_GRASS_FIRST + variant;
    if (c->biome == ATLAS_TUNDRA) id = ATLAS_NATURE_SNOW_FIRST + variant;
    else if (c->biome == ATLAS_DESERT || c->landform == ATLAS_COAST) id = ATLAS_NATURE_SAND_FIRST + variant;
    else if (!clearCover && (c->biome == ATLAS_JUNGLE || c->biome == ATLAS_FOREST || c->biome == ATLAS_TAIGA || c->biome == ATLAS_GRASSLAND))
    {
        s32 threshold = c->biome == ATLAS_JUNGLE ? 125 : c->biome == ATLAS_TAIGA ? 165 : c->biome == ATLAS_FOREST ? 145 : 160;
        /* Reuse the existing 96-tile cover field. Coordinate dither breaks
         * solid rectangles inside a coherent patch, rather than filling a
         * whole local ridge with native encounter-grass artwork. */
        if (Noise(x, y, 6) > threshold + (s32)((detail >> 4) & 31)) id = METATILE_General_TallGrass;
    }
    else if (!clearCover && c->biome == ATLAS_WETLAND)
    {
        if ((detail & 7) < 5) id = METATILE_General_TallGrass;
    }
    else if (!clearCover && c->biome == ATLAS_SAVANNA && Noise(x, y, 6) < 85) id = ATLAS_NATURE_SAND_FIRST + variant;

    // Reuse pass stones as sparse, walkable high-ground scree. Preserve tall
    // grass, sand, water and every collision/elevation bit; no new sampler.
    if (c->elevation>=175 && ((detail>>16)&63)==0
     && ((id>=ATLAS_NATURE_GRASS_FIRST && id<ATLAS_NATURE_GRASS_FIRST+4)
      || (id>=ATLAS_NATURE_SNOW_FIRST && id<ATLAS_NATURE_SNOW_FIRST+4)))
        id=ATLAS_NATURE_PASS_FIRST+(c->biome==ATLAS_TUNDRA)+(((detail>>22)&1)<<1);

    return PACK_METATILE(id) | PACK_ELEVATION(3);
}
u16 AtlasTerrain_Tile(s32 x, s32 y)
{
    struct AtlasCell c;
    AtlasTerrain_Sample(x, y, &c);
    return AtlasTiles_At(x, y, &c);
}
bool8 AtlasTerrain_Fill(u16 *map, s32 width, s32 height, s32 chunkX, s32 chunkY)
{
    s32 y, bx, by;
    if (!map || width != 35 || height != 34
        || chunkX < -ATLAS_WORLD_WIDTH / 20 || chunkX >= ATLAS_WORLD_WIDTH / 20
        || chunkY < -ATLAS_WORLD_HALF_HEIGHT / 20 || chunkY >= ATLAS_WORLD_HALF_HEIGHT / 20)
        return FALSE;
    bx = chunkX * 20 - 7; by = chunkY * 20 - 7;
    for (y = 0; y < height; y++)
        AtlasTiles_FillRow(map + y * width, bx, by + y, width);
    return TRUE;
}
bool8 AtlasTerrain_FindSpawn(s32 *x, s32 *y)
{
    s32 i, dx, dy, wx, wy;
    if (!x || !y) return FALSE;
    /* Prefer existing riverbanks so the new geography can be inspected on foot. */
    if (gAtlasPlanReady)
    {
        /* Normalize the seed offset before adding i: u32 wrap would skip nodes.
         * The bounded index is a full permutation of the 96x48 plan. */
        u16 start = Hash(0,0,123) % ATLAS_PLAN_COUNT;
        for (i = 0; i < ATLAS_PLAN_COUNT; i++)
        {
            u16 node = (i + start) % ATLAS_PLAN_COUNT;
            u16 width, level; s32 ax,ay,bx,by;
            if (gAtlasPlan[node].height < ATLAS_PLAN_SEA || gAtlasPlan[node].runoff < 600 || gAtlasPlan[node].runoff > 6000) continue;
            /* AtlasWater_Point reads the parent node; skip malformed plan links. */
            if (gAtlasPlan[node].parent >= ATLAS_PLAN_COUNT) continue;
            width = 24 + gAtlasPlan[node].runoff / 128; if (width > 400) width = 400;
            AtlasWater_Point(node,1,&ax,&ay,&level); AtlasWater_Point(node,3,&bx,&by,&level);
            {
                struct AtlasCell cell; bool8 safe = TRUE;
                AtlasWater_Point(node,2,&wx,&wy,&level);
                if (bx - ax > by - ay && bx - ax > ay - by) wy += (width + 6);
                else if (ax - bx > by - ay && ax - bx > ay - by) wy += (width + 6);
                else wx += (width + 6);
                wx = Wrap(wx); wx = AtlasTerrain_FloorDiv(wx,20)*20+10; wy = AtlasTerrain_FloorDiv(wy,20)*20+10;
                AtlasTerrain_Sample(wx,wy,&cell);
                if (cell.temperature < 85 || cell.waterform != ATLAS_DRY || cell.elevation > 185) continue;
                for (dy=-4;dy<=4&&safe;dy++)for(dx=-4;dx<=4;dx++)
                    if(AtlasTerrain_Tile(wx+dx,wy+dy)&0xC00){safe=FALSE;break;}
                for(dx=0;dx<=25&&safe;dx++)if(AtlasTerrain_Tile(wx+dx,wy)&0xC00)safe=FALSE;
                if(safe){*x=wx;*y=wy;return TRUE;}
            }
        }
    }
    for (i = 0; i < 4096; i++)
    {
        struct AtlasCell cell;
        bool8 safe = TRUE;
        wx = 33453850 + (i % 64) * 223; wy = 4307290 + (i / 64) * 197;
        AtlasTerrain_Sample(wx, wy, &cell);
        if (cell.biome == ATLAS_OCEAN || cell.biome == ATLAS_DESERT || cell.landform == ATLAS_MOUNTAIN) continue;
        for (dy = -4; dy <= 4 && safe; dy++)
            for (dx = -4; dx <= 4; dx++)
                if (AtlasTerrain_Tile(wx + dx, wy + dy) & 0x0C00) { safe = FALSE; break; }
        if (safe) { *x = wx; *y = wy; return TRUE; }
    }
    /* Global fallback surveys real land; no artificial starting terrain. */
    for (i = 0; i < 4096; i++)
    {
        wx = (s32)(Hash(i, 0, 99) % ATLAS_WORLD_WIDTH);
        wy = (s32)(Hash(i, 1, 99) % (2u * ATLAS_WORLD_HALF_HEIGHT)) - ATLAS_WORLD_HALF_HEIGHT;
        {
            bool8 safe = TRUE;
            for (dy = -4; dy <= 4 && safe; dy++)
                for (dx = -4; dx <= 4; dx++)
                    if (AtlasTerrain_Tile(wx + dx, wy + dy) & 0x0C00) { safe = FALSE; break; }
            if (safe) { *x = wx; *y = wy; return TRUE; }
        }
    }
    return FALSE;
}

/* Terrain revisions can cover an old standing tile with a new small channel.
 * Reject wet candidates with one sample instead of compiling nine neighbors. */
bool8 AtlasTerrain_RepairLanding(s32 *x,s32 *y)
{
    s32 dx,dy,r,baseX; s64 yy; struct AtlasCell c;
    if(!x || !y)return FALSE;
    if(!(AtlasTerrain_Tile(*x,*y)&0xC00))return TRUE;
    /* Normalize before adding the search offset: saved x may be extreme. */
    baseX=Wrap(*x);
    for(r=1;r<=32;r++)for(dy=-r;dy<=r;dy++)for(dx=-r;dx<=r;dx++)
    {
        if(dx!=-r && dx!=r && dy!=-r && dy!=r)continue;
        /* Widen before addition; a malformed old latitude must not overflow. */
        yy=(s64)*y+dy;
        if(yy<=-ATLAS_WORLD_HALF_HEIGHT || yy>=ATLAS_WORLD_HALF_HEIGHT)continue;
        AtlasTerrain_Sample(baseX+dx,(s32)yy,&c);
        if(c.waterform || (AtlasTerrain_Tile(baseX+dx,(s32)yy)&0xC00))continue;
        *x=Wrap(baseX+dx);*y=(s32)yy;return TRUE;
    }
    /* An isthmus can disappear into connected water on both sides. Prefer a
     * verified spawn over leaving an older save immobilized inside that water. */
    return AtlasTerrain_FindSpawn(x,y);
}
