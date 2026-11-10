#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_dune.h"

/* Cache only short river pieces intersecting a 4 km tile-query neighborhood. */
#define WATER_BUCKET 4096
#define WATER_MARGIN 480
/* Sixteen candidate owners, each with four base pieces, at most one shore
 * split and one flare. Generations 15–17 retain all 96 possible main pieces.
 * Local creeks add at most 32 pieces: near a macro grid vertex at most four
 * cardinal corridors approach a bucket, each considering six nearby branches.
 * Between vertices at most two adjacent parts approach it. Geometry is culled
 * before validation. New generation21 banks reserve 96 main and 96 local slots;
 * at most 32 local parents and two children apiece fit without evicting main
 * channels. Older generation18-20 admission retains its original 128-piece cap.
 * Generations3-14 retain their original 24-piece admission cap. */
#define WATER_SEGMENTS 192
#define LEGACY_CREEK_SEGMENTS 128
#define HEADWATER_FLAG 0x80
#define CREEK_FLAG 0x8000
#define WATER_T_ONE 16777216
#define COAST_ONE 1048576u
#define COAST_QUARTER (COAST_ONE / 4)
/* widthB's high bit marks a constant-width local creek. Bits 8–14 encode
 * its flat receiving fraction /128. Low bit 0x80 marks generation21 children;
 * TipWidth reads the separate physical width and never treats flags as width. */
