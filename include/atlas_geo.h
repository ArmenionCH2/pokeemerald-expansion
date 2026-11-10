#ifndef GUARD_ATLAS_GEO_H
#define GUARD_ATLAS_GEO_H
enum AtlasGeoType { ATLAS_GEO_LAND, ATLAS_GEO_MOUNTAIN, ATLAS_GEO_LAKE, ATLAS_GEO_RIVER, ATLAS_GEO_COAST, ATLAS_GEO_VOLCANO, ATLAS_GEO_RETURN, ATLAS_GEO_RIDGE, ATLAS_GEO_VALLEY, ATLAS_GEO_FOOTHILLS, ATLAS_GEO_MOUTH, ATLAS_GEO_TRIBUTARY, ATLAS_GEO_FOREST, ATLAS_GEO_DESERT, ATLAS_GEO_TUNDRA, ATLAS_GEO_WETLAND, ATLAS_GEO_PLATEAU, ATLAS_GEO_GROVE, ATLAS_GEO_SAND_EDGE, ATLAS_GEO_ISLAND, ATLAS_GEO_ESTUARY, ATLAS_GEO_FLOODPLAIN, ATLAS_GEO_COUNT };
#define ATLAS_GEO_SEARCH_STEPS 4096
struct AtlasGeoResult { s32 x,y; u16 landmass,node; u8 type; };
extern struct AtlasGeoResult gAtlasGeoResult;
extern u8 gAtlasGeoStatus;
void AtlasGeo_Begin(u8 type);
u8 AtlasGeo_Step(void); /* 0 searching, 1 ready, 2 no safe spot, 3 unsupported */
void CB2_AtlasGeoMenu(void);
void AtlasWorld_GetPosition(s32 *x, s32 *y);
void AtlasWorld_Teleport(s32 x, s32 y);
void AtlasWorld_RepairLanding(void);
#endif
