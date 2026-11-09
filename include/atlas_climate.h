#ifndef GUARD_ATLAS_CLIMATE_H
#define GUARD_ATLAS_CLIMATE_H
/* Macro climate only. Values are ordinal model units, not mm/year or Celsius. */
u8 AtlasClimate_Upwind(u16 row);
void AtlasClimate_Transport(u16 row,u8 height,u8 upstreamHeight,u8 upstreamVapor,
    bool8 dryRow,u8 *vapor,u8 *rain);
#endif