struct RiverPiece { s16 ax, ay, bx, by; u16 ha, hb, width, widthB; };
EWRAM_DATA static struct RiverPiece sPieceBank[4][WATER_SEGMENTS] = {{{0}}};
struct CreekLine {s32 ax,ay,bx,by;u16 ha,hb;u8 width,join;};
struct WaterBucket {
    s32 x,y,col,row;u32 stamp,coastLo,coastHi;u16 coastNode;
    u8 count,cursor,coastPhase,coastPart;bool8 valid,ready;
    struct CreekLine creek;
    u8 microSlot,microProbe,microPass,microStart,microEnd;
};
EWRAM_DATA static struct WaterBucket sBuckets[4] = {{0}};
EWRAM_DATA static u8 sActive = 0;
EWRAM_DATA static u32 sClock = 0;
#define sPieces (sPieceBank[sActive])
#define sBucketX (sBuckets[sActive].x)
#define sBucketY (sBuckets[sActive].y)
#define sPieceCount (sBuckets[sActive].count)
static u32 Mix(u32 n)
{
    n ^= n >> 16; n *= 0x7FEB352D; n ^= n >> 15; n *= 0x846CA68B; return n ^ (n >> 16);
}
void AtlasWater_Reset(void) { memset(sBuckets, 0, sizeof(sBuckets)); sActive = 0; sClock = 0; }
static u8 ChannelLevel(u16 node)
{
    unsigned level=gAtlasPlan[node].water;
    if(AtlasWorld_Generation()>=24 && AtlasPlan_CanyonDrain(node) && level>ATLAS_PLAN_SEA+8)
    {
        unsigned cut=(level-ATLAS_PLAN_SEA-8)*3/4;if(cut>24)cut=24;
        level-=cut;
    }
    return level;
}
void AtlasWater_Point(u16 node, u8 part, s32 *x, s32 *y, u16 *height)
{
    u16 parent = gAtlasPlan[node].parent;
    s32 ax = (u32)(node % ATLAS_PLAN_WIDTH) * ATLAS_WORLD_WIDTH / ATLAS_PLAN_WIDTH;
    s32 ay = (node / ATLAS_PLAN_WIDTH) * (2 * ATLAS_WORLD_HALF_HEIGHT / (ATLAS_PLAN_HEIGHT - 1)) - ATLAS_WORLD_HALF_HEIGHT;
    s32 bx = (u32)(parent % ATLAS_PLAN_WIDTH) * ATLAS_WORLD_WIDTH / ATLAS_PLAN_WIDTH;
    s32 by = (parent / ATLAS_PLAN_WIDTH) * (2 * ATLAS_WORLD_HALF_HEIGHT / (ATLAS_PLAN_HEIGHT - 1)) - ATLAS_WORLD_HALF_HEIGHT;
    s32 dx = bx - ax, dy = by - ay, jitter;
    if (dx > ATLAS_WORLD_WIDTH / 2) dx -= ATLAS_WORLD_WIDTH;
    if (dx < -ATLAS_WORLD_WIDTH / 2) dx += ATLAS_WORLD_WIDTH;
    *x = ax + dx * part / 4; *y = ay + dy * part / 4;
    if (part && part < 4)
    {
        jitter = (s32)(Mix(gAtlasWorldSeed ^ node ^ ((u32)part * 0x9E3779B1)) % 257) - 128;
        if (dx) *y += jitter * (ATLAS_WORLD_WIDTH / ATLAS_PLAN_WIDTH / 24) / 128;
        else *x += jitter * (2 * ATLAS_WORLD_HALF_HEIGHT / (ATLAS_PLAN_HEIGHT - 1) / 24) / 128;
    }
    if (*y < -ATLAS_WORLD_HALF_HEIGHT) *y = -ATLAS_WORLD_HALF_HEIGHT;
    if (*y >= ATLAS_WORLD_HALF_HEIGHT) *y = ATLAS_WORLD_HALF_HEIGHT - 1;
    *height = ((u32)ChannelLevel(node) * (4 - part) + (u32)ChannelLevel(parent) * part) * 64;

}
static s32 ClipParameter(s32 distance, s32 delta)
{
    s64 t = (s64)distance * WATER_T_ONE / delta;
    if (t < -WATER_T_ONE) return -WATER_T_ONE;
    if (t > 2 * WATER_T_ONE) return 2 * WATER_T_ONE;
    return t;
}
static bool8 ClipAxis(s32 start, s32 delta, s32 low, s32 high, s32 *first, s32 *last)
{
    s32 a, b, swap;
    if (!delta) return start >= low && start <= high;
    a = ClipParameter(low - start, delta);
    b = ClipParameter(high - start, delta);
    if (a > b) { swap = a; a = b; b = swap; }
    if (a > *first) *first = a;
    if (b < *last) *last = b;
    return *first <= *last;
}
static void AddTaper(s32 ax, s32 ay, s32 bx, s32 by, u16 ha, u16 hb, u16 width,u16 widthB)
{
    s32 first = 0, last = WATER_T_ONE, dx = bx - ax, dy = by - ay;
    struct RiverPiece *piece;
    s32 cacheMargin=AtlasWorld_Generation()>=17?2000:WATER_MARGIN;
    if (sPieceCount >= (AtlasWorld_Generation()>=21?WATER_SEGMENTS:AtlasWorld_Generation()>=18?LEGACY_CREEK_SEGMENTS:AtlasWorld_Generation()>=15?96:24)) return;
    ax -= sBucketX * WATER_BUCKET; bx -= sBucketX * WATER_BUCKET;
    ay -= sBucketY * WATER_BUCKET; by -= sBucketY * WATER_BUCKET;
    /* Reject before fixed-point clipping, keeping parameter calculations bounded. */
    if ((ax < -cacheMargin && bx < -cacheMargin) || (ax > WATER_BUCKET + cacheMargin && bx > WATER_BUCKET + cacheMargin)
        || (ay < -cacheMargin && by < -cacheMargin) || (ay > WATER_BUCKET + cacheMargin && by > WATER_BUCKET + cacheMargin)) return;
    /* Short tapers retain their original endpoints. Re-clipping and rounding
     * their width ramp independently in adjacent buckets would create seams. */
    if(widthB)
    {
        s32 limit=AtlasWorld_Generation()>=18?16384:3000;
        if(dx< -limit || dx>limit || dy< -limit || dy>limit)return;
    }
    else if (!ClipAxis(ax, dx, -cacheMargin, WATER_BUCKET + cacheMargin, &first, &last)
        || !ClipAxis(ay, dy, -cacheMargin, WATER_BUCKET + cacheMargin, &first, &last)) return;
    piece = &sPieces[sPieceCount++];
    piece->ax = ax + (s64)dx * first / WATER_T_ONE; piece->ay = ay + (s64)dy * first / WATER_T_ONE;
    piece->bx = ax + (s64)dx * last / WATER_T_ONE; piece->by = ay + (s64)dy * last / WATER_T_ONE;
    piece->ha = ha + (s64)((s32)hb - ha) * first / WATER_T_ONE;
    piece->hb = ha + (s64)((s32)hb - ha) * last / WATER_T_ONE;
    piece->width=width;piece->widthB=0;
    if(widthB)
    {
        piece->width=width+(s64)((s32)widthB-width)*first/WATER_T_ONE;
        piece->widthB=width+(s64)((s32)widthB-width)*last/WATER_T_ONE;
    }
}
static void AddPiece(s32 ax,s32 ay,s32 bx,s32 by,u16 ha,u16 hb,u16 width)
{AddTaper(ax,ay,bx,by,ha,hb,width,0);}
static void UseBucket(s32 x, s32 y)
{
    s32 bx = x / WATER_BUCKET, by = AtlasTerrain_FloorDiv(y, WATER_BUCKET);
    s32 anchorX = bx*WATER_BUCKET+WATER_BUCKET/2, anchorY = by*WATER_BUCKET+WATER_BUCKET/2;
    u8 i, best = 0;
    for (i = 0; i < 4; i++)
    {
        if (sBuckets[i].valid && sBuckets[i].x == bx && sBuckets[i].y == by)
        { sActive = i; sBuckets[i].stamp = ++sClock; return; }
        if (!sBuckets[i].valid || sBuckets[i].stamp < sBuckets[best].stamp) best = i;
    }
    sActive = best;
    memset(&sBuckets[best], 0, sizeof(sBuckets[best]));
    sBucketX = bx; sBucketY = by;
    /* Candidate ownership belongs to the bucket, never its first visitor. */
    if(anchorX>=ATLAS_WORLD_WIDTH)anchorX=ATLAS_WORLD_WIDTH-1;
    if(anchorY>=ATLAS_WORLD_HALF_HEIGHT)anchorY=ATLAS_WORLD_HALF_HEIGHT-1;
    if(anchorY< -ATLAS_WORLD_HALF_HEIGHT)anchorY= -ATLAS_WORLD_HALF_HEIGHT;
    sBuckets[best].col = (u32)anchorX * ATLAS_PLAN_WIDTH / ATLAS_WORLD_WIDTH;
    sBuckets[best].row = (u32)(anchorY + ATLAS_WORLD_HALF_HEIGHT) * (ATLAS_PLAN_HEIGHT - 1) / (2 * ATLAS_WORLD_HALF_HEIGHT);
    sBuckets[best].valid = TRUE; sBuckets[best].stamp = ++sClock;
}
/* Receiving-shore fitting is bucket work, never a recursive water query. Each
 * step evaluates one point of the existing ground field. Keeping its scalar
 * bracket in the bucket lets neighboring frontiers interleave safely. */
