#ifndef GUARD_ATLAS_KARST_H
#define GUARD_ATLAS_KARST_H
/* Rare closed surface depressions, not a groundwater/cave simulation. */
struct AtlasKarst {s32 x,y;u16 radius;u8 cut,relief;};
bool8 AtlasKarst_Site(u16 node,struct AtlasKarst *out);
bool8 AtlasKarst_Sample(s32 x,s32 y,struct AtlasKarst *out);
#endif
