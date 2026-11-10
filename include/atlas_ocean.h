#ifndef GUARD_ATLAS_OCEAN_H
#define GUARD_ATLAS_OCEAN_H
/* Creation-time bathymetry; reads land membership, never changes it. */
u8 AtlasOcean_Height(u16 node);
u8 AtlasOcean_MacroNoise(u16 node,u32 salt);
bool8 AtlasOcean_IslandCandidate(u16 node);
#endif