static void CoastPoint(u16 node, u32 t, s32 *x, s32 *y)
{
    u8 part = t / COAST_QUARTER;
    s32 ax,ay,bx,by;u16 unused;
    if(part==4){part=3;}
    t-=part*COAST_QUARTER;
    AtlasWater_Point(node,part,&ax,&ay,&unused);
    AtlasWater_Point(node,part+1,&bx,&by,&unused);
    *x=ax+(s64)(bx-ax)*t/COAST_QUARTER;
    *y=ay+(s64)(by-ay)*t/COAST_QUARTER;
}
static bool8 ReceiverStep(struct WaterBucket *b,u16 node)
{
    u32 t;s32 x,y;struct AtlasPlanSample plan;u8 ground;
    struct AtlasPlanNode *parent=&gAtlasPlan[gAtlasPlan[node].parent];
    if(!b->coastPhase || b->coastNode!=node)
    {b->coastNode=node;b->coastPhase=1;b->coastPart=0;b->coastLo=0;b->coastHi=COAST_ONE;}
    if(b->coastPhase==3)return TRUE;
    t=b->coastPhase==1?b->coastPart*COAST_QUARTER:b->coastLo+(b->coastHi-b->coastLo)/2;
    CoastPoint(node,t,&x,&y);
    ground=AtlasTerrain_GroundHeight(x,y,&plan);
    if(parent->height<ATLAS_PLAN_SEA ? ground<ATLAS_PLAN_SEA
        : plan.lake==1 && plan.water==parent->water && ground+1<plan.water)
    {
        b->coastHi=t;
        if(b->coastPhase==1)b->coastPhase=b->coastPart?2:3;
    }
    else
    {
        b->coastLo=t;
        if(b->coastPhase==1 && ++b->coastPart==5)b->coastPhase=3;
    }
    if(b->coastPhase==2 && b->coastHi-b->coastLo<=1)b->coastPhase=3;
    return FALSE;
}
u8 AtlasWater_MouthStep(u16 node,s32 *x,s32 *y,s32 *dx,s32 *dy)
{
    s32 ax,ay;u16 unused;struct WaterBucket *b;
    if(node>=ATLAS_PLAN_COUNT || gAtlasPlan[node].parent==node
        || gAtlasPlan[node].parent>=ATLAS_PLAN_COUNT
        || gAtlasPlan[gAtlasPlan[node].parent].height>=ATLAS_PLAN_SEA)return 2;
    AtlasWater_Point(node,0,&ax,&ay,&unused);
    UseBucket(ax,ay);b=&sBuckets[sActive];
    if(!ReceiverStep(b,node))return 0;
    if(!b->coastHi || b->coastLo>=COAST_ONE)return 2;
    CoastPoint(node,b->coastLo,x,y);
    {
        u8 part=b->coastLo/COAST_QUARTER;
        s32 bx,by,length;
        AtlasWater_Point(node,part,&ax,&ay,&unused);
        AtlasWater_Point(node,part+1,&bx,&by,&unused);
        *dx=bx-ax;*dy=by-ay;
        length=*dx<0?-*dx:*dx;
        if((*dy<0?-*dy:*dy)>length)length=*dy<0?-*dy:*dy;
        if(length){*dx=*dx*512/length;*dy=*dy*512/length;}
    }
    return 1;
}
static u16 ReceiverLevel(u16 node,u32 t,u32 coast)
{
    struct AtlasPlanNode *parent=&gAtlasPlan[gAtlasPlan[node].parent];
    u32 base=(parent->height<ATLAS_PLAN_SEA?ATLAS_PLAN_SEA:parent->water)*256u;
    u32 rise=ChannelLevel(node)*256u-base;
    if(t>=coast)return base;
    return base+(s64)rise*(coast-t)/coast;
}
/* Flared lowland river outlets; this does not simulate tides or salinity. */
bool8 AtlasWater_IsEstuary(u16 id)
{
    if(AtlasWorld_Generation()<15 || id>=ATLAS_PLAN_COUNT)return FALSE;
    const struct AtlasPlanNode *n=&gAtlasPlan[id];
    return n->height>=ATLAS_PLAN_SEA && n->water<=ATLAS_PLAN_SEA+30 && n->runoff>=600
        && n->parent<ATLAS_PLAN_COUNT && n->parent!=id
        && gAtlasPlan[n->parent].height<ATLAS_PLAN_SEA
        && Mix(gAtlasWorldSeed^id^0x45535455u)%3!=0;
}
bool8 AtlasWater_IsTributary(u16 id)
{
    struct AtlasPlanNode *n,*parent;
    if(id>=ATLAS_PLAN_COUNT)return FALSE;
    n=&gAtlasPlan[id];if(n->parent>=ATLAS_PLAN_COUNT)return FALSE;
    parent=&gAtlasPlan[n->parent];
    /* Older worlds reveal only the first tier beside a main channel. New
     * worlds retain qualifying upstream tributaries throughout that graph. */
    return n->height>=ATLAS_PLAN_SEA && n->runoff>=240 && n->runoff<600
        && n->parent!=id && parent->height>=ATLAS_PLAN_SEA && parent->runoff>=(AtlasWorld_Generation()>=16?240:600);
}
bool8 AtlasWater_IsSmallRiver(u16 id)
{
    if(AtlasWorld_Generation()<16)return AtlasWater_IsTributary(id);
    if(id>=ATLAS_PLAN_COUNT)return FALSE;
    const struct AtlasPlanNode *n=&gAtlasPlan[id];
    if(n->height<ATLAS_PLAN_SEA || n->runoff<240 || n->runoff>=600
     || n->parent>=ATLAS_PLAN_COUNT || n->parent==id)return FALSE;
    /* Every selected edge continues into another qualifying small/main edge,
     * a receiving lake, or the sea. No isolated random stream is fabricated. */
    return gAtlasPlan[n->parent].height<ATLAS_PLAN_SEA
        || gAtlasPlan[n->parent].runoff>=240;
}
static s32 BankReach(s32 width);
static u8 PieceHeight(const struct RiverPiece *p,s32 dot,s32 length);
bool8 AtlasWater_CreekSource(u16 id)
{
    if(AtlasWorld_Generation()<18 || !gAtlasPlanReady || id>=ATLAS_PLAN_COUNT)return FALSE;
    const struct AtlasPlanNode *n=&gAtlasPlan[id];
    if(n->height<ATLAS_PLAN_SEA || n->rain<80 || n->parent>=ATLAS_PLAN_COUNT || n->parent==id
     || (n->runoff<600 && !AtlasWater_IsSmallRiver(id)))return FALSE;
    const struct AtlasPlanNode *parent=&gAtlasPlan[n->parent];
    return parent->height>=ATLAS_PLAN_SEA
        && !(n->basin&ATLAS_LAKE_FLAG) && !(parent->basin&ATLAS_LAKE_FLAG);
}
/* A shared, seed-only branch geometry. Preparation and inspection use the same
 * root identity; bucket position never changes its shape or validation points. */
