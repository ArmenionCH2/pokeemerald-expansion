#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_place.h"
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
     || sPlace.kind!=sAtlasPlaceTracker.previous.kind) return;
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
