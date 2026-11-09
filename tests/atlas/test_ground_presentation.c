#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "constants/metatile_labels.h"
#include <assert.h>
#include <stdio.h>
/* Frozen previous renderer: presentation must preserve every native collision
 * and elevation result, including arbitrary overlapping water/pass semantics. */
static u16 PreviousRender(u16 floor, const u16 *cells)
{
    static const u8 neighbors[8] = {1,5,7,3,2,8,6,0};
    u16 center = cells[4];
    u8 water = center & 3, tier = (center >> 4) & 7;
    u8 i, mask = 0, material = 0, count[4] = {0};
    if (!water)
    {
        if (!tier || (center & 0x80)) return floor;
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
static u32 state=0x47524E44;
static u32 Next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void)
{
    struct AtlasCell c;
    u32 seed;
    unsigned i,j,variants[4]={0},snow[4]={0},sand[4]={0},stones=0,tall=0,shortCover=0;
    for(i=0;i<200000;i++)
    {
        u16 cells[9],floor=0x3001;
        for(j=0;j<9;j++)cells[j]=Next()&0xFFFF;
        assert((AtlasTiles_Render(floor,cells)&0xFC00)==(PreviousRender(floor,cells)&0xFC00));
    }
    for(seed=0;seed<2;seed++)
    {
        AtlasTerrain_SetSeed(seed?4220706268u:0);
        memset(&c,0,sizeof(c));c.elevation=200;c.landform=ATLAS_MOUNTAIN;c.valley=255;
        for(i=0;i<4096;i++)
        {
            s32 x=(i%64)*3,y=(s32)(i/64)*3-96;
            u16 a,b;
            c.biome=ATLAS_GRASSLAND;
            a=AtlasTerrain_BaseTile(x,y,&c,FALSE);b=AtlasTerrain_BaseTile(x+ATLAS_WORLD_WIDTH,y,&c,FALSE);assert(a==b);
            AtlasTerrain_BaseTile(999999,123456,&c,FALSE);assert(a==AtlasTerrain_BaseTile(x,y,&c,FALSE));
            assert(!(a&0xC00));
            if((a&1023)==METATILE_General_TallGrass)tall++;
            else if ((a&1023)>=ATLAS_NATURE_PASS_FIRST) stones++;
            else {unsigned v=(a&1023)-ATLAS_NATURE_GRASS_FIRST;assert(v<4);variants[v]++;shortCover++;}
            assert((AtlasTerrain_BaseTile(x,y,&c,TRUE)&1023)!=METATILE_General_TallGrass);
            c.biome=ATLAS_TUNDRA;a=AtlasTerrain_BaseTile(x,y,&c,FALSE);
            assert(a==AtlasTerrain_BaseTile(x-ATLAS_WORLD_WIDTH,y,&c,FALSE));assert(!(a&0xC00));
            if ((a&1023)>=ATLAS_NATURE_PASS_FIRST) stones++;
            else {j=(a&1023)-ATLAS_NATURE_SNOW_FIRST;assert(j<4);snow[j]++;}
            c.biome=ATLAS_DESERT;a=AtlasTerrain_BaseTile(x,y,&c,FALSE);
            assert(!(a&0xC00) && (a>>12)==3);
            j=(a&1023)-ATLAS_NATURE_SAND_FIRST;assert(j<4);sand[j]++;
        }
    }
    for(i=0;i<4;i++)assert(variants[i]&&snow[i]&&sand[i]);
    assert(stones>0 && stones<512);
    // Compare imported sand behavior/layer against the actual native floor.
    FILE *native=fopen("data/tilesets/primary/general/metatile_attributes.bin","rb");assert(native);
    assert(!fseek(native,0x124*2,SEEK_SET));unsigned lo=fgetc(native),hi=fgetc(native);fclose(native);
    assert(lo<256 && hi<256);
    for (i=0;i<4;i++) assert(gAtlasNatureAttributes[ATLAS_NATURE_SAND_FIRST+i-512]==(lo|(hi<<8)));
    assert(tall&&shortCover);
    for(i=0;i<4;i++)
    {
        assert(gAtlasNatureAttributes[ATLAS_NATURE_PASS_FIRST+i-512]==0x1000);
        u16 cells[9]={0},floor=0x3002|(i>>1);
        cells[4]=0xDC90|((i&1)?12:0); /* dry, marked tier-1 cut; neighbor tier-0 */
        assert((AtlasTiles_Render(floor,cells)&1023)==ATLAS_NATURE_PASS_FIRST+i);
        assert(AtlasTiles_Render(0x3000,cells)==0x3000); /* sparse blank variant */
        assert(AtlasTiles_Render(0x3000|METATILE_General_TallGrass,cells)==(0x3000|METATILE_General_TallGrass));
        for (j=0;j<4;j++)
        {
            u16 sandFloor=0x3000|(ATLAS_NATURE_SAND_FIRST+j);
            assert(AtlasTiles_Render(sandFloor,cells)==sandFloor); // no green stone patch on a sand connector
        }
        for(j=0;j<9;j++)cells[j]=cells[4];
        assert(AtlasTiles_Render(floor,cells)==floor); /* uniform interior */
        cells[4]|=ATLAS_RIVER;assert(AtlasTiles_Render(floor,cells)&0xC00);
    }
    printf("PASS presentation: 200000 unchanged collision/elevation neighborhoods; periodic/order-stable grass/snow variants; mixed cover %u/%u; visible connectors, protected water and unmarked flat interiors\n",tall,shortCover);
    return 0;
}
