#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"

/* Cache only short river pieces intersecting a 4 km tile-query neighborhood. */
#define WATER_BUCKET 4096
#define WATER_MARGIN 480
/* Sixteen candidate owners, each with four base pieces, at most one shore
 * split and one flare. New worlds retain all 96 possible clipped pieces.
 * Older generation identities retain their original 24-piece admission cap. */
#define WATER_SEGMENTS 96
#define WATER_T_ONE 16777216
#define COAST_ONE 1048576u
#define COAST_QUARTER (COAST_ONE / 4)
struct RiverPiece { s16 ax, ay, bx, by; u16 ha, hb, width, widthB; };
EWRAM_DATA static struct RiverPiece sPieceBank[4][WATER_SEGMENTS] = {{{0}}};
struct WaterBucket { s32 x,y,col,row; u32 stamp,coastLo,coastHi; u16 coastNode; u8 count,cursor,coastPhase,coastPart; bool8 valid,ready; };
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
    *height = ((u32)gAtlasPlan[node].water * (4 - part) + (u32)gAtlasPlan[parent].water * part) * 64;

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
    if (sPieceCount >= (AtlasWorld_Generation()>=15?WATER_SEGMENTS:24)) return;
    ax -= sBucketX * WATER_BUCKET; bx -= sBucketX * WATER_BUCKET;
    ay -= sBucketY * WATER_BUCKET; by -= sBucketY * WATER_BUCKET;
    /* Reject before fixed-point clipping, keeping parameter calculations bounded. */
    if ((ax < -cacheMargin && bx < -cacheMargin) || (ax > WATER_BUCKET + cacheMargin && bx > WATER_BUCKET + cacheMargin)
        || (ay < -cacheMargin && by < -cacheMargin) || (ay > WATER_BUCKET + cacheMargin && by > WATER_BUCKET + cacheMargin)) return;
    /* Short tapers retain their original endpoints. Re-clipping and rounding
     * their width ramp independently in adjacent buckets would create seams. */
    if(widthB)
    {
        if(dx< -3000 || dx>3000 || dy< -3000 || dy>3000)return;
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
    u32 rise=gAtlasPlan[node].water*256u-base;
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
static void PreparePart(void)
{
    struct WaterBucket *b = &sBuckets[sActive];
    u8 cursor = b->cursor, part = cursor % 4, node = cursor / 4;
    s32 gx = b->col + node % 4 - 1, gy = b->row + node / 4 - 1, shift = 0;
    u16 id, width;bool8 fit;
    if (gy < 0 || gy >= ATLAS_PLAN_HEIGHT) goto next;
    if (gx < 0) { gx += ATLAS_PLAN_WIDTH; shift = -ATLAS_WORLD_WIDTH; }
    if (gx >= ATLAS_PLAN_WIDTH) { gx -= ATLAS_PLAN_WIDTH; shift = ATLAS_WORLD_WIDTH; }
    id = gy * ATLAS_PLAN_WIDTH + gx;
    if (gAtlasPlan[id].height < ATLAS_PLAN_SEA || (gAtlasPlan[id].runoff < 600 && !AtlasWater_IsSmallRiver(id)) || gAtlasPlan[id].parent >= ATLAS_PLAN_COUNT || gAtlasPlan[id].parent == id) goto next;
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
    if (++b->cursor == 64) b->ready = TRUE;
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
    if (dot >= length) return p->hb >> 8;
    if ((p->ha >> 8) == (p->hb >> 8) + 1)
        return (s64)(p->ha-p->hb)*dot < (s64)((p->ha&255)+1)*length ? p->ha >> 8 : p->hb >> 8;
    return (p->ha + (s64)((s32)p->hb - p->ha)*dot/length) >> 8;
}
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
    if(bank){bank->height=terrainHeight;bank->wetness=0;}
    UseBucket(x, y);
    if (!sBuckets[sActive].ready) Prepare(x,y);
    localX = x - sBucketX * WATER_BUCKET; localY = y - sBucketY * WATER_BUCKET;
    for (i = 0; i < sPieceCount; i++)
    {
        struct RiverPiece *p = &sPieces[i];
        s32 minX = p->ax < p->bx ? p->ax : p->bx, maxX = p->ax > p->bx ? p->ax : p->bx;
        s32 minY = p->ay < p->by ? p->ay : p->by, maxY = p->ay > p->by ? p->ay : p->by;
        s32 width=p->width,maximum=p->widthB>p->width?p->widthB:p->width;
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
        if(p->widthB && p->widthB!=p->width)
        {
            if(dot>=length && length)width=p->widthB;
            else if(dot>0 && length)width+=(s64)((s32)p->widthB-p->width)*dot/length;
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
                        if(terrainHeight>bed+1)target=bed+1+(terrainHeight-bed-1)*band*band/64;
                        if(target<bank->height)bank->height=target;
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
bool8 AtlasWater_RiverNear(s32 x,s32 y,u8 padding)
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
            s32 maximum=p->widthB>p->width?p->widthB:p->width;
            s32 margin=maximum+padding;
            if(x<(p->ax<p->bx?p->ax:p->bx)-margin || x>(p->ax>p->bx?p->ax:p->bx)+margin
             || y<(p->ay<p->by?p->ay:p->by)-margin || y>(p->ay>p->by?p->ay:p->by)+margin)continue;
            s32 dx=p->bx-p->ax,dy=p->by-p->ay,px=x-p->ax,py=y-p->ay;
            s32 length=dx*dx+dy*dy,dot=px*dx+py*dy,width=p->width;
            s64 metric,unit;
            if(dot<=0 || !length){metric=px*px+py*py;unit=1;}
            else if(dot>=length)
            {px=x-p->bx;py=y-p->by;metric=px*px+py*py;unit=1;if(p->widthB)width=p->widthB;}
            else
            {
                s32 cross=px*dy-py*dx;metric=(s64)cross*cross;unit=length;
                if(p->widthB && p->widthB!=p->width)width+=(s64)((s32)p->widthB-p->width)*dot/length;
            }
            width+=padding;
            if(metric<=(s64)width*width*unit)return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}
