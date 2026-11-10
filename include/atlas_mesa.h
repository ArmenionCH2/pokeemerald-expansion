#ifndef GUARD_ATLAS_MESA_H
#define GUARD_ATLAS_MESA_H
/* Dry-country flat caps and bounded shoulders, in ordinal height units. */
struct AtlasMesa {s32 x,y;u16 radius,distance;u8 uplift,relief;};
bool8 AtlasMesa_Site(u16 node,struct AtlasMesa *out);
/* Beyond the two-radius influence area, distance is approximate and saturated. */
bool8 AtlasMesa_Region(s32 x,s32 y,struct AtlasMesa *out);
bool8 AtlasMesa_Sample(s32 x,s32 y,struct AtlasMesa *out);
#endif
