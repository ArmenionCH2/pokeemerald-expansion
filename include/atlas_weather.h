#ifndef GUARD_ATLAS_WEATHER_H
#define GUARD_ATLAS_WEATHER_H
struct AtlasSaveState;
/* A smooth, deterministic regional potential. Low values favor precipitation.
 * Saved simulation time advects fronts; querying never advances that time. */
u8 AtlasWeather_Potential(const struct AtlasSaveState *state, s32 x, s32 y);
#endif
