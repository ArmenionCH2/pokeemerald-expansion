#ifndef GUARD_ATLAS_DUNE_H
#define GUARD_ATLAS_DUNE_H
struct AtlasPlanSample;
/* Local, ordinal sand relief. Envelope is 0..128, rise is 0..6. */
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *plan);
u8 AtlasDune_Rise(s32 x,s32 y,const struct AtlasPlanSample *plan);
bool8 AtlasDune_Probe(s32 x,s32 y);
#endif
