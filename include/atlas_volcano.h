#ifndef GUARD_ATLAS_VOLCANO_H
#define GUARD_ATLAS_VOLCANO_H
/* Dormant local morphology; ordinal uplift, not metres or an eruption model. */
struct AtlasVolcano {s32 x,y;u16 radius,distance;u8 uplift,relief;};
bool8 AtlasVolcano_Bare(s32 x,s32 y);
bool8 AtlasVolcano_Site(u16 node,struct AtlasVolcano *out);
bool8 AtlasVolcano_Sample(s32 x,s32 y,struct AtlasVolcano *out);
#endif
