#ifndef GUARD_ATLAS_REGION_H
#define GUARD_ATLAS_REGION_H
enum AtlasRegion { ATLAS_REGION_OCEAN, ATLAS_REGION_COAST, ATLAS_REGION_PLAIN,
    ATLAS_REGION_HILLS, ATLAS_REGION_MOUNTAINS, ATLAS_REGION_PLATEAU,
    ATLAS_REGION_VALLEY, ATLAS_REGION_RIDGE, ATLAS_REGION_UNKNOWN };
/* Regional relief across a 400 km stencil, not tile terraces or named components. */
u8 AtlasRegion_Classify(const u8 heights[9]);
u8 AtlasRegion_At(s32 x,s32 y);
#endif
