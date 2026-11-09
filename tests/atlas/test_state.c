#include "global.h"
#include "atlas_world.h"
#include "atlas_state.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    struct AtlasSaveState state, before, restored;
    unsigned i;
    AtlasState_Init(&state,123);
    assert(AtlasState_Valid(&state,123));
    assert(!AtlasState_Valid(&state,124));
    assert(AtlasState_Visit(&state,-1,-1));
    assert(AtlasState_Explored(&state,ATLAS_WORLD_WIDTH-1,-1));
    assert(!AtlasState_Explored(&state,ATLAS_WORLD_WIDTH-2,-1));
    assert(!AtlasState_Explored(&state,0,-1));
    assert(AtlasState_Visit(&state,ATLAS_WORLD_WIDTH,0));
    assert(AtlasState_Explored(&state,0,0));
    assert(!AtlasState_Visit(&state,INT_MAX,INT_MIN));
    assert(!AtlasState_Visit(&state,0,ATLAS_WORLD_HALF_HEIGHT));
    assert(AtlasState_Visit(&state,INT_MIN,-ATLAS_WORLD_HALF_HEIGHT));
    for (i=0;i<ATLAS_JOURNAL_MAX_CHUNKS && AtlasState_Visit(&state,i*20,40);i++) {}
    assert(state.count > ATLAS_JOURNAL_CHUNKS);
    before=state;
    assert(!AtlasState_Visit(&state,80000,80000));
    assert(memcmp(&before,&state,sizeof(state))==0);
    assert(AtlasState_Explored(&state,-1,-1));
    for (i=0;i<3600;i++) AtlasState_Tick(&state);
    assert(state.seconds==60 && state.frames==0);
    memcpy(&restored,&state,sizeof(restored));
    assert(AtlasState_Valid(&restored,123));
    assert(AtlasState_Explored(&restored,-1,-1));
    before=state;
    for (i=0;i<1000;i++)
    {
        assert(AtlasState_Weather(&state,-1,0,180,220)==AtlasState_Weather(&restored,ATLAS_WORLD_WIDTH-1,0,180,220));
        assert(!AtlasState_Explored(&state,777,777));
    }
    assert(memcmp(&before,&state,sizeof(state))==0); // Map/weather queries never discover.
    state.seconds=UINT_MAX;state.frames=59;AtlasState_Tick(&state);
    assert(state.seconds==UINT_MAX && state.frames==0);
    restored.version++;assert(!AtlasState_Valid(&restored,123));
    restored=state;restored.count=255;assert(!AtlasState_Valid(&restored,123));
    restored=state;restored.journal[6]=127;assert(!AtlasState_Valid(&restored,123));
    printf("PASS journal exact fog, seam/poles, full capacity, read-only queries, persistence and clock (%zu bytes)\n",sizeof(state));
}
