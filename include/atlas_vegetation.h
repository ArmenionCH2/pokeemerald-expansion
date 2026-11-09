#ifndef GUARD_ATLAS_VEGETATION_H
#define GUARD_ATLAS_VEGETATION_H
#include "atlas_tiles.h"
/* A proposal is never exposed as a partial canopy. Both direct rendering and
 * the streaming ring validate the same complete 3x3 footprint first. */
static inline bool8 AtlasVegetation_IsTree(u16 floor)
{
    u16 id=floor&1023;
    return id>=ATLAS_NATURE_TREE_FIRST && id<ATLAS_NATURE_TREE_FIRST+9;
}
static inline bool8 AtlasVegetation_Valid(const u16 floors[9],const u16 semantics[9])
{
    unsigned i,low=255,high=0;
    for (i=0;i<9;i++)
    {
        unsigned height=semantics[i]>>8;
        if ((floors[i]&1023)!=ATLAS_NATURE_TREE_FIRST+i || (semantics[i]&0xF3)) return FALSE;
        if (height<low) low=height;
        if (height>high) high=height;
    }
    return high-low<=2;
}
static inline u16 AtlasVegetation_Result(u16 floor,bool8 valid)
{
    return valid?(floor|PACK_COLLISION(1)):(PACK_METATILE(ATLAS_NATURE_GRASS_FIRST)|PACK_ELEVATION(3));
}
#endif