static bool8 CreekGeometry(u16 id,u8 part,s32 index,u8 side,struct CreekLine *line)
{
    if(!AtlasWater_CreekSource(id) || part>=4 || index<1 || index>16)return FALSE;
    s32 ax,ay,bx,by,dx,dy,length;u16 ha,hb;
    AtlasWater_Point(id,part,&ax,&ay,&ha);AtlasWater_Point(id,part+1,&bx,&by,&hb);
    dx=bx-ax;dy=by-ay;length=dx<0?-dx:dx;
    if((dy<0?-dy:dy)>length)length=dy<0?-dy:dy;
    s32 at=index*8192;
    if(at>=length-1024)return FALSE;
    u32 h=Mix(gAtlasWorldSeed^id*0x9E3779B1u^part*0x85EBCA77u^index*0xC2B2AE3Du^side*0x27D4EB2Fu);
    if((h>>24)>gAtlasPlan[id].rain)return FALSE;
    s32 branch=4096+(h&4095),up=(h>>12)&2047,sign=side?1:-1;
    line->bx=ax+(s64)dx*at/length;line->by=ay+(s64)dy*at/length;
    line->ax=line->bx+(s64)(-dy)*branch*sign/length-(s64)dx*up/length;
    line->ay=line->by+(s64)dx*branch*sign/length-(s64)dy*up/length;
    if(line->ay<=-ATLAS_WORLD_HALF_HEIGHT+16 || line->ay>=ATLAS_WORLD_HALF_HEIGHT-16
     || line->by<=-ATLAS_WORLD_HALF_HEIGHT+16 || line->by>=ATLAS_WORLD_HALF_HEIGHT-16)return FALSE;
    line->hb=ha+(s64)((s32)hb-ha)*at/length;line->ha=0;
    unsigned width=2+gAtlasPlan[id].runoff/512;if(width>10)width=10;
    line->width=width;
    unsigned parentWidth=gAtlasPlan[id].runoff<600?8+gAtlasPlan[id].runoff/32:24+gAtlasPlan[id].runoff/128;
    if(parentWidth>400)parentWidth=400;
    unsigned join=(parentWidth+BankReach(parentWidth)+64)*128/branch;
    line->join=join>64?64:join;
    return TRUE;
}
bool8 AtlasWater_CreekProbe(u16 node,u8 attempt,s32 *x,s32 *y)
{
    struct CreekLine line;
    if(!x || !y || attempt>=128 || !CreekGeometry(node,attempt/32,(attempt%32)/2+1,attempt&1,&line))return FALSE;
    *x=line.bx+(line.ax-line.bx)/2;*y=line.by+(line.ay-line.by)/2;return TRUE;
}
/* Match the existing channel's integer receiving surface in the root's own
 * canonical bucket, independent of the bucket currently preparing the creek. */
static bool8 CreekRootLevel(u16 id,u8 part,s32 shift,struct CreekLine *line)
{
    s32 ax,ay,bx,by;u16 ha,hb;
    AtlasWater_Point(id,part,&ax,&ay,&ha);AtlasWater_Point(id,part+1,&bx,&by,&hb);
    s32 raw=line->bx-shift,x=raw%ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
    s32 wrap=x-raw,originX=x/WATER_BUCKET*WATER_BUCKET;
    s32 originY=AtlasTerrain_FloorDiv(line->by,WATER_BUCKET)*WATER_BUCKET;
    ax+=wrap-originX;bx+=wrap-originX;ay-=originY;by-=originY;
    s32 dx=bx-ax,dy=by-ay,first=0,last=WATER_T_ONE;
    if(!ClipAxis(ax,dx,-2000,WATER_BUCKET+2000,&first,&last)
     || !ClipAxis(ay,dy,-2000,WATER_BUCKET+2000,&first,&last))return FALSE;
    struct RiverPiece p={0};
    p.ax=ax+(s64)dx*first/WATER_T_ONE;p.ay=ay+(s64)dy*first/WATER_T_ONE;
    p.bx=ax+(s64)dx*last/WATER_T_ONE;p.by=ay+(s64)dy*last/WATER_T_ONE;
    p.ha=ha+(s64)((s32)hb-ha)*first/WATER_T_ONE;p.hb=ha+(s64)((s32)hb-ha)*last/WATER_T_ONE;
    dx=p.bx-p.ax;dy=p.by-p.ay;
    s32 px=x-originX-p.ax,py=line->by-originY-p.ay;
    line->hb=PieceHeight(&p,px*dx+py*dy,dx*dx+dy*dy)*256;
    return TRUE;
}
/* At most one ground probe per preparation step. All main pieces are complete
 * before this second pass, so an added creek never takes their water ownership. */
