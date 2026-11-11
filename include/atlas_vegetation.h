#ifndef GUARD_ATLAS_VEGETATION_H
#define GUARD_ATLAS_VEGETATION_H
#include "atlas_tiles.h"
/* A proposal is never exposed as a partial canopy. Both direct rendering and
 * the streaming ring validate the same complete 3x3 footprint first. */
static inline bool8 AtlasVegetation_IsTree(u16 floor)
{
    u16 id=floor&1023;
    return (id>=ATLAS_NATURE_TREE_FIRST && id<ATLAS_NATURE_TREE_FIRST+9)
        || (id>=ATLAS_ALT_TREE_FIRST && id<ATLAS_ALT_TREE_FIRST+9);
}
static inline u16 AtlasVegetation_First(u16 floor)
{
    u16 id=floor&1023;
    return id>=ATLAS_ALT_TREE_FIRST && id<ATLAS_ALT_TREE_FIRST+9?ATLAS_ALT_TREE_FIRST:ATLAS_NATURE_TREE_FIRST;
}
static inline u16 AtlasVegetation_Part(u16 floor)
{
    return (floor&1023)-AtlasVegetation_First(floor);
}
static inline bool8 AtlasVegetation_Valid(const u16 floors[9],const u16 semantics[9])
{
    unsigned i,low=255,high=0;
    if(!AtlasVegetation_IsTree(floors[0]) || AtlasVegetation_Part(floors[0]))return FALSE;
    u16 first=AtlasVegetation_First(floors[0]);
    for (i=0;i<9;i++)
    {
        unsigned height=semantics[i]>>8;
        if ((floors[i]&1023)!=first+i || (semantics[i]&0xF3)) return FALSE;
        if (height<low) low=height;
        if (height>high) high=height;
    }
    return high-low<=2;
}
/* Lowland hilltops can carry whole crowns only when their complete footprint
 * and one-cell halo are on the same raised surface. Cliff/ramp edges stay clear. */
static inline bool8 AtlasVegetation_HillFootprint(const u16 floors[9],const u16 semantics[9])
{
    if(AtlasWorld_Generation()<27 || !AtlasVegetation_IsTree(floors[0]) || AtlasVegetation_Part(floors[0]))return FALSE;
    unsigned low=255,high=0,first=AtlasVegetation_First(floors[0]);
    for(unsigned i=0;i<9;i++)
    {
        unsigned height=semantics[i]>>8;
        if((floors[i]&1023)!=first+i || (semantics[i]&0xF3)!=0x10 || height>164)return FALSE;
        if(height<low)low=height;
        if(height>high)high=height;
    }
    return high-low<=2;
}
static inline bool8 AtlasVegetation_HillHalo(const u16 halo[25])
{
    for(unsigned i=0;i<25;i++)if((halo[i]&0xF3)!=0x10 || (halo[i]>>8)>164)return FALSE;
    return TRUE;
}
static inline u16 AtlasVegetation_Result(u16 floor,bool8 valid)
{
    return valid?(floor|PACK_COLLISION(1)):(PACK_METATILE(ATLAS_NATURE_GRASS_FIRST)|PACK_ELEVATION(3));
}
#endif
