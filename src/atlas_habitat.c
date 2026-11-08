#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
static u8 Clamp(s32 n) { return n < 0 ? 0 : n > 255 ? 255 : n; }
EWRAM_DATA static s32 sLatLo = 0, sLatHi = 0, sHeat = 0, sSeason = 0;
EWRAM_DATA static u32 sKey = 0;
EWRAM_DATA static struct AtlasHabitat sHabitat = {0};
EWRAM_DATA static bool8 sValid = 0;
void AtlasHabitat_Sample(s32 y, u8 elevation, u8 rain, u8 vapor, u8 waterform, struct AtlasHabitat *out)
{
    u32 key = elevation | ((u32)rain << 8) | ((u32)vapor << 16) | ((u32)waterform << 24);
    s32 lat = y < 0 ? -y : y;
    s32 temperature;
    s32 moisture, humidity, seasonal;
    if (sValid && lat >= sLatLo && lat <= sLatHi && sKey == key) { *out = sHabitat; return; }
    if (!sValid || lat < sLatLo || lat > sLatHi)
    {
        s32 lo, hi;
        sHeat = lat * 200 / ATLAS_WORLD_HALF_HEIGHT;
        sSeason = lat * 24 / ATLAS_WORLD_HALF_HEIGHT;
        /* The intersection of the two exact integer-quotient intervals.
         * Ceil(q*H/k) through ceil((q+1)*H/k)-1 preserves every boundary;
         * no latitude rounding or approximate climate is introduced. */
        sLatLo = (sHeat * ATLAS_WORLD_HALF_HEIGHT + 199) / 200;
        sLatHi = ((sHeat + 1) * ATLAS_WORLD_HALF_HEIGHT + 199) / 200 - 1;
        lo = (sSeason * ATLAS_WORLD_HALF_HEIGHT + 23) / 24;
        hi = ((sSeason + 1) * ATLAS_WORLD_HALF_HEIGHT + 23) / 24 - 1;
        if (lo > sLatLo) sLatLo = lo;
        if (hi < sLatHi) sLatHi = hi;
    }
    temperature = 238 - sHeat;
    if (elevation > 145) temperature -= (elevation - 145) * 2;
    temperature = Clamp(temperature);
    /* Vapor supply and relative humidity are distinct from precipitation. */
    humidity = (s32)vapor * 255 / (90 + temperature * 2 / 3);
    moisture = (rain * 3 + vapor) / 4 - temperature / 12;
    if (elevation > 170) moisture -= (elevation - 170) / 3;
    seasonal = 3 + sSeason + (230 - vapor) / 12;
    if (waterform != ATLAS_DRY)
    {
        moisture = 255; humidity += 30; seasonal = seasonal * 2 / 3;
    }
    out->temperature = temperature;
    out->humidity = Clamp(humidity);
    out->moisture = Clamp(moisture);
    out->seasonalRange = Clamp(seasonal);
    sKey = key; sHabitat = *out; sValid = TRUE;
}
