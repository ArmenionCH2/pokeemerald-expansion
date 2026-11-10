/* Frozen pre-optimization sampler is an independent output oracle. Inject
 * clipped pieces directly so rare endpoints, zero lengths, overlap ordering,
 * all valley bands and bucket edges do not depend on random macro worlds. */
#include "../../src/atlas_water.c"
#include <assert.h>
#include <stdio.h>
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *p){return 0;}
u16 AtlasWorld_Generation(void){return 3;}
struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
u32 gAtlasWorldSeed;
bool8 gAtlasPlanReady;
s32 AtlasTerrain_FloorDiv(s32 value, s32 divisor)
{ s32 q = value / divisor; return q - (value % divisor < 0); }
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *plan)
{ (void)x;(void)y;(void)plan;assert(!"unexpected ground query in injected ready buckets");return 0; }
static u8 ReferenceSample(s32 x, s32 y, u8 terrainHeight, const struct AtlasPlanSample *plan, u8 *surface, u8 *valley)
{
    s32 localX, localY; u8 i;
    *valley=255;
    UseBucket(x, y);
    if (!sBuckets[sActive].ready) Prepare(x,y);
    localX = x - sBucketX * WATER_BUCKET; localY = y - sBucketY * WATER_BUCKET;
    for (i = 0; i < sPieceCount; i++)
    {
        struct RiverPiece *p = &sPieces[i];
        s32 dx = p->bx - p->ax, dy = p->by - p->ay;
        s32 px = localX - p->ax, py = localY - p->ay, length = dx*dx + dy*dy, dot = px*dx + py*dy;
        s32 cross;
        if(terrainHeight>175)
        {
            s32 minX=p->ax<p->bx?p->ax:p->bx,maxX=p->ax>p->bx?p->ax:p->bx;
            s32 minY=p->ay<p->by?p->ay:p->by,maxY=p->ay>p->by?p->ay:p->by;
            s32 margin=p->width+48;
            if(localX>=minX-margin && localX<=maxX+margin && localY>=minY-margin && localY<=maxY+margin)
            {
                s64 metric,unit;
                u8 cap;
                if(dot<=0 || !length){metric=(s64)px*px+(s64)py*py;unit=1;}
                else if(dot>=length){s32 ex=localX-p->bx,ey=localY-p->by;metric=(s64)ex*ex+(s64)ey*ey;unit=1;}
                else{cross=px*dy-py*dx;metric=(s64)cross*cross;unit=length;}
                for(cap=0;cap<4;cap++)
                {
                    s32 radius=p->width+(cap==0?8:cap==1?18:cap==2?30:48);
                    if(metric<=(s64)radius*radius*unit){if(cap<*valley)*valley=cap;break;}
                }
            }
        }
        if (dot <= 0 || !length)
        {
            if (px*px + py*py > p->width*p->width) continue;
            *surface = p->ha >> 8;
        }
        else if (dot >= length)
        {
            px = localX - p->bx; py = localY - p->by;
            if (px*px + py*py > p->width*p->width) continue;
            *surface = p->hb >> 8;
        }
        else
        {
            cross = px*dy - py*dx;
            if ((s64)cross*cross > (s64)p->width*p->width*length) continue;
            *surface = (p->ha + (s64)((s32)p->hb - p->ha)*dot/length) >> 8;
        }
        return ATLAS_RIVER;
    }
    if (terrainHeight < ATLAS_PLAN_SEA) { *surface = ATLAS_PLAN_SEA; return ATLAS_SEA; }
    if (plan->lake == 1 && plan->water > ATLAS_PLAN_SEA && terrainHeight + 1 < plan->water)
    {
        *surface = plan->water; return ATLAS_LAKE;
    }
    return ATLAS_DRY;
}

