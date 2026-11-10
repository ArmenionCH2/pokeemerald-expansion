#include "global.h"
#include "atlas_world.h"
#include "atlas_vegetation.h"

/* One world-coordinate ring replaces three independently generated chunks.
 * 9,604 bytes: final/native entries and compact geographic semantics.
 * Only the off-screen apron can be pending; an incoming camera strip must be
 * ready before a new step starts. No whole terrain fill on a warm rebase. */
#define SIDE 49
#define HALF 24
#define SAMPLED 0x4000
#define READY 0x8000
EWRAM_DATA static u16 sEntries[SIDE * SIDE] = {0};
EWRAM_DATA static u16 sSemantics[SIDE * SIDE] = {0};
EWRAM_DATA u32 gAtlasCacheHits = 0;
EWRAM_DATA u32 gAtlasCacheMisses = 0;
EWRAM_DATA u32 gAtlasStreamWaits = 0;
EWRAM_DATA static s32 sX = 0, sY = 0, sCenterX = 0, sCenterY = 0, sMapX = 0, sMapY = 0;
EWRAM_DATA static u16 *sMap = NULL;
EWRAM_DATA static u8 sPX = 0, sPY = 0, sEdge = 0;
EWRAM_DATA static u16 sAt = 0;
EWRAM_DATA static s32 sLeft = 0, sRight = 0, sTop = 0, sBottom = 0;
EWRAM_DATA static bool8 sActive = 0;
static s32 Unwrap(s32 x)
{
    if (x - sCenterX > ATLAS_WORLD_WIDTH / 2) x -= ATLAS_WORLD_WIDTH;
    if (x - sCenterX < -ATLAS_WORLD_WIDTH / 2) x += ATLAS_WORLD_WIDTH;
    return x;
}
static void CanonicalizeX(void)
{
    s32 offset;
    if (sCenterX >= 0 && sCenterX < ATLAS_WORLD_WIDTH) return;
    /* Keep keys near the canonical planet after each seam crossing. Moving
     * the keys together preserves physical slots and unfinished frontier
     * work, and prevents coordinates accumulating across repeated laps. */
    offset = sCenterX < 0 ? -ATLAS_WORLD_WIDTH : ATLAS_WORLD_WIDTH;
    sCenterX -= offset; sX -= offset; sMapX -= offset;
    sLeft -= offset; sRight -= offset;
}
static s32 Index(s32 x, s32 y)
{
    s32 dx = x - sX, dy = y - sY;
    if (dx < 0 || dx >= SIDE || dy < 0 || dy >= SIDE) return -1;
    dx += sPX; if (dx >= SIDE) dx -= SIDE;
    dy += sPY; if (dy >= SIDE) dy -= SIDE;
    return dy * SIDE + dx;
}
static bool8 IsReady(s32 x, s32 y)
{
    s32 i = Index(x,y);
    return i >= 0 && (sEntries[i] & READY);
}
bool8 AtlasStream_ReadSemantics(s32 x,s32 y,u16 *out)
{
    s32 index;
    if (!sActive || !out || y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT) return FALSE;
    x%=ATLAS_WORLD_WIDTH;if (x<0) x+=ATLAS_WORLD_WIDTH;
    index=Index(Unwrap(x),y);
    if (index<0 || !(sEntries[index]&READY)) return FALSE;
    *out=sSemantics[index];return TRUE;
}
static void Render(s32 x, s32 y)
{
    s32 index = Index(x,y), dx,dy,i,col,row,c,r;
    u16 cells[9], entry, center;
    if (index < 0 || !(sEntries[index] & SAMPLED) || (sEntries[index] & READY)) return;
    if(x<=sX || x>=sX+SIDE-1 || y<=sY || y>=sY+SIDE-1)return;
    entry=sEntries[index]&0x3FFF;
    center=sSemantics[index];
    if (AtlasVegetation_IsTree(entry))
    {
        u16 floors[9],semantics[9],part=(entry&1023)-ATLAS_NATURE_TREE_FIRST;
        s32 ax=x-part%3,ay=y-part/3;
        for (i=0;i<9;i++)
        {
            s32 at=Index(ax+(s32)(i%3),ay+(s32)(i/3));
            if (at<0 || !(sEntries[at]&SAMPLED)) return;
            floors[i]=sEntries[at]&0x3FFF;semantics[i]=sSemantics[at];
        }
        entry=AtlasVegetation_Result(entry,AtlasVegetation_Valid(floors,semantics));
        goto ready;
    }
    /* Material borders also read the prepared stencil. This adds no sampling;
     * ordinary flat grass still takes the neighbor-free fast path. */
    if((center&3) || ((center&0x80)?(entry&3):(center&0x70)) || AtlasTiles_GroundEdge(entry,center))
    {
        col=sPX+x-sX;if(col>=SIDE)col-=SIDE;
        row=sPY+y-sY;if(row>=SIDE)row-=SIDE;
        for (dy=-1;dy<=1;dy++)
        {
            r=row+dy;if(r<0)r+=SIDE;else if(r>=SIDE)r-=SIDE;
            for(dx=-1;dx<=1;dx++)
            {
                c=col+dx;if(c<0)c+=SIDE;else if(c>=SIDE)c-=SIDE;
                i=r*SIDE+c;
                if(!(sEntries[i]&SAMPLED))return;
                cells[(dy+1)*3+dx+1]=sSemantics[i];
            }
        }
        entry=AtlasTiles_Render(entry,cells);
    }
ready:
    sEntries[index]=entry|SAMPLED|READY;
    if(sMap && x>=sMapX && x<sMapX+35 && y>=sMapY && y<sMapY+34)
        sMap[(y-sMapY)*35+x-sMapX]=entry;
}
void AtlasStream_Reset(void)
{
    memset(sEntries, 0, sizeof(sEntries));
    sActive = FALSE; sMap = NULL; sPX = sPY = 0; sEdge = 4; sAt = 0;
    gAtlasCacheHits = gAtlasCacheMisses = gAtlasStreamWaits = 0;
}
static void Cold(s32 x,s32 y)
{
    s32 dx,dy,index=0;
    u16 floor;
    memset(sEntries, 0, sizeof(sEntries)); sPX = sPY = 0;
    sCenterX = x; sCenterY = y; sX = x-HALF; sY = y-HALF;
    for (dy=0;dy<SIDE;dy++) for(dx=0;dx<SIDE;dx++,index++)
    {
        sSemantics[index] = AtlasTiles_Describe(sX+dx,sY+dy,&floor);
        sEntries[index] = floor | SAMPLED;
    }
    sMap = NULL;
    for(dy=1;dy<SIDE-1;dy++)for(dx=1;dx<SIDE-1;dx++)Render(sX+dx,sY+dy);
    sLeft=sX;sRight=sX+SIDE;sTop=sY;sBottom=sY+SIDE;
    sEdge=4;sAt=0;sActive=TRUE;
}
bool8 AtlasStream_Load(u16 *map, s32 x, s32 y)
{
    s32 dx,dy,bx,row,col,left,right,worldY;
    u16 *source,*end;
    u16 value;
    if (!map || y < -ATLAS_WORLD_HALF_HEIGHT/20 || y >= ATLAS_WORLD_HALF_HEIGHT/20) return FALSE;
    x %= ATLAS_WORLD_WIDTH/20; if(x<0)x+=ATLAS_WORLD_WIDTH/20;
    bx = sActive ? Unwrap(x*20) : x*20;
    if (!sActive || bx+10-sCenterX > 40 || bx+10-sCenterX < -40 || y*20+10-sCenterY > 40 || y*20+10-sCenterY < -40)
    { Cold(bx+10,y*20+10); gAtlasCacheMisses++; }
    else gAtlasCacheHits++;
    sMap = map; sMapX = bx-7; sMapY = y*20-7;
    left=sMapX<sX?sX-sMapX:0;
    right=sX+SIDE-sMapX;if(right>35)right=35;
    for(dy=0;dy<34;dy++)
    {
        worldY=sMapY+dy-sY;
        if(worldY<0 || worldY>=SIDE || left>=right)
        {for(dx=0;dx<35;dx++)map[dy*35+dx]=0x3401;continue;}
        row=sPY+worldY;if(row>=SIDE)row-=SIDE;
        col=sPX+sMapX+left-sX;if(col>=SIDE)col-=SIDE;
        source=sEntries+row*SIDE+col;end=sEntries+(row+1)*SIDE;
        for(dx=0;dx<left;dx++)map[dy*35+dx]=0x3401;
        for(;dx<right;dx++)
        {
            value=*source++;if(source==end)source-=SIDE;
            map[dy*35+dx]=(value&READY)?value&0x3FFF:0x3401;
        }
        for(;dx<35;dx++)map[dy*35+dx]=0x3401;
    }
    return TRUE;
}
static void Shift(s32 x,s32 y)
{
    s32 i, col, row;
    while(sX < x-HALF)
    {
        for(i=0;i<SIDE;i++)sEntries[i*SIDE+sPX]=0;
        if(++sPX==SIDE)sPX=0;
        sX++;
    }
    while(sX > x-HALF)
    {
        if(!sPX)sPX=SIDE;
        sPX--; sX--;
        for(i=0;i<SIDE;i++)sEntries[i*SIDE+sPX]=0;
    }
    while(sY < y-HALF)
    {
        row=sPY*SIDE;for(i=0;i<SIDE;i++)sEntries[row+i]=0;
        if(++sPY==SIDE)sPY=0;
        sY++;
    }
    while(sY > y-HALF)
    {
        if(!sPY)sPY=SIDE;
        sPY--;sY--;
        row=sPY*SIDE;for(col=0;col<SIDE;col++)sEntries[row+col]=0;
    }
    sCenterX=x;sCenterY=y;sEdge=4;sAt=0;
    if(sLeft<sX)sLeft=sX;
    if(sRight>sX+SIDE)sRight=sX+SIDE;
    if(sTop<sY)sTop=sY;
    if(sBottom>sY+SIDE)sBottom=sY+SIDE;
}
static bool8 Job(s32 *x,s32 *y)
{
    s32 distance=0x7FFFFFFF;
    if(sEdge==4)
    {
        if(sLeft>sX){distance=sCenterX-sLeft;sEdge=0;}
        if(sRight<sX+SIDE && sRight-sCenterX<distance){distance=sRight-sCenterX;sEdge=1;}
        if(sTop>sY && sCenterY-sTop<distance){distance=sCenterY-sTop;sEdge=2;}
        if(sBottom<sY+SIDE && sBottom-sCenterY<distance)sEdge=3;
        sAt=0;
    }
    if(sEdge==4)return FALSE;
    if(sEdge<2){*x=sEdge?sRight:sLeft-1;*y=sTop+sAt;}
    else{*x=sLeft+sAt;*y=sEdge==2?sTop-1:sBottom;}
    return TRUE;
}
static void Advance(void)
{
    if(++sAt < (sEdge<2?sBottom-sTop:sRight-sLeft))return;
    if(sEdge==0)sLeft--;
    if(sEdge==1)sRight++;
    if(sEdge==2)sTop--;
    if(sEdge==3)sBottom++;
    sEdge=4;sAt=0;
}
void AtlasStream_Tick(s32 x,s32 y,s32 localX,s32 localY)
{
    s32 wx,wy,index,dx,dy;
    s64 rawX,rawY;
    u16 floor;
    u8 work=8;
    if(!sActive)return;

    /* Saved chunk coordinates can be malformed after a failed restore.
     * Load rejects out-of-range latitude; Tick must not bypass that gate. */
    if(y < -ATLAS_WORLD_HALF_HEIGHT/ATLAS_CHUNK_SIZE ||
       y >= ATLAS_WORLD_HALF_HEIGHT/ATLAS_CHUNK_SIZE)return;
    x %= ATLAS_WORLD_WIDTH/ATLAS_CHUNK_SIZE;
    if(x < 0)x += ATLAS_WORLD_WIDTH/ATLAS_CHUNK_SIZE;

    /* Widen before composing saved chunk and local position. A corrupted
     * local position must not overflow Unwrap or drive a huge Shift loop. */
    rawX=(s64)x*ATLAS_CHUNK_SIZE+localX;
    rawY=(s64)y*ATLAS_CHUNK_SIZE+localY;
    if(rawX < -ATLAS_CHUNK_SIZE ||
       rawX > ATLAS_WORLD_WIDTH+ATLAS_CHUNK_SIZE ||
       rawY < -ATLAS_WORLD_HALF_HEIGHT-ATLAS_CHUNK_SIZE ||
       rawY > ATLAS_WORLD_HALF_HEIGHT+ATLAS_CHUNK_SIZE)return;
    wx=Unwrap((s32)rawX);wy=(s32)rawY;

    /* A valid warm camera rebase is at most 40 tiles from its center.
     * Large jumps must go through Load's bounded cold path, not Shift's
     * per-tile while loops (which can otherwise run millions of times). */
    if(wx-sCenterX > 40 || wx-sCenterX < -40 ||
       wy-sCenterY > 40 || wy-sCenterY < -40)return;
    if(wx!=sCenterX || wy!=sCenterY)Shift(wx,wy);
    CanonicalizeX();
    while(work && Job(&wx,&wy))
    {
        index=Index(wx,wy);
        if(!(sEntries[index]&SAMPLED))
        {
            work--;
            if(!AtlasWater_Prewarm(wx,wy))continue;
            sSemantics[index]=AtlasTiles_Describe(wx,wy,&floor);
            sEntries[index]=floor|SAMPLED;
            for(dy=-2;dy<=2;dy++)for(dx=-2;dx<=2;dx++)Render(wx+dx,wy+dy);
        }
        Advance();
    }
}
bool8 AtlasStream_CanStep(u8 direction)
{
    s32 i,x=sCenterX,y=sCenterY;
    if(!sActive || !direction)return TRUE;
    /* Camera apron may be READY outside the physical latitude domain.
     * It must never authorize an outward player step at either pole. */
    if((direction==1 && y>=ATLAS_WORLD_HALF_HEIGHT-1) ||
       (direction==2 && y<=-ATLAS_WORLD_HALF_HEIGHT))return FALSE;
    if(direction==1)y++;
    if(direction==2)y--;
    if(direction==3)x--;
    if(direction==4)x++;
    if(direction==1 || direction==2)
    {
        y+=direction==1?8:-8;
        for(i=-10;i<=10;i++)if(!IsReady(x+i,y)){gAtlasStreamWaits++;return FALSE;}
    }
    else
    {
        x+=direction==4?10:-10;
        for(i=-8;i<=8;i++)if(!IsReady(x,y+i)){gAtlasStreamWaits++;return FALSE;}
    }
    return TRUE;
}
