#ifndef GUARD_ATLAS_COAST_H
#define GUARD_ATLAS_COAST_H
struct AtlasPlanSample;
/* Ordinal exposure-driven coastal uplift; never raises original sea floor. */
u8 AtlasCoast_Rise(s32 x,s32 y,const struct AtlasPlanSample *plan,u8 ground);
#endif
