#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_place.h"
#include "atlas_region.h"
#include "atlas_volcano.h"
#include "script.h"
#include "map_name_popup.h"
#include "constants/characters.h"

static EWRAM_DATA struct AtlasPlaceTracker sAtlasPlaceTracker = {0};
static EWRAM_DATA struct AtlasPlace sPlace = {0};
static EWRAM_DATA s32 sX = 0, sY = 0;
static EWRAM_DATA u8 sCadence = 0, sPending = 0;
static EWRAM_DATA bool8 sPositionValid = FALSE;
static EWRAM_DATA u8 sAtlasLocationPopup[ATLAS_PLACE_NAME_BYTES] = {0};

void AtlasLocation_Reset(void)
{
    memset(&sAtlasPlaceTracker, 0, sizeof(sAtlasPlaceTracker));
    sPositionValid = FALSE; sCadence = sPending = 0; sAtlasLocationPopup[0] = EOS;
}

u8 *AtlasLocation_PopupName(u8 *dest)
{
    unsigned i = 0;
    do {dest[i] = sAtlasLocationPopup[i];} while (sAtlasLocationPopup[i++] != EOS);
    return dest;
}

static void Encode(const char *text)
{
    unsigned i;
    for (i = 0; text[i] && i + 1 < sizeof(sAtlasLocationPopup); i++)
    {
        char c = text[i];
        sAtlasLocationPopup[i] = c >= 'A' && c <= 'Z' ? CHAR_A + c - 'A'
                  : c >= 'a' && c <= 'z' ? CHAR_a + c - 'a' : CHAR_SPACE;
    }
    sAtlasLocationPopup[i] = EOS;
}

void AtlasLocation_Tick(void)
{
    s32 x, y;
    unsigned level;
    if (!AtlasWorld_IsActive() || !gAtlasPlanReady || ArePlayerFieldControlsLocked()) return;
    if (++sCadence < 12) return;
    sCadence = 0;
    AtlasWorld_GetPosition(&x, &y);
    if (!sPositionValid || x != sX || y != sY)
    {
        struct AtlasCell cell;
        AtlasTerrain_Sample(x, y, &cell);
        sPositionValid = AtlasPlace_Identify(x, y, &cell, &sPlace);
        if (sPositionValid && cell.waterform==ATLAS_DRY && cell.landform!=ATLAS_COAST)
        {
            struct AtlasVolcano volcano;
            u8 region=AtlasRegion_At(x,y);
            if(cell.landform==ATLAS_BACKSWAMP)sPlace.kind=ATLAS_PLACE_BACKSWAMP;
            else if(cell.landform==ATLAS_LEVEE)sPlace.kind=ATLAS_PLACE_LEVEE;
            else if(cell.landform==ATLAS_CANYON)sPlace.kind=ATLAS_PLACE_CANYON;
            else if(AtlasVolcano_Sample(x,y,&volcano))
            {
                sPlace.kind=volcano.distance<volcano.radius/4?ATLAS_PLACE_CALDERA:ATLAS_PLACE_VOLCANO;
                AtlasPlace_SetLandmark(&sPlace,x,y);
            }
            else if(cell.landform==ATLAS_SINKHOLE)sPlace.kind=ATLAS_PLACE_SINKHOLE;
            else if(cell.landform==ATLAS_ROCK_COAST)sPlace.kind=ATLAS_PLACE_ROCK_COAST;
            else if(cell.landform==ATLAS_MESA)sPlace.kind=ATLAS_PLACE_MESA;
            else if(cell.landform==ATLAS_DUNE || cell.landform==ATLAS_DUNE_CREST)sPlace.kind=ATLAS_PLACE_DUNES;
            else if(AtlasWorld_Generation()>=17 && cell.valley==0 && cell.elevation<155 && cell.moisture>=132)
                sPlace.kind=ATLAS_PLACE_FLOODPLAIN;
            else if (region==ATLAS_REGION_PLATEAU) sPlace.kind=ATLAS_PLACE_PLATEAU;
            else if (region==ATLAS_REGION_VALLEY) sPlace.kind=ATLAS_PLACE_VALLEY;
            else if (region==ATLAS_REGION_RIDGE) sPlace.kind=ATLAS_PLACE_RIDGE;
        }
        if (sPositionValid && cell.waterform==ATLAS_DRY)
        {
            unsigned distance,direction;
            u8 watermask=0;
            // Read the resident stencil only. Even at a water-cache boundary,
            // announcing a shore must not synchronously prepare more terrain.
            for (distance=1;distance<=3 && !watermask;distance++)
            {
                static const s8 offsets[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
                for (direction=0;direction<4;direction++)
                {
                    u16 semantic;
                    if (AtlasStream_ReadSemantics(x+offsets[direction][0]*(s32)distance,
                        y+offsets[direction][1]*(s32)distance,&semantic) && (semantic&3))
                        watermask|=1<<(semantic&3);
                }
            }
            AtlasPlace_ApplyShore(&sPlace,watermask);
            if(sPlace.kind==ATLAS_PLACE_LAKE)AtlasPlace_SetLandmark(&sPlace,x,y);
            if(sPlace.kind==ATLAS_PLACE_COAST && cell.landform==ATLAS_ROCK_COAST)sPlace.kind=ATLAS_PLACE_SEA_CLIFF;
            if(sPlace.kind==ATLAS_PLACE_RIVER && AtlasWater_DistributaryNear(x,y,4))
                sPlace.kind=ATLAS_PLACE_DISTRIBUTARY;
            else if(sPlace.kind==ATLAS_PLACE_RIVER && AtlasWater_CreekNear(x,y,4))
                sPlace.kind=AtlasWater_HeadwaterNear(x,y,4)?ATLAS_PLACE_HEADWATER:ATLAS_PLACE_CREEK;
        }
        sX = x; sY = y;
    }
    if (!sPositionValid) return;
    u8 changed=AtlasPlace_Update(&sAtlasPlaceTracker, &sPlace);
    if (changed && IsMapNamePopupActive()) HideMapNamePopUpWindow();
    sPending |= changed;
    // Do not drain an old area's queued labels while a new destination is
    // being debounced, especially after a developer teleport or map return.
    if (!sAtlasPlaceTracker.initialized
     || sPlace.landmass!=sAtlasPlaceTracker.previous.landmass
     || sPlace.basin!=sAtlasPlaceTracker.previous.basin
     || sPlace.kind!=sAtlasPlaceTracker.previous.kind
     || sPlace.landmark!=sAtlasPlaceTracker.previous.landmark) return;
    if (IsMapNamePopupActive()) return;
    for (level = 0; level < 3; level++)
    {
        char name[ATLAS_PLACE_NAME_BYTES];
        if (!(sPending & (1 << level))) continue;
        sPending &= ~(1 << level);
        if (level == 2 && sAtlasPlaceTracker.previous.kind == ATLAS_PLACE_OCEAN) continue;
        if (!AtlasPlace_Name(&sAtlasPlaceTracker.previous, level, name)) continue;
        Encode(name); ShowMapNamePopup(); return;
    }
}