static bool8 CreekStep(struct WaterBucket *b,u16 id,u8 part,s32 shift)
{
    s32 ax,ay,bx,by;u16 unused;
    AtlasWater_Point(id,part,&ax,&ay,&unused);AtlasWater_Point(id,part+1,&bx,&by,&unused);
    s32 dx=bx-ax,dy=by-ay;bool8 horizontal=(dx<0?-dx:dx)>(dy<0?-dy:dy);
    s32 offset=horizontal?b->x*WATER_BUCKET+WATER_BUCKET/2-ax-shift:b->y*WATER_BUCKET+WATER_BUCKET/2-ay;
    if(horizontal?dx<0:dy<0)offset=-offset;
    s32 nearest=AtlasTerrain_FloorDiv(offset,8192);
    while(b->microSlot<6)
    {
        struct CreekLine *line=&b->creek;
        if(!b->microProbe)
        {
            s32 index=nearest-1+b->microSlot/2;
            if(!CreekGeometry(id,part,index,b->microSlot&1,line)){b->microSlot++;continue;}
            line->ax+=shift;line->bx+=shift;
            s32 margin=line->width+BankReach(line->width);
            if(AtlasWorld_Generation()>=21 && margin<1900)margin=1900;
            s32 left=b->x*WATER_BUCKET-margin,right=(b->x+1)*WATER_BUCKET+margin;
            s32 top=b->y*WATER_BUCKET-margin,bottom=(b->y+1)*WATER_BUCKET+margin;
            if((line->ax<left && line->bx<left) || (line->ax>right && line->bx>right)
             || (line->ay<top && line->by<top) || (line->ay>bottom && line->by>bottom))
            {b->microSlot++;continue;}
            if(!CreekRootLevel(id,part,shift,line)){b->microSlot++;continue;}
            b->microProbe=17;
        }
        unsigned probe=b->microProbe;
        if(probe==17)
        {
            struct AtlasPlanSample rootPlan;
            unsigned height=AtlasTerrain_GroundHeight(line->bx,line->by,&rootPlan);
            if(rootPlan.lake || AtlasDune_Envelope(line->bx,line->by,&rootPlan) || height*256<line->hb){b->microProbe=0;b->microSlot++;}
            else b->microProbe=16;
            return FALSE;
        }
        s32 x=line->bx+(s64)(line->ax-line->bx)*probe/16;
        s32 y=line->by+(s64)(line->ay-line->by)*probe/16;
        struct AtlasPlanSample plan;unsigned height=AtlasTerrain_GroundHeight(x,y,&plan);
        if(probe==16 && height>=2)
        {
            u32 head=(height-2)*256,limit=line->hb+6*256;
            line->ha=head<limit?head:limit;
        }
        unsigned along=probe*128,flat=line->join*16;
        u32 surface=line->hb;
        if(along>flat && line->ha>=line->hb)
            surface+=(u32)(line->ha-line->hb)*(along-flat)/((128-line->join)*16);
        if(plan.lake || AtlasDune_Envelope(x,y,&plan) || line->ha<line->hb+64 || height*256<surface+512)
        {b->microProbe=0;b->microSlot++;return FALSE;}
        if(!--b->microProbe)
        {
            unsigned before=sPieceCount;
            AddTaper(line->ax,line->ay,line->bx,line->by,line->ha,line->hb,line->width,line->width);
            if(sPieceCount>before)sPieces[before].widthB=CREEK_FLAG|((u16)line->join<<8)|line->width;
            b->microSlot++;
        }
        return FALSE;
    }
    b->microSlot=0;return TRUE;
}
/* A second, bounded tributary level. Parent endpoints are retained whole,
 * so root identity, receiving height and validation points agree in every bank. */
