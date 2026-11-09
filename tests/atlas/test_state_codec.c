#include "global.h"
#include "atlas_world.h"
#include "atlas_state.h"
#include <assert.h>
#include <stdio.h>

static void RandomCoverage(void)
{
    struct AtlasSaveState state;
    unsigned i,c,t;
    u32 rng=123;
    u8 expected[25][50]={{0}};
    AtlasState_Init(&state,123);
    for (i=0;i<20000;i++)
    {
        rng=rng*1664525u+1013904223u;c=(rng>>16)%25;
        rng=rng*1664525u+1013904223u;t=(rng>>16)%400;
        assert(AtlasState_Visit(&state,c*20+t%20,t/20));
        expected[c][t/8]|=1<<(t%8);
    }
    assert(AtlasState_Valid(&state,123));
    assert(AtlasState_UsedBytes(&state)<=ATLAS_JOURNAL_BYTES);
    for (c=0;c<25;c++) for (t=0;t<400;t++)
        assert(AtlasState_Explored(&state,c*20+t%20,t/20)==!!(expected[c][t/8]&(1<<(t%8))));
}
static void LongWalks(void)
{
    struct AtlasSaveState state,before;
    unsigned c,t;
    AtlasState_Init(&state,123);
    for (c=0;c<ATLAS_JOURNAL_MAX_CHUNKS;c++)
        for (t=0;t<20;t++) assert(AtlasState_Visit(&state,c*20+7,t));
    assert(state.count==130 && AtlasState_UsedBytes(&state)==1430);
    assert(AtlasState_Valid(&state,123));
    before=state;
    assert(!AtlasState_Visit(&state,4000,4000));
    assert(!memcmp(&state,&before,sizeof(state)));
    for (c=0;c<130;c++) for (t=0;t<20;t++)
    {
        assert(AtlasState_Explored(&state,c*20+7,t));
        assert(!AtlasState_Explored(&state,c*20+6,t));
    }
    // Duplicate coordinates and unsupported descriptors fail validation.
    AtlasState_Init(&state,123);assert(AtlasState_Visit(&state,0,0));
    unsigned second=AtlasState_UsedBytes(&state);
    assert(AtlasState_Visit(&state,20,0));
    memcpy(state.journal+second,state.journal,6);
    assert(!AtlasState_Valid(&state,123));
}
static void LegacyUpgrade(void)
{
    struct AtlasSaveState state,legacy,invalid;
    struct AtlasVisitedChunk chunk;
    unsigned c,t;
    memset(&state,0,sizeof(state));
    state.magic=ATLAS_STATE_MAGIC;state.seed=123;state.version=1;
    state.seconds=77;state.frames=59;state.count=24;
    for (c=0;c<24;c++)
    {
        state.visited[c].x=c*777;
        state.visited[c].y=-500075+c*123;
        for (t=0;t<400;t++) // checkerboard remains fragmented in either orientation
            if (!((t/20+t%20)&1)) state.visited[c].tiles[t/8]|=1<<(t%8);
    }
    legacy=state;
    assert(AtlasState_Valid(&state,123));assert(AtlasState_Upgrade(&state));
    assert(sizeof(state)==1456 && state.version==2 && state.count==24);
    assert(state.seconds==77 && state.frames==59 && state.seed==123);
    assert(AtlasState_UsedBytes(&state)==24*57 && AtlasState_Valid(&state,123));
    for (c=0;c<24;c++)
    {
        assert(AtlasState_GetChunk(&state,c,&chunk));
        assert(chunk.x==legacy.visited[c].x && chunk.y==legacy.visited[c].y);
        assert(!memcmp(chunk.tiles,legacy.visited[c].tiles,50));
        for (t=0;t<400;t++) assert(AtlasState_Explored(&state,chunk.x*20+t%20,chunk.y*20+t/20)==!((t/20+t%20)&1));
    }
    invalid=legacy;invalid.version=99;legacy=invalid;
    assert(!AtlasState_Upgrade(&invalid));assert(!memcmp(&invalid,&legacy,sizeof(invalid)));
    invalid=state;invalid.journal[6]=128;legacy=invalid;
    assert(!AtlasState_Upgrade(&invalid));assert(!memcmp(&invalid,&legacy,sizeof(invalid)));
}
int main(void)
{
    RandomCoverage();LongWalks();LegacyUpgrade();
    puts("PASS lossless row/column/bitmap journal, 130-chunk walks, overflow atomicity and exact version-1 upgrade");
    return 0;
}