static u32 state = 0x41544c33;
static u32 Next(void) { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
static unsigned checks, types[4], caps[5];
static void Check(s32 x, s32 y, u8 height, const struct AtlasPlanSample *plan)
{
    u8 surface = 237, refSurface = 237, valley, refValley, type, refType;
    type = AtlasWater_Sample(x, y, height, plan, &surface, &valley);
    refType = ReferenceSample(x, y, height, plan, &refSurface, &refValley);
    /* Independent original oracle: only a compatible receiving plane may
     * change ownership. Synthetic geometry and its surface stay unchanged. */
    if(refType==ATLAS_RIVER && plan->lake==1 && height+1<plan->water && refSurface==plan->water)
        assert(type==ATLAS_LAKE && surface==refSurface);
    else if(refType==ATLAS_RIVER && height<ATLAS_PLAN_SEA && refSurface==ATLAS_PLAN_SEA)
        assert(type==ATLAS_SEA && surface==refSurface);
    else if(refType==ATLAS_SEA && plan->lake==1 && plan->water>ATLAS_PLAN_SEA && height+1<plan->water)
        assert(type==ATLAS_LAKE && surface==plan->water);
    else assert(type == refType && surface == refSurface);
    if(height>155 && height<=175)
    {
        u8 highSurface=237,highValley;
        ReferenceSample(x,y,176,plan,&highSurface,&highValley);
        assert(valley==highValley); /* extend the same corridor into foothills */
    }
    else assert(valley==refValley);
    checks++; types[type]++; caps[valley == 255 ? 4 : valley]++;
}
int main(void)
{
    unsigned bank, piece, query, j;
    struct AtlasPlanSample plan = {200,96,96,150,1};
    /* Synthetic ready buckets include both canonical X ends and negative Y. */
    for (bank = 0; bank < 256; bank++)
    {
        s32 bx = bank % 2 ? (ATLAS_WORLD_WIDTH - 1) / WATER_BUCKET : 0;
        s32 by = bank % 3 ? 0 : -1;
        s32 availableX = bx ? ATLAS_WORLD_WIDTH - bx * WATER_BUCKET : WATER_BUCKET;
        AtlasWater_Reset(); UseBucket(bx * WATER_BUCKET, by * WATER_BUCKET);
        sBuckets[sActive].ready = TRUE;
        sPieceCount = bank % (24 + 1) /* frozen legacy admission capacity */;
        for (piece = 0; piece < sPieceCount; piece++)
        {
            struct RiverPiece *p = &sPieces[piece];
            p->ax = (s32)(Next() % (WATER_BUCKET + 961)) - 480;
            p->ay = (s32)(Next() % (WATER_BUCKET + 961)) - 480;
            p->bx = (s32)(Next() % (WATER_BUCKET + 961)) - 480;
            p->by = (s32)(Next() % (WATER_BUCKET + 961)) - 480;
            if (piece % 5 == 0) { p->bx = p->ax; p->by = p->ay; }
            if (piece % 5 == 1) p->bx = p->ax;
            if (piece % 5 == 2) p->by = p->ay;
            p->ha = Next() % 65281; p->hb = Next() % 65281;
            p->width = piece % 2 ? 400 : 24 + Next() % 377;
        }
        for (query = 0; query < 8192; query++)
        {
            s32 x = Next() % availableX, y = Next() % WATER_BUCKET;
            Check(bx*WATER_BUCKET+x, by*WATER_BUCKET+y, Next() % 256, &plan);
        }
        /* Exact dry/wet and valley boundaries, on the straight section and
         * both rounded endpoints, plus first-wet overlap ownership. */
        if (!bx)
        {
            static const s32 radii[] = {24,32,42,54,72};
            s32 side, delta, at;
            sPieceCount = 1;
            sPieces[0] = (struct RiverPiece){512,2048,3072,2048,60000,32000,24,0};
            for (j = 0; j < 5; j++) for (side = -1; side <= 1; side += 2)
                for (delta = -1; delta <= 1; delta++) for (at = 0; at < 3; at++)
                {
                    s32 r = (radii[j]+delta)*side;
                    s32 x = at == 0 ? 512-r : at == 1 ? 2048 : 3072+r;
                    s32 y = by*WATER_BUCKET+2048+(at == 1 ? r : 0);
                    Check(x,y,175,&plan); Check(x,y,176,&plan);
                }
            sPieceCount = 2;
            sPieces[1] = (struct RiverPiece){3072,2048,512,2048,32000,60000,400,0};
            for (query = 0; query < WATER_BUCKET; query++)
                for (j = 0; j < 4; j++)
                    Check(query, by*WATER_BUCKET+1600+j*224, 174+j, &plan);
        }
    }
    /* Exercise all endpoint remainders immediately around the descending
     * output-byte transition. ReferenceSample still uses the original divide. */
    AtlasWater_Reset();UseBucket(0,0);sBuckets[sActive].ready=TRUE;sPieceCount=1;
    plan.lake=0;
    for(unsigned hi=0;hi<256;hi++)for(unsigned lo=0;lo<256;lo++)
    {
        s32 distance=256+hi-lo;
        s32 transition=((hi+1)*1000+distance-1)/distance;
        sPieces[0]=(struct RiverPiece){0,100,1000,100,200*256+hi,199*256+lo,30,0};
        Check(0,100,180,&plan);Check(1000,100,180,&plan);
        for(s32 x=transition-1;x<=transition+1;x++)if(x>=0 && x<=1000)Check(x,100,180,&plan);
    }
    for (j = 0; j < 4; j++) assert(types[j]);
    for (j = 0; j < 5; j++) assert(caps[j]);
    printf("PASS: %u water membership/levels agree; existing caps agree and foothill caps match the highland oracle, all water types and valley caps covered\n", checks);
    return 0;
}

bool8 AtlasPlan_CanyonDrain(u16 node){return FALSE;}