static bool8 HeadwaterGeometry(const struct WaterBucket *b,const struct RiverPiece *parent,u8 slot,struct CreekLine *line)
{
    s32 dx=parent->bx-parent->ax,dy=parent->by-parent->ay;
    s32 length=dx<0?-dx:dx;if((dy<0?-dy:dy)>length)length=dy<0?-dy:dy;
    if(!length || slot>=2 || !(parent->widthB&CREEK_FLAG))return FALSE;
    s32 lx=parent->ax+dx*(slot+1)/3,ly=parent->ay+dy*(slot+1)/3;
    line->bx=b->x*WATER_BUCKET+lx;line->by=b->y*WATER_BUCKET+ly;
    s32 wrapped=line->bx%ATLAS_WORLD_WIDTH;if(wrapped<0)wrapped+=ATLAS_WORLD_WIDTH;
    u32 hash=Mix(gAtlasWorldSeed^(u32)wrapped*0x9E3779B1u^(u32)line->by*0x85EBCA77u^0x48454144u);
    if((hash>>24)>=224)return FALSE;
    s32 branch=512+(hash&1023),up=(hash>>10)&255,sign=(hash&0x10000)?1:-1;
    /* Upstream is -parent direction: A is its head and B its receiver. */
    line->ax=line->bx-(s64)dy*branch*sign/length-(s64)dx*up/length;
    line->ay=line->by+(s64)dx*branch*sign/length-(s64)dy*up/length;
    if(line->ay<=-ATLAS_WORLD_HALF_HEIGHT+16 || line->ay>=ATLAS_WORLD_HALF_HEIGHT-16)return FALSE;
    s32 dot=(lx-parent->ax)*dx+(ly-parent->ay)*dy;
    line->hb=PieceHeight(parent,dot,dx*dx+dy*dy)*256;line->ha=0;
    line->width=1+parent->width/4;if(line->width>3)line->width=3;
    unsigned join=(parent->width+BankReach(parent->width)+16)*128/branch;
    line->join=join>64?64:join;return TRUE;
}
static void HeadwaterAdvance(struct WaterBucket *b)
{
    b->microProbe=0;
    if(++b->microSlot==2){b->microSlot=0;b->cursor++;}
}
static void HeadwaterStep(struct WaterBucket *b)
{
    while(b->cursor<b->microEnd)
    {
        struct CreekLine *line=&b->creek;
        if(!b->microProbe)
        {
            if(!HeadwaterGeometry(b,&sPieces[b->cursor],b->microSlot,line))
            {HeadwaterAdvance(b);continue;}
            s32 margin=line->width+BankReach(line->width);
            s32 left=b->x*WATER_BUCKET-margin,right=(b->x+1)*WATER_BUCKET+margin;
            s32 top=b->y*WATER_BUCKET-margin,bottom=(b->y+1)*WATER_BUCKET+margin;
            if((line->ax<left && line->bx<left) || (line->ax>right && line->bx>right)
             || (line->ay<top && line->by<top) || (line->ay>bottom && line->by>bottom))
            {HeadwaterAdvance(b);continue;}
            b->microProbe=17;
        }
        unsigned probe=b->microProbe;struct AtlasPlanSample plan;
        if(probe==17)
        {
            unsigned height=AtlasTerrain_GroundHeight(line->bx,line->by,&plan);
            if(plan.lake || plan.rain<96 || AtlasDune_Envelope(line->bx,line->by,&plan) || height*256<line->hb)
                HeadwaterAdvance(b);
            else b->microProbe=16;
            return;
        }
        s32 x=line->bx+(s64)(line->ax-line->bx)*probe/16;
        s32 y=line->by+(s64)(line->ay-line->by)*probe/16;
        unsigned height=AtlasTerrain_GroundHeight(x,y,&plan);
        if(probe==16 && height>=2)
        {
            u32 head=(height-2)*256,limit=line->hb+3*256;
            line->ha=head<limit?head:limit;
        }
        unsigned along=probe*128,flat=line->join*16;
        u32 surface=line->hb;
        if(along>flat && line->ha>=line->hb)
            surface+=(u32)(line->ha-line->hb)*(along-flat)/((128-line->join)*16);
        if(plan.lake || AtlasDune_Envelope(x,y,&plan) || line->ha<line->hb+64 || height*256<surface+512)
        {HeadwaterAdvance(b);return;}
        if(!--b->microProbe)
        {
            unsigned before=sPieceCount;
            AddTaper(line->ax,line->ay,line->bx,line->by,line->ha,line->hb,line->width,line->width);
            if(sPieceCount>before)sPieces[before].widthB=CREEK_FLAG|HEADWATER_FLAG|((u16)line->join<<8)|line->width;
            HeadwaterAdvance(b);
        }
        return;
    }
    b->ready=TRUE;
}
static void PreparePart(void)
{
    struct WaterBucket *b = &sBuckets[sActive];
    if(b->microPass==2){HeadwaterStep(b);return;}
    u8 cursor = b->cursor, part = cursor % 4, node = cursor / 4;
    s32 gx = b->col + node % 4 - 1, gy = b->row + node / 4 - 1, shift = 0;
    u16 id, width;bool8 fit;
    if (gy < 0 || gy >= ATLAS_PLAN_HEIGHT) goto next;
    if (gx < 0) { gx += ATLAS_PLAN_WIDTH; shift = -ATLAS_WORLD_WIDTH; }
    if (gx >= ATLAS_PLAN_WIDTH) { gx -= ATLAS_PLAN_WIDTH; shift = ATLAS_WORLD_WIDTH; }
    id = gy * ATLAS_PLAN_WIDTH + gx;
    if (gAtlasPlan[id].height < ATLAS_PLAN_SEA || (gAtlasPlan[id].runoff < 600 && !AtlasWater_IsSmallRiver(id)) || gAtlasPlan[id].parent >= ATLAS_PLAN_COUNT || gAtlasPlan[id].parent == id) goto next;
    if(b->microPass)
    {
        if(AtlasWater_CreekSource(id) && !CreekStep(b,id,part,shift))return;
        goto next;
    }
    fit=gAtlasPlan[gAtlasPlan[id].parent].height<ATLAS_PLAN_SEA
        || ((gAtlasPlan[gAtlasPlan[id].parent].basin&ATLAS_LAKE_FLAG)
            && gAtlasPlan[id].water>gAtlasPlan[gAtlasPlan[id].parent].water);
    if(fit && !ReceiverStep(b,id))return;
    width = gAtlasPlan[id].runoff<600?8+gAtlasPlan[id].runoff/32:24+gAtlasPlan[id].runoff/128;
    if (width > 400) width = 400;
    {
        s32 ax, ay, bx, by; u16 ha, hb;
        AtlasWater_Point(id,part,&ax,&ay,&ha); AtlasWater_Point(id,part+1,&bx,&by,&hb);
        if(fit)
        {
            u32 start=part*COAST_QUARTER,end=start+COAST_QUARTER,coast=b->coastHi;
            ha=ReceiverLevel(id,start,coast);hb=ReceiverLevel(id,end,coast);
            if(coast>start && coast<end)
            {
                s32 cx=ax+(s64)(bx-ax)*(coast-start)/COAST_QUARTER;
                s32 cy=ay+(s64)(by-ay)*(coast-start)/COAST_QUARTER;
                if(AtlasWater_IsEstuary(id))
                {
                    s32 dx=cx-ax,dy=cy-ay,length=dx<0?-dx:dx;
                    if((dy<0?-dy:dy)>length)length=dy<0?-dy:dy;
                    if(length>=128)
                    {
                        s32 span=length<3000?length:3000;
                        s32 ux=cx-(s64)dx*span/length,uy=cy-(s64)dy*span/length;
                        u32 t=coast-(s64)(coast-start)*span/length;
                        unsigned mouth=width*4;if(mouth>400)mouth=400;
                        AddTaper(ux+shift,uy,cx+shift,cy,ReceiverLevel(id,t,coast),hb,width,mouth);
                    }
                }
                AddPiece(ax+shift,ay,cx+shift,cy,ha,hb,width);
                ax=cx;ay=cy;ha=hb;
            }
        }
        AddPiece(ax+shift,ay,bx+shift,by,ha,hb,width);
    }
next:
    if (++b->cursor == 64)
    {
        if(AtlasWorld_Generation()>=18 && !b->microPass)
        {b->cursor=0;b->microPass=1;b->microStart=b->count;}
        else if(AtlasWorld_Generation()>=21 && b->microPass==1)
        {b->microEnd=b->count;b->cursor=b->microStart;b->microPass=2;b->microSlot=b->microProbe=0;}
        else b->ready=TRUE;
    }
}
static void Prepare(s32 x, s32 y)
{
    UseBucket(x, y);
    while (!sBuckets[sActive].ready) PreparePart();
}
bool8 AtlasWater_Prewarm(s32 x, s32 y)
{
    if (!gAtlasPlanReady || y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT) return TRUE;
    x %= ATLAS_WORLD_WIDTH; if (x < 0) x += ATLAS_WORLD_WIDTH;
    UseBucket(x, y);
    if (sBuckets[sActive].ready) return TRUE;
    PreparePart();
    return FALSE;
}
static u8 PieceHeight(const struct RiverPiece *p,s32 dot,s32 length)
{
    if ((p->ha ^ p->hb) < 256) return p->ha >> 8;
    if (dot <= 0 || !length) return p->ha >> 8;
    if(p->widthB&CREEK_FLAG)
    {
        unsigned join=(p->widthB>>8)&127;
        length=(s64)length*(128-join)/128;if(!length)length=1;
    }
    if (dot >= length) return p->hb >> 8;
    if ((p->ha >> 8) == (p->hb >> 8) + 1)
        return (s64)(p->ha-p->hb)*dot < (s64)((p->ha&255)+1)*length ? p->ha >> 8 : p->hb >> 8;
    return (p->ha + (s64)((s32)p->hb - p->ha)*dot/length) >> 8;
}
static u16 TipWidth(const struct RiverPiece *p)
{return (p->widthB&CREEK_FLAG)?p->width:p->widthB;}
static s32 BankReach(s32 width)
{
    if(AtlasWorld_Generation()<17)return 80;
    s32 reach=width*16;
    return reach<80?80:reach>1600?1600:reach;
}
static u8 Sample(s32 x, s32 y, u8 terrainHeight, const struct AtlasPlanSample *plan, u8 *surface, u8 *valley,struct AtlasBankProfile *bank)
{
    s32 localX, localY; u8 i;
    *valley=255;
    if(bank){bank->height=terrainHeight;bank->wetness=bank->canyon=0;}
    UseBucket(x, y);
    if (!sBuckets[sActive].ready) Prepare(x,y);
    localX = x - sBucketX * WATER_BUCKET; localY = y - sBucketY * WATER_BUCKET;
    for (i = 0; i < sPieceCount; i++)
    {
        struct RiverPiece *p = &sPieces[i];
        s32 minX = p->ax < p->bx ? p->ax : p->bx, maxX = p->ax > p->bx ? p->ax : p->bx;
        s32 minY = p->ay < p->by ? p->ay : p->by, maxY = p->ay > p->by ? p->ay : p->by;
        u16 tip=TipWidth(p);
        s32 width=p->width,maximum=tip>p->width?tip:p->width;
        s32 margin = maximum, dx, dy, px, py, length, dot;
        s64 metric, unit;
        u8 cap;
        /* Once a previous piece has supplied a floor, only water can change
         * the answer. Reject remote pieces before any distance products. */
        if (terrainHeight > 155 && *valley) margin += 48;
        if(bank && margin<maximum+BankReach(maximum))margin=maximum+BankReach(maximum);
        if (localX < minX - margin || localX > maxX + margin
            || localY < minY - margin || localY > maxY + margin) continue;
        dx = p->bx - p->ax; dy = p->by - p->ay;
        px = localX - p->ax; py = localY - p->ay;
        length = dx*dx + dy*dy; dot = px*dx + py*dy;
        if (dot <= 0 || !length)
        { metric = px*px + py*py; unit = 1; }
        else if (dot >= length)
        {
            s32 ex = localX - p->bx, ey = localY - p->by;
            metric = ex*ex + ey*ey; unit = 1;
        }
        else
        {
            s32 cross = px*dy - py*dx;
            metric = (s64)cross*cross; unit = length;
        }
        if(tip && tip!=p->width)
        {
            if(dot>=length && length)width=tip;
            else if(dot>0 && length)width+=(s64)((s32)tip-p->width)*dot/length;
        }
        /* Water and its valley use the same capsule distance. A wet tile is
         * necessarily inside the innermost valley band. Preserve first-water
         * ownership and the minimum cap from preceding overlapping pieces. */
        if (metric > (s64)width*width*unit)
        {
            if(bank && terrainHeight>=ATLAS_PLAN_SEA)
            {
                unsigned band;
                s32 reach=BankReach(width);
                /* River-scaled floodplains fit inside the creation epoch's
                 * cache apron. Older worlds retain their 80-tile profile. */
                for(band=1;band<=8;band++)
                {
                    s32 radius=width+reach*band/8;
                    if(metric<=(s64)radius*radius*unit)
                    {
                        unsigned bed=PieceHeight(p,dot,length),target=terrainHeight;
                        bool8 canyon=AtlasWorld_Generation()>=24 && !(p->widthB&CREEK_FLAG)
                            && width>=16 && width<=96 && terrainHeight>=145
                            && bed>=ATLAS_PLAN_SEA+8 && terrainHeight>=bed+16;
                        static const u8 section[8]={1,2,3,4,16,40,60,64};
                        unsigned weight=band*band;
                        unsigned strength=canyon?terrainHeight-bed-15:0;if(strength>16)strength=16;
                        if(canyon)weight=((16-strength)*weight+strength*section[band-1])/16;
                        if(terrainHeight>bed+1)target=bed+1+(terrainHeight-bed-1)*weight/64;
                        bool8 carved=canyon && strength>=1 && band<8 && target<terrainHeight;
                        /* Lowest fitted floor owns its material. Ties combine
                         * labels, so overlap order cannot change the result. */
                        if(target<bank->height){bank->height=target;bank->canyon=carved;}
                        else if(target==bank->height && carved)bank->canyon=TRUE;
                        if(8-band>bank->wetness)bank->wetness=8-band;
                        break;
                    }
                }
            }
            if (terrainHeight > 155)
                for (cap = 0; cap < 4 && cap < *valley; cap++)
                {
                    s32 radius = width + (cap == 0 ? 8 : cap == 1 ? 18 : cap == 2 ? 30 : 48);
                    if (metric <= (s64)radius*radius*unit) { *valley = cap; break; }
                }
            continue;
        }
        if (terrainHeight > 155) *valley = 0;
        /* Every interpolated value remains between these endpoints. If
         * both occupy one output-height bin, the byte result is constant:
         * avoid the software 64-bit divide without changing its rounding. */
        *surface=PieceHeight(p,dot,length);
        /* A receiving plane owns the junction once the river has reached
         * that level. Never force an upstream channel down to a noisy sea
         * pocket: that can make the following river tile flow uphill. */
        if (plan->lake == 1 && terrainHeight + 1 < plan->water && *surface == plan->water)
            return ATLAS_LAKE;
        if (terrainHeight < ATLAS_PLAN_SEA && *surface == ATLAS_PLAN_SEA) return ATLAS_SEA;
        return ATLAS_RIVER;
    }
    if (plan->lake == 1 && plan->water > ATLAS_PLAN_SEA && terrainHeight + 1 < plan->water)
    {
        *surface = plan->water; return ATLAS_LAKE;
    }
    if (terrainHeight < ATLAS_PLAN_SEA) { *surface = ATLAS_PLAN_SEA; return ATLAS_SEA; }
    return ATLAS_DRY;
}
u8 AtlasWater_Sample(s32 x,s32 y,u8 terrainHeight,const struct AtlasPlanSample *plan,u8 *surface,u8 *valley)
{
    return Sample(x,y,terrainHeight,plan,surface,valley,NULL);
}
u8 AtlasWater_SampleBank(s32 x,s32 y,u8 terrainHeight,const struct AtlasPlanSample *plan,u8 *surface,u8 *valley,struct AtlasBankProfile *bank)
{
    return Sample(x,y,terrainHeight,plan,surface,valley,bank);
}

