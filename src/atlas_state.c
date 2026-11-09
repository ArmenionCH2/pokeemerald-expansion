#include "global.h"
#include "atlas_world.h"
#include "atlas_state.h"

/* Six coordinate bytes + format byte. Runs are inclusive u16 tile intervals;
 * transpose permits both horizontal and vertical walks to use one short run.
 * Fragmented coverage falls back to the original exact 50-byte bitmap. */
static u32 Read24(const u8 *p) { return p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16); }
static void Write24(u8 *p, u32 n) {p[0]=n;p[1]=n>>8;p[2]=n>>16;}
static u16 Read16(const u8 *p) {return p[0] | ((u16)p[1]<<8);}
static void Write16(u8 *p, u16 n) {p[0]=n;p[1]=n>>8;}
static u16 Transpose(u16 n) {return (n%20)*20+n/20;}
static bool8 Bit(const u8 *bits, u16 n) {return !!(bits[n/8] & (1<<(n%8)));}
static bool8 Coordinates(s32 *x, s32 y)
{
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT) return FALSE;
    *x %= ATLAS_WORLD_WIDTH; if (*x < 0) *x += ATLAS_WORLD_WIDTH;
    return TRUE;
}
static s32 ChunkY(s32 y) {return y >= 0 ? y/20 : (y-19)/20;}
static u16 TileIndex(s32 x,s32 y) {return (y-ChunkY(y)*20)*20+x%20;}
static u16 EntrySize(const u8 *data,u16 at)
{
    u8 format, runs;
    u16 size;
    if (at+7 > ATLAS_JOURNAL_BYTES) return 0;
    format=data[at+6];runs=format&127;
    if (runs>12 || format==128) return 0;
    size=7+(runs?runs*4:ATLAS_VISIT_BYTES);
    return at+size<=ATLAS_JOURNAL_BYTES?size:0;
}
static bool8 Decode(const u8 *data,u16 at,struct AtlasVisitedChunk *out)
{
    unsigned i,j;
    u16 previous=0;
    u8 format;
    if (!EntrySize(data,at)) return FALSE;
    out->x=Read24(data+at);out->y=(s32)Read24(data+at+3)-ATLAS_WORLD_HALF_HEIGHT/20;
    if (out->x>=ATLAS_WORLD_WIDTH/20 || out->y < -ATLAS_WORLD_HALF_HEIGHT/20
     || out->y>=ATLAS_WORLD_HALF_HEIGHT/20) return FALSE;
    memset(out->tiles,0,sizeof(out->tiles));
    format=data[at+6];
    if (!format) memcpy(out->tiles,data+at+7,ATLAS_VISIT_BYTES);
    else for (i=0;i<(format&127);i++)
    {
        u16 start=Read16(data+at+7+i*4),end=Read16(data+at+9+i*4);
        if (start>end || end>=400 || (i && start<=previous+1)) return FALSE;
        for (j=start;j<=end;j++)
        {
            u16 bit=format&128?Transpose(j):j;
            out->tiles[bit/8]|=1<<(bit%8);
        }
        previous=end;
    }
    for (i=0;i<ATLAS_VISIT_BYTES;i++) if (out->tiles[i]) return TRUE;
    return FALSE;
}
static u8 Runs(const u8 *bits,bool8 transpose)
{
    u16 i;
    u8 count=0;
    bool8 last=FALSE;
    for (i=0;i<400;i++)
    {
        bool8 current=Bit(bits,transpose?Transpose(i):i);
        if (current && !last && ++count>12) return 13;
        last=current;
    }
    return count;
}
static u16 Encode(const struct AtlasVisitedChunk *chunk,u8 out[57])
{
    u8 row=Runs(chunk->tiles,FALSE),col=Runs(chunk->tiles,TRUE),runs;
    bool8 transpose=col<row,last=FALSE;
    u16 i,start=0,at=7;
    runs=transpose?col:row;
    Write24(out,chunk->x);Write24(out+3,chunk->y+ATLAS_WORLD_HALF_HEIGHT/20);
    if (runs>12)
    {
        out[6]=0;memcpy(out+7,chunk->tiles,ATLAS_VISIT_BYTES);return 57;
    }
    out[6]=runs|(transpose?128:0);
    for (i=0;i<=400;i++)
    {
        bool8 current=i<400 && Bit(chunk->tiles,transpose?Transpose(i):i);
        if (current && !last) start=i;
        if (!current && last) {Write16(out+at,start);Write16(out+at+2,i-1);at+=4;}
        last=current;
    }
    return at;
}
void AtlasState_Init(struct AtlasSaveState *state,u32 seed)
{
    memset(state,0,sizeof(*state));state->magic=ATLAS_STATE_MAGIC;
    state->version=ATLAS_STATE_VERSION;state->seed=seed;
}
u16 AtlasState_UsedBytes(const struct AtlasSaveState *state)
{
    u16 at=0,size;
    unsigned i;
    if (!state) return ATLAS_JOURNAL_BYTES;
    if (state->version==1) return state->count<=ATLAS_JOURNAL_CHUNKS?state->count*sizeof(state->visited[0]):ATLAS_JOURNAL_BYTES;
    if (state->version!=ATLAS_STATE_VERSION || state->count>ATLAS_JOURNAL_MAX_CHUNKS) return ATLAS_JOURNAL_BYTES;
    for (i=0;i<state->count;i++)
    {
        size=EntrySize(state->journal,at);
        if (!size) return ATLAS_JOURNAL_BYTES;
        at+=size;
    }
    return at;
}
bool8 AtlasState_GetChunk(const struct AtlasSaveState *state,u8 index,struct AtlasVisitedChunk *out)
{
    u16 at=0,size;
    unsigned i;
    if (!state || !out || index>=state->count) return FALSE;
    if (state->version==1)
    {
        if (state->count>ATLAS_JOURNAL_CHUNKS) return FALSE;
        *out=state->visited[index];return TRUE;
    }
    if (state->version!=ATLAS_STATE_VERSION || state->count>ATLAS_JOURNAL_MAX_CHUNKS) return FALSE;
    for (i=0;i<index;i++) {size=EntrySize(state->journal,at);if (!size) return FALSE;at+=size;}
    return Decode(state->journal,at,out);
}
bool8 AtlasState_Valid(const struct AtlasSaveState *state,u32 seed)
{
    unsigned i,j;
    if (!state || state->magic!=ATLAS_STATE_MAGIC || state->seed!=seed || state->frames>=60
     || (state->version!=1 && state->version!=ATLAS_STATE_VERSION)
     || state->count>(state->version==1?ATLAS_JOURNAL_CHUNKS:ATLAS_JOURNAL_MAX_CHUNKS)) return FALSE;
    for (i=0;i<state->count;i++)
    {
        struct AtlasVisitedChunk a,b;
        bool8 nonempty=FALSE;
        u16 other=0;
        if (!AtlasState_GetChunk(state,i,&a) || a.x<0 || a.x>=ATLAS_WORLD_WIDTH/20
         || a.y < -ATLAS_WORLD_HALF_HEIGHT/20 || a.y>=ATLAS_WORLD_HALF_HEIGHT/20) return FALSE;
        for (j=0;j<ATLAS_VISIT_BYTES;j++) if (a.tiles[j]) nonempty=TRUE;
        if (!nonempty) return FALSE;
        for (j=0;j<i;j++)
        {
            if (state->version==1) b=state->visited[j];
            else
            {
                b.x=Read24(state->journal+other);
                b.y=(s32)Read24(state->journal+other+3)-ATLAS_WORLD_HALF_HEIGHT/20;
                other+=EntrySize(state->journal,other);
            }
            if (a.x==b.x && a.y==b.y) return FALSE;
        }
    }
    return TRUE;
}
bool8 AtlasState_Upgrade(struct AtlasSaveState *state)
{
    unsigned i;
    u16 at=0;
    if (!state || !AtlasState_Valid(state,state->seed)) return FALSE;
    if (state->version==ATLAS_STATE_VERSION) return TRUE;
    for (i=0;i<state->count;i++)
    {
        struct AtlasVisitedChunk chunk=state->visited[i];
        u8 encoded[57];
        u16 size=Encode(&chunk,encoded);
        // Every encoded legacy entry is <=57 bytes, versus its old 60 bytes.
        memcpy(state->journal+at,encoded,size);at+=size;
    }
    memset(state->journal+at,0,ATLAS_JOURNAL_BYTES-at);
    state->version=ATLAS_STATE_VERSION;return TRUE;
}
bool8 AtlasState_Explored(const struct AtlasSaveState *state,s32 x,s32 y)
{
    unsigned i;
    struct AtlasVisitedChunk chunk;
    u16 at=0,size;
    if (!state || !Coordinates(&x,y)) return FALSE;
    for (i=0;i<state->count;i++)
    {
        if (state->version==ATLAS_STATE_VERSION)
        {
            size=EntrySize(state->journal,at);if (!size) return FALSE;
            if (Read24(state->journal+at)!=(u32)(x/20)
             || (s32)Read24(state->journal+at+3)-ATLAS_WORLD_HALF_HEIGHT/20!=ChunkY(y))
            {at+=size;continue;}
        }
        if (!AtlasState_GetChunk(state,i,&chunk)) return FALSE;
        if (chunk.x==x/20 && chunk.y==ChunkY(y)) return Bit(chunk.tiles,TileIndex(x,y));
    }
    return FALSE;
}
bool8 AtlasState_Visit(struct AtlasSaveState *state,s32 x,s32 y)
{
    struct AtlasVisitedChunk chunk;
    u8 encoded[57];
    u16 at=0,old=0,size,used;
    unsigned i;
    if (!state || state->version!=ATLAS_STATE_VERSION || state->count>ATLAS_JOURNAL_MAX_CHUNKS || !Coordinates(&x,y)) return FALSE;
    used=AtlasState_UsedBytes(state);
    for (i=0;i<state->count;i++)
    {
        old=EntrySize(state->journal,at);
        if (!old) return FALSE;
        if (Read24(state->journal+at)==(u32)(x/20)
         && (s32)Read24(state->journal+at+3)-ATLAS_WORLD_HALF_HEIGHT/20==ChunkY(y))
        {
            if (!Decode(state->journal,at,&chunk)) return FALSE;
            break;
        }
        at+=old;
    }
    if (i==state->count)
    {
        if (state->count==ATLAS_JOURNAL_MAX_CHUNKS) return FALSE;
        memset(&chunk,0,sizeof(chunk));chunk.x=x/20;chunk.y=ChunkY(y);old=0;
    }
    else if (Bit(chunk.tiles,TileIndex(x,y))) return TRUE;
    chunk.tiles[TileIndex(x,y)/8]|=1<<(TileIndex(x,y)%8);
    size=Encode(&chunk,encoded);
    if (used-old+size>ATLAS_JOURNAL_BYTES) return FALSE;
    memmove(state->journal+at+size,state->journal+at+old,used-at-old);
    memcpy(state->journal+at,encoded,size);
    if (size<old) memset(state->journal+used-old+size,0,old-size);
    if (i==state->count) state->count++;
    return TRUE;
}
void AtlasState_Tick(struct AtlasSaveState *state)
{
    if (++state->frames == 60)
    {
        state->frames = 0;
        if (state->seconds != 0xFFFFFFFFu) state->seconds++;
    }
}