/* Minimum-width cartographic stroke from prepared local channel geometry.
 * A bounded footprint prevents narrow rivers aliasing out of a close atlas.
 * No LRU touch, preparation, terrain query, or exploration write is allowed. */
static bool8 RiverNear(s32 x,s32 y,u8 padding,u8 creekOnly)
{
    if(!gAtlasPlanReady || padding>80 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return FALSE;
    x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
    s32 bx=x/WATER_BUCKET,by=AtlasTerrain_FloorDiv(y,WATER_BUCKET);
    for(unsigned bucket=0;bucket<4;bucket++)
    {
        const struct WaterBucket *b=&sBuckets[bucket];
        if(!b->valid || !b->ready || b->x!=bx || b->y!=by)continue;
        x-=bx*WATER_BUCKET;y-=by*WATER_BUCKET;
        for(unsigned i=0;i<b->count;i++)
        {
            const struct RiverPiece *p=&sPieceBank[bucket][i];
            if(creekOnly && !(p->widthB&CREEK_FLAG))continue;
            if(creekOnly==2 && !(p->widthB&HEADWATER_FLAG))continue;
            u16 tip=TipWidth(p);
            s32 maximum=tip>p->width?tip:p->width;
            s32 margin=maximum+padding;
            if(x<(p->ax<p->bx?p->ax:p->bx)-margin || x>(p->ax>p->bx?p->ax:p->bx)+margin
             || y<(p->ay<p->by?p->ay:p->by)-margin || y>(p->ay>p->by?p->ay:p->by)+margin)continue;
            s32 dx=p->bx-p->ax,dy=p->by-p->ay,px=x-p->ax,py=y-p->ay;
            s32 length=dx*dx+dy*dy,dot=px*dx+py*dy,width=p->width;
            s64 metric,unit;
            if(dot<=0 || !length){metric=px*px+py*py;unit=1;}
            else if(dot>=length)
            {px=x-p->bx;py=y-p->by;metric=px*px+py*py;unit=1;if(tip)width=tip;}
            else
            {
                s32 cross=px*dy-py*dx;metric=(s64)cross*cross;unit=length;
                if(tip && tip!=p->width)width+=(s64)((s32)tip-p->width)*dot/length;
            }
            width+=padding;
            if(metric<=(s64)width*width*unit)return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

bool8 AtlasWater_RiverNear(s32 x,s32 y,u8 padding)
{return RiverNear(x,y,padding,FALSE);}
bool8 AtlasWater_CreekNear(s32 x,s32 y,u8 padding)
{return AtlasWorld_Generation()>=18 && RiverNear(x,y,padding,TRUE);}

bool8 AtlasWater_HeadwaterNear(s32 x,s32 y,u8 padding)
{return AtlasWorld_Generation()>=21 && RiverNear(x,y,padding,2);}
/* Read only the resident bucket. The caller yields to prepare the returned
 * destination before asking for its final terrain or walking patch. */
bool8 AtlasWater_HeadwaterProbe(s32 x,s32 y,s32 *outX,s32 *outY)
{
    if(AtlasWorld_Generation()<21 || !outX || !outY)return FALSE;
    x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
    s32 bx=x/WATER_BUCKET,by=AtlasTerrain_FloorDiv(y,WATER_BUCKET);
    for(unsigned bucket=0;bucket<4;bucket++)
    {
        const struct WaterBucket *b=&sBuckets[bucket];
        if(!b->valid || !b->ready || b->x!=bx || b->y!=by)continue;
        for(unsigned i=0;i<b->count;i++)
        {
            const struct RiverPiece *p=&sPieceBank[bucket][i];
            if((p->widthB&(CREEK_FLAG|HEADWATER_FLAG))!=(CREEK_FLAG|HEADWATER_FLAG))continue;
            s32 dx=p->bx-p->ax,dy=p->by-p->ay,length=dx<0?-dx:dx;
            if((dy<0?-dy:dy)>length)length=dy<0?-dy:dy;
            if(!length)continue;
            s32 offset=p->width+3;
            *outX=bx*WATER_BUCKET+(p->ax+p->bx)/2-(s64)dy*offset/length;
            *outY=by*WATER_BUCKET+(p->ay+p->by)/2+(s64)dx*offset/length;
            return TRUE;
        }
    }
    return FALSE;
}
