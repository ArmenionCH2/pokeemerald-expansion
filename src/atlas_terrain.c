#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_tiles.h"
#include "atlas_volcano.h"
#include "atlas_dune.h"
#include "atlas_mesa.h"
#include "atlas_coast.h"
#include "atlas_karst.h"
#include "constants/metatile_labels.h"

/* Geographic generation 1 for box 3, not the missing historical generator. */
struct NoiseCache {
    s32 x0, x1, y0, y1;
    s32 lastX, lastY;
    u32 rawX, rawY, remX, remY;
    u16 a, b, c, d;
    bool8 valid;
};
EWRAM_DATA static struct NoiseCache sNoise[12] = {{0}};
static EWRAM_DATA struct {
    s32 x0,x1,y0,y1,cx,cy;
    u16 node,radius;
    u8 peak;
    bool8 valid,active;
} sVolcanoCache={0},sMesaCache={0},sKarstCache={0};
static EWRAM_DATA struct {s32 x,y;struct AtlasMesa value;bool8 valid;} sMesaLast={0};
static EWRAM_DATA struct {s32 x,y;struct AtlasKarst value;bool8 valid,active;} sKarstLast={0};
void AtlasTerrain_SetSeed(u32 seed)
{
    gAtlasWorldSeed = seed;
    memset(sNoise, 0, sizeof(sNoise));
    sVolcanoCache.valid=sMesaCache.valid=sMesaLast.valid=sKarstCache.valid=sKarstLast.valid=FALSE;
    AtlasTiles_Reset();
    AtlasPlan_ResetSample();
}
static const s32 sScales[12] = {2000000, 350000, 160000, 14000, 600000, 1200, 96, 128, 1024, 256, 80000, 20000};
s32 AtlasTerrain_FloorDiv(s32 n, s32 d)
{
    s32 q = n / d;
    return n % d < 0 ? q - 1 : q;
}
static s32 Wrap(s32 n)
{
    n %= ATLAS_WORLD_WIDTH;
    return n < 0 ? n + ATLAS_WORLD_WIDTH : n;
}
static u32 Hash(s32 x, s32 y, u32 salt)
{
    u32 h = gAtlasWorldSeed ^ salt ^ (u32)x * 0x9E3779B1u ^ (u32)y * 0x85EBCA77u;
    h ^= h >> 16; h *= 0x7FEB352Du;
    h ^= h >> 15; h *= 0x846CA68Bu;
    return h ^ (h >> 16);
}
/* Sites are rare dry lower uplands, separated by the macro grid. A cone's
 * kilometre-scale footprint is tiny relative to its ~420 km owner spacing.
 * Site selection reads final notes only: it cannot recurse into water/terrain. */
bool8 AtlasVolcano_Site(u16 node,struct AtlasVolcano *out)
{
    if(!out || !gAtlasPlanReady || AtlasWorld_Generation()<13 || node>=ATLAS_PLAN_COUNT)return FALSE;
    const struct AtlasPlanNode *n=&gAtlasPlan[node];
    u32 h=Hash(node,0,0x564F4C43u);
    if(n->height<140 || n->height>155 || n->water>n->height || n->runoff>=600 || h%29)return FALSE;
    out->x=(u32)(node%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
    out->y=(node/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
    out->radius=2048+((h>>8)&3)*1024;out->distance=0;
    out->uplift=48+((h>>12)&15);out->relief=0;
    return TRUE;
}
bool8 AtlasVolcano_Region(s32 x,s32 y,struct AtlasVolcano *out)
{
    struct AtlasVolcano site={0};
    s32 dx,dy,r,small,rim;
    if(!out || !gAtlasPlanReady || AtlasWorld_Generation()<13 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return FALSE;
    x=Wrap(x);
    if(!sVolcanoCache.valid || x<sVolcanoCache.x0 || x>=sVolcanoCache.x1 || y<sVolcanoCache.y0 || y>=sVolcanoCache.y1)
    {
        u32 col=((u32)x*ATLAS_PLAN_WIDTH+ATLAS_WORLD_WIDTH/2)/ATLAS_WORLD_WIDTH;
        u32 row=((u32)(y+ATLAS_WORLD_HALF_HEIGHT)*(ATLAS_PLAN_HEIGHT-1)+ATLAS_WORLD_HALF_HEIGHT)/(2*ATLAS_WORLD_HALF_HEIGHT);
        sVolcanoCache.node=row*ATLAS_PLAN_WIDTH+col%ATLAS_PLAN_WIDTH;
        sVolcanoCache.cx=col*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
        sVolcanoCache.cy=row*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        sVolcanoCache.x0=sVolcanoCache.cx-ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
        sVolcanoCache.x1=sVolcanoCache.cx+ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
        sVolcanoCache.y0=sVolcanoCache.cy-ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
        sVolcanoCache.y1=sVolcanoCache.cy+ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
        sVolcanoCache.active=AtlasVolcano_Site(sVolcanoCache.node,&site);
        if(sVolcanoCache.active){sVolcanoCache.radius=site.radius;sVolcanoCache.peak=site.uplift;}
        sVolcanoCache.valid=TRUE;
    }
    if(!sVolcanoCache.active)return FALSE;
    site.x=Wrap(sVolcanoCache.cx);site.y=sVolcanoCache.cy;
    site.radius=sVolcanoCache.radius;site.uplift=sVolcanoCache.peak;
    dx=x-sVolcanoCache.cx;dy=y-site.y;
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    r=dx>dy?dx:dy;small=dx>dy?dy:dx;
    r+=small/2; /* bounded octagonal distance, no square root on the GBA */
    if(r>=site.radius)
    {
        site.distance=r>65535?65535:r;site.uplift=site.relief=0;
        *out=site;return TRUE;
    }
    rim=site.radius/4;
    site.distance=r;
    site.uplift=r<rim?site.uplift-24+24*r/rim:site.uplift*(site.radius-r)/(site.radius-rim);
    site.relief=(site.uplift>=48?3:site.uplift>=32?2:site.uplift>=16?1:0)<<4;
    /* A ten-tile-wide radial erosion gap joins outside, rim and crater floor.
     * Prepared cliff stencils retain their usual collision on either side. */
    if(dx<=5)site.relief|=0x80;
    *out=site;return TRUE;
}
bool8 AtlasVolcano_Sample(s32 x,s32 y,struct AtlasVolcano *out)
{
    struct AtlasVolcano site;
    if(!out || !AtlasVolcano_Region(x,y,&site) || site.distance>=site.radius)return FALSE;
    *out=site;return TRUE;
}
bool8 AtlasVolcano_Bare(s32 x,s32 y)
{
    struct AtlasVolcano v;
    return AtlasWorld_Generation()>=14 && AtlasVolcano_Sample(x,y,&v)
        && v.distance<v.radius*3/4;
}
static u32 Smooth(u32 f) { return (f * f * (768 - 2 * f)) >> 16; }
static s32 Noise(s32 x, s32 y, u32 channel)
{
    struct NoiseCache *n = &sNoise[channel];
    s32 scale = sScales[channel];
    u32 fx, fy, top, bottom;
    if (!n->valid || x < n->x0 || x >= n->x1 || y < n->y0 || y >= n->y1)
    {
        s32 gx = x / scale, gy = AtlasTerrain_FloorDiv(y, scale), next;
        n->x0 = gx * scale; n->x1 = n->x0 + scale;
        if (n->x1 > ATLAS_WORLD_WIDTH) n->x1 = ATLAS_WORLD_WIDTH;
        n->y0 = gy * scale; n->y1 = n->y0 + scale;
        next = n->x1 == ATLAS_WORLD_WIDTH ? 0 : gx + 1;
        n->a = Hash(gx, gy, channel + 1) & 255;
        n->b = Hash(next, gy, channel + 1) & 255;
        n->c = Hash(gx, gy + 1, channel + 1) & 255;
        n->d = Hash(next, gy + 1, channel + 1) & 255;
        n->lastX = -2; n->lastY = -0x7FFFFFFF;
        n->rawX = n->rawY = n->remX = n->remY = 0;
        n->valid = TRUE;
    }
    if (x == n->lastX + 1)
    {
        n->remX += 256;
        while (n->remX >= (u32)(n->x1 - n->x0)) { n->rawX++; n->remX -= n->x1 - n->x0; }
    }
    else if (x != n->lastX)
    {
        u32 v = (u32)(x - n->x0) * 256;
        n->rawX = v / (u32)(n->x1 - n->x0); n->remX = v - n->rawX * (n->x1 - n->x0);
    }
    if (y == n->lastY + 1)
    {
        n->remY += 256;
        while (n->remY >= (u32)scale) { n->rawY++; n->remY -= scale; }
    }
    else if (y != n->lastY)
    {
        u32 v = (u32)(y - n->y0) * 256;
        n->rawY = v / (u32)scale; n->remY = v - n->rawY * scale;
    }
    n->lastX = x; n->lastY = y;
    fx = Smooth(n->rawX); fy = Smooth(n->rawY);
    top = (n->a * (256 - fx) + n->b * fx) >> 8;
    bottom = (n->c * (256 - fx) + n->d * fx) >> 8;
    return (top * (256 - fy) + bottom * fy) >> 8;
}
u8 AtlasCoast_Rise(s32 x,s32 y,const struct AtlasPlanSample *plan,u8 ground)
{
    if(!gAtlasPlanReady || AtlasWorld_Generation()<22 || !plan || plan->lake
     || ground<ATLAS_PLAN_SEA || ground>=136 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return 0;
    x=Wrap(x);unsigned context=AtlasPlan_SeaContext(x,y);if(!context)return 0;
    s32 exposure=Noise(x,y,11)-176;if(exposure<=0)return 0;
    unsigned gain=exposure/3;if(gain>24)gain=24;
    if(context<32)gain=gain*context/32;
    return gain*(136-ground)/(136-ATLAS_PLAN_SEA);
}
bool8 AtlasMesa_Site(u16 node,struct AtlasMesa *out)
{
    if(!out || !gAtlasPlanReady || AtlasWorld_Generation()<20 || node>=ATLAS_PLAN_COUNT)return FALSE;
    const struct AtlasPlanNode *n=&gAtlasPlan[node];
    if(n->height<156 || n->height>185 || n->water>n->height || n->runoff>=600)return FALSE;
    u32 hash=Hash(node,0,0x4D455341u);if(hash%5)return FALSE;
    s32 y=(node/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
    struct AtlasHabitat habitat;AtlasHabitat_Sample(y,n->height,n->rain,n->humidity,ATLAS_DRY,&habitat);
    if(habitat.temperature<155 || habitat.moisture>=110)return FALSE;
    out->x=(u32)(node%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;out->y=y;
    out->radius=2048+((hash>>8)&3)*1024;out->distance=0;
    out->uplift=24+((hash>>18)&15);out->relief=0;return TRUE;
}
static bool8 MesaResult(s32 x,s32 y,const struct AtlasMesa *site,struct AtlasMesa *out)
{
    sMesaLast.x=x;sMesaLast.y=y;sMesaLast.value=*site;sMesaLast.valid=TRUE;
    *out=*site;return TRUE;
}
bool8 AtlasMesa_Region(s32 x,s32 y,struct AtlasMesa *out)
{
    struct AtlasMesa site={0};
    if(!out || !gAtlasPlanReady || AtlasWorld_Generation()<20 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return FALSE;
    x=Wrap(x);
    if(sMesaLast.valid && sMesaLast.x==x && sMesaLast.y==y){*out=sMesaLast.value;return TRUE;}
    if(!sMesaCache.valid || x<sMesaCache.x0 || x>=sMesaCache.x1 || y<sMesaCache.y0 || y>=sMesaCache.y1)
    {
        u32 col=((u32)x*ATLAS_PLAN_WIDTH+ATLAS_WORLD_WIDTH/2)/ATLAS_WORLD_WIDTH;
        u32 row=((u32)(y+ATLAS_WORLD_HALF_HEIGHT)*(ATLAS_PLAN_HEIGHT-1)+ATLAS_WORLD_HALF_HEIGHT)/(2*ATLAS_WORLD_HALF_HEIGHT);
        sMesaCache.node=row*ATLAS_PLAN_WIDTH+col%ATLAS_PLAN_WIDTH;
        sMesaCache.cx=col*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
        sMesaCache.cy=row*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        sMesaCache.x0=sMesaCache.cx-ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
        sMesaCache.x1=sMesaCache.cx+ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
        sMesaCache.y0=sMesaCache.cy-ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
        sMesaCache.y1=sMesaCache.cy+ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
        sMesaCache.active=AtlasMesa_Site(sMesaCache.node,&site);
        if(sMesaCache.active){sMesaCache.radius=site.radius;sMesaCache.peak=site.uplift;}
        sMesaCache.valid=TRUE;
    }
    if(!sMesaCache.active)return FALSE;
    site.x=Wrap(sMesaCache.cx);site.y=sMesaCache.cy;site.radius=sMesaCache.radius;
    site.uplift=site.relief=0;
    s32 dx=x-sMesaCache.cx,dy=y-site.y,axis=dx<0?-dx:dx;
    s32 ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    s32 far=(ax>ay?ax:ay)+(ax>ay?ay:ax)/2;
    if(far>=site.radius*2+384)
    {site.distance=far>65535?65535:far;return MesaResult(x,y,&site,out);}
    /* Existing smooth relief fields perturb the outline, not the cap height. */
    dx+=(Noise(x,y,8)-128)*2;dy+=(Noise(Wrap(x+17777),y,8)-128)*2;
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    s32 r=(dx>dy?dx:dy)+(dx>dy?dy:dx)/2;
    site.distance=r>65535?65535:r;
    if(r<site.radius)
    {
        s32 cap=site.radius/2;
        site.uplift=r<=cap?sMesaCache.peak:sMesaCache.peak*(site.radius-r)/(site.radius-cap);
        site.relief=(site.uplift>=36?3:site.uplift>=24?2:site.uplift>=12?1:0)<<4;
        /* A narrow natural approach crosses the surrounding contours. */
        if(axis<=5)site.relief|=0x80;
    }
    return MesaResult(x,y,&site,out);
}
bool8 AtlasMesa_Sample(s32 x,s32 y,struct AtlasMesa *out)
{
    struct AtlasMesa site;
    if(!out || !AtlasMesa_Region(x,y,&site) || site.distance>=site.radius)return FALSE;
    *out=site;return TRUE;
}
bool8 AtlasKarst_Site(u16 node,struct AtlasKarst *out)
{
    if(!out || !gAtlasPlanReady || AtlasWorld_Generation()<23 || node>=ATLAS_PLAN_COUNT)return FALSE;
    const struct AtlasPlanNode *n=&gAtlasPlan[node];
    if(n->height<145 || n->height>175 || n->water>n->height || n->runoff>=240 || n->rain<132)return FALSE;
    u32 hash=Hash(node,0,0x4B415253u);if(hash%5)return FALSE;
    s32 y=(node/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
    struct AtlasHabitat habitat;AtlasHabitat_Sample(y,n->height,n->rain,n->humidity,ATLAS_DRY,&habitat);
    if(habitat.temperature<100 || habitat.moisture<128)return FALSE;
    /* Dormant cones retain their own floor and rim ownership. Dry mesas have
     * incompatible climate, but the explicit exclusion preserves that contract. */
    struct AtlasVolcano volcano;struct AtlasMesa mesa;
    if(AtlasVolcano_Site(node,&volcano) || AtlasMesa_Site(node,&mesa))return FALSE;
    out->x=(u32)(node%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;out->y=y;
    out->radius=512+((hash>>8)&3)*256;out->cut=16+((hash>>18)&15);out->relief=0;return TRUE;
}
bool8 AtlasKarst_Sample(s32 x,s32 y,struct AtlasKarst *out)
{
    if(!out || !gAtlasPlanReady || AtlasWorld_Generation()<23 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return FALSE;
    x=Wrap(x);
    if(sKarstLast.valid && sKarstLast.x==x && sKarstLast.y==y)
    {if(sKarstLast.active)*out=sKarstLast.value;return sKarstLast.active;}
    struct AtlasKarst site={0};bool8 active=FALSE;
    if(!sKarstCache.valid || x<sKarstCache.x0 || x>=sKarstCache.x1 || y<sKarstCache.y0 || y>=sKarstCache.y1)
    {
        u32 col=((u32)x*ATLAS_PLAN_WIDTH+ATLAS_WORLD_WIDTH/2)/ATLAS_WORLD_WIDTH;
        u32 row=((u32)(y+ATLAS_WORLD_HALF_HEIGHT)*(ATLAS_PLAN_HEIGHT-1)+ATLAS_WORLD_HALF_HEIGHT)/(2*ATLAS_WORLD_HALF_HEIGHT);
        sKarstCache.node=row*ATLAS_PLAN_WIDTH+col%ATLAS_PLAN_WIDTH;
        sKarstCache.cx=col*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
        sKarstCache.cy=row*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        sKarstCache.x0=sKarstCache.cx-ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
        sKarstCache.x1=sKarstCache.cx+ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH/2;
        sKarstCache.y0=sKarstCache.cy-ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
        sKarstCache.y1=sKarstCache.cy+ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1);
        sKarstCache.active=AtlasKarst_Site(sKarstCache.node,&site);
        if(sKarstCache.active){sKarstCache.radius=site.radius;sKarstCache.peak=site.cut;}
        sKarstCache.valid=TRUE;
    }
    if(sKarstCache.active)
    {
        s32 dx=x-sKarstCache.cx,dy=y-sKarstCache.cy,axis=dx<0?-dx:dx;
        s32 ax=axis,ay=dy<0?-dy:dy;
        if(ax+ay<2*sKarstCache.radius+256)
        {
            dx+=(Noise(x,y,8)-128)/2;dy+=(Noise(Wrap(x+17777),y,8)-128)/2;
            if(dx<0)dx=-dx;
            if(dy<0)dy=-dy;
            s32 r=(dx>dy?dx:dy)+(dx>dy?dy:dx)/2;
            if(r<sKarstCache.radius)
            {
                site.x=Wrap(sKarstCache.cx);site.y=sKarstCache.cy;site.radius=sKarstCache.radius;
                unsigned core=site.radius/4;
                site.cut=r<=(s32)core?sKarstCache.peak:sKarstCache.peak*(site.radius-r)/(site.radius-core);
                site.relief=0x70|(axis<=5?0x80:0);active=site.cut>0;
            }
        }
    }
    sKarstLast.x=x;sKarstLast.y=y;sKarstLast.value=site;sKarstLast.active=active;sKarstLast.valid=TRUE;
    if(active)*out=site;
    return active;
}
static u8 KarstCut(s32 x,s32 y,u8 height)
{
    struct AtlasKarst karst;
    if(height<=ATLAS_PLAN_SEA+8 || !AtlasKarst_Sample(x,y,&karst))return 0;
    unsigned room=height-ATLAS_PLAN_SEA-8;
    return karst.cut>room?room:karst.cut;
}
/* Coherent erg availability is independent of humidity noise. A smooth
 * envelope fades the local relief at its substrate/climate margins. */
u8 AtlasDune_Envelope(s32 x,s32 y,const struct AtlasPlanSample *plan)
{
    if(!gAtlasPlanReady || AtlasWorld_Generation()<19 || !plan || plan->lake
     || plan->height<132 || plan->height>165 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return 0;
    struct AtlasHabitat habitat;
    AtlasHabitat_Sample(y,plan->height,plan->rain,plan->humidity,ATLAS_DRY,&habitat);
    if(habitat.temperature<190 || habitat.moisture>=85)return 0;
    s32 weight=(Noise(Wrap(x),y,10)-96)*4;
    if(weight<=0)return 0;
    if(weight>128)weight=128;
    if(AtlasWorld_Generation()>=22)
    {
        u8 coastBase=plan->height+(Noise(Wrap(x),y,3)-128)/16;
        if(AtlasCoast_Rise(x,y,plan,coastBase))return 0;
    }
    s32 edge=(habitat.temperature-189)*64;if(weight>edge)weight=edge;
    edge=(85-habitat.moisture)*32;if(weight>edge)weight=edge;
    edge=(plan->height-131)*64;if(weight>edge)weight=edge;
    edge=(166-plan->height)*64;if(weight>edge)weight=edge;
    struct AtlasVolcano site;
    if(AtlasVolcano_Region(x,y,&site) && site.distance<site.radius*2)
    {
        if(site.distance<=site.radius)return 0;
        edge=(site.distance-site.radius)*128/site.radius;
        if(weight>edge)weight=edge;
    }
    struct AtlasMesa mesa;
    if(AtlasMesa_Region(x,y,&mesa) && mesa.distance<mesa.radius*2)
    {
        if(mesa.distance<=mesa.radius)return 0;
        edge=(mesa.distance-mesa.radius)*128/mesa.radius;
        if(weight>edge)weight=edge;
    }
    return weight;
}
static unsigned DuneScore(s32 x,s32 y)
{
    x=Wrap(x);
    s32 warp=(Noise(x,y,8)-128)*2;
    unsigned n=Noise(Wrap(x+y/6+warp),AtlasTerrain_FloorDiv(y,8),9);
    s32 d=(s32)n*2-255;if(d<0)d=-d;
    return 255-d;
}
u8 AtlasDune_Rise(s32 x,s32 y,const struct AtlasPlanSample *plan)
{
    unsigned weight=AtlasDune_Envelope(x,y,plan);
    return weight?DuneScore(x,y)*6*weight/(254*128):0;
}
bool8 AtlasDune_Probe(s32 x,s32 y)
{
    if(!gAtlasPlanReady || AtlasWorld_Generation()<19 || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)return FALSE;
    x=Wrap(x);
    struct AtlasPlanSample plan;AtlasPlan_SampleDetail(x,y,&plan);
    unsigned weight=AtlasDune_Envelope(x,y,&plan);
    if(weight<64)return FALSE;
    unsigned here=DuneScore(x,y)*6*weight/(254*128);
    if(here!=2 && here!=3)return FALSE;
    static const s8 delta[4][2]={{-6,0},{6,0},{0,-4},{0,4}};
    for(unsigned i=0;i<4;i++)
        if((here>=3)!=(DuneScore(x+delta[i][0],y+delta[i][1])*6*weight/(254*128)>=3))return TRUE;
    return FALSE;
}
static u8 Clamp(s32 n) { return n < 0 ? 0 : n > 255 ? 255 : n; }
u8 AtlasTerrain_ReliefNoise(s32 x,s32 y,bool8 broad)
{
    return Noise(Wrap(x),y,broad?8:7);
}
u8 AtlasTerrain_RawHeight(s32 x,s32 y)
{
    s32 e,ridge;
    x=Wrap(x);
    e=(Noise(x,y,0)*3+Noise(x,y,1))/4;
    ridge=Noise(x,y,2)-128;if(ridge<0)ridge=-ridge;
    ridge=255-ridge*2;
    if(e>132 && ridge>140)e+=(ridge-140)*(e-132)/150;
    return Clamp(e+(Noise(x,y,3)-128)/12);
}
u8 AtlasTerrain_GroundHeight(s32 x,s32 y,struct AtlasPlanSample *plan)
{
    x=Wrap(x);
    if(y< -ATLAS_WORLD_HALF_HEIGHT)y= -ATLAS_WORLD_HALF_HEIGHT;
    if(y>=ATLAS_WORLD_HALF_HEIGHT)y=ATLAS_WORLD_HALF_HEIGHT-1;
    AtlasPlan_SampleDetail(x,y,plan);
    s32 height=plan->lake?plan->height:Clamp(plan->height+(Noise(x,y,3)-128)/16);
    struct AtlasVolcano volcano;
    if(!plan->lake && AtlasVolcano_Sample(x,y,&volcano))height=Clamp(height+volcano.uplift);
    struct AtlasMesa mesa;
    if(!plan->lake && AtlasMesa_Sample(x,y,&mesa))height=Clamp(height+mesa.uplift);
    if(!plan->lake && AtlasWorld_Generation()>=22)height=Clamp(height+AtlasCoast_Rise(x,y,plan,height));
    if(!plan->lake)height=Clamp(height+AtlasDune_Rise(x,y,plan));
    if(!plan->lake && AtlasWorld_Generation()>=23)height-=KarstCut(x,y,height);
    return height;
}
void AtlasTerrain_Sample(s32 x, s32 y, struct AtlasCell *cell)
{
    s32 e, rain, vapor, t, m;
    struct AtlasHabitat habitat;
    u8 water = ATLAS_DRY, surface = 0;
    struct AtlasBankProfile bank={0};
    bool8 coastalGround=TRUE;
    bool8 lakeBank=FALSE,dune=FALSE,mesaGround=FALSE,rockCoast=FALSE,karstGround=FALSE;
    u8 duneRise=0;
    if (!cell) return;
    cell->valley = 255;
    x = Wrap(x);
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
    {
        memset(cell, 0, sizeof(*cell)); cell->moisture = cell->humidity = 255;
        cell->biome = ATLAS_OCEAN; cell->waterform = ATLAS_SEA; return;
    }
    if (gAtlasPlanReady)
    {
        struct AtlasPlanSample plan;
        AtlasPlan_SampleDetail(x,y,&plan);
        e = plan.lake ? plan.height : Clamp(plan.height + (Noise(x,y,3)-128)/16);
        struct AtlasVolcano volcano;
        if(!plan.lake && AtlasVolcano_Sample(x,y,&volcano))e=Clamp(e+volcano.uplift);
        struct AtlasMesa mesa;
        if(!plan.lake && AtlasMesa_Sample(x,y,&mesa)){e=Clamp(e+mesa.uplift);mesaGround=mesa.uplift>0;}
        unsigned coastRise=AtlasWorld_Generation()>=22?AtlasCoast_Rise(x,y,&plan,e):0;rockCoast=coastRise>=8;e=Clamp(e+coastRise);
        unsigned duneWeight=AtlasDune_Envelope(x,y,&plan);dune=duneWeight>0;
        if(dune){duneRise=DuneScore(x,y)*6*duneWeight/(254*128);e=Clamp(e+duneRise);}
        if(!plan.lake && AtlasWorld_Generation()>=23){unsigned cut=KarstCut(x,y,e);karstGround=cut>0;e-=cut;}
        rain = Clamp(plan.rain + (Noise(x, y, 4) - 128) / 8); vapor = plan.humidity;
        if(AtlasWorld_Generation()>=9)
        {
            coastalGround=e>=ATLAS_PLAN_SEA && e<128 && AtlasPlan_HasSea(x,y);
            water=AtlasWater_SampleBank(x,y,e,&plan,&surface,&cell->valley,&bank);
            if(water==ATLAS_DRY)
            {
                e=bank.height;
                if(cell->valley==255 && bank.wetness>=6)cell->valley=0;
                /* Lake fitting already identifies its dry shoreline apron.
                 * Wetness follows height above that lake, not sea level. */
                if(plan.lake==2 && e>=plan.water && e-plan.water<=3)
                {
                    unsigned wetness=4-(e-plan.water);
                    lakeBank=TRUE;
                    if(wetness>bank.wetness)bank.wetness=wetness;
                }
            }
        }
        else water = AtlasWater_Sample(x, y, e, &plan, &surface, &cell->valley);
        if (water != ATLAS_DRY) e = surface;
    }
    else
    {
        e=AtlasTerrain_RawHeight(x,y);
        rain = vapor = Noise(x, y, 4);
        if (e < ATLAS_PLAN_SEA) water = ATLAS_SEA;
    }
    AtlasHabitat_Sample(y, e, rain, vapor, water, &habitat);
    t = habitat.temperature; m = habitat.moisture;
    if(water==ATLAS_DRY && bank.wetness)m+=(255-m)*bank.wetness/8;
    cell->elevation = e; cell->temperature = t; cell->moisture = m;
    cell->humidity = habitat.humidity; cell->seasonalRange = habitat.seasonalRange; cell->waterform = water;
    cell->landform = coastalGround && e < 128 && e >= 121 ? ATLAS_COAST : e > 185 ? ATLAS_MOUNTAIN : e > 155 ? ATLAS_HILL : ATLAS_PLAIN;
    if (water == ATLAS_SEA) cell->biome = ATLAS_OCEAN;
    else if (water != ATLAS_DRY) cell->biome = ATLAS_WETLAND;
    else if (t < 45) cell->biome = ATLAS_TUNDRA;
    else if (t < 85) cell->biome = ATLAS_TAIGA;
    else if ((e < 132 || lakeBank) && m > 160) cell->biome = ATLAS_WETLAND;
    else if (t > 160 && m < 98) cell->biome = ATLAS_DESERT;
    else if (t > 175 && m > 155) cell->biome = ATLAS_JUNGLE;
    else if (m > 132) cell->biome = ATLAS_FOREST;
    else if (t > 150 && m < 125) cell->biome = ATLAS_SAVANNA;
    else cell->biome = ATLAS_GRASSLAND;
    if(dune && water==ATLAS_DRY && !bank.wetness && cell->valley==255)
        cell->landform=duneRise>=3?ATLAS_DUNE_CREST:ATLAS_DUNE;
    if(mesaGround && water==ATLAS_DRY && !bank.wetness && cell->valley==255)cell->landform=ATLAS_MESA;
    if(karstGround && water==ATLAS_DRY && !bank.wetness && cell->valley==255)cell->landform=ATLAS_SINKHOLE;
    if(rockCoast && water==ATLAS_DRY && !bank.wetness && cell->valley==255)cell->landform=ATLAS_ROCK_COAST;
}

u16 AtlasTerrain_BaseTile(s32 x, s32 y, const struct AtlasCell *c, bool8 clearCover)
{
    u16 id;
    u32 detail;
    u8 variant;
    if (c->waterform != ATLAS_DRY)
        return PACK_METATILE(AtlasTiles_Water(255, 0, c->waterform)) | PACK_COLLISION(1) | PACK_ELEVATION(3);
    x = Wrap(x);
    /* Stateless native floor detail; no new lattice, cache or geographic query. */
    detail = Hash(x, y, 0x47524E44);
    if(AtlasWorld_Generation()>=14 && c->biome!=ATLAS_TUNDRA
     && (c->elevation>205 || c->landform==ATLAS_MESA
      || (c->landform==ATLAS_ROCK_COAST && c->moisture<110) || AtlasVolcano_Bare(x,y)))
        return PACK_METATILE(ATLAS_ROCK_FLOOR_FIRST+((detail&15)<4?(detail&3):0))|PACK_ELEVATION(3);
    if (AtlasWorld_Generation()>=7 && !clearCover && c->elevation<186
     && c->landform!=ATLAS_COAST && c->landform!=ATLAS_ROCK_COAST && c->landform!=ATLAS_SINKHOLE
     && (c->biome==ATLAS_FOREST || c->biome==ATLAS_JUNGLE || c->biome==ATLAS_TAIGA))
    {
        s32 cy=AtlasTerrain_FloorDiv(y,5),cx=x/5;
        u32 grove=Hash(cx,cy,0x54524545u);
        s32 ox=x-cx*5-(s32)(grove&1),oy=y-cy*5-(s32)((grove>>1)&1);
        unsigned density=c->biome==ATLAS_JUNGLE?75:c->biome==ATLAS_FOREST?55:40;
        // Every fifth row/column stays free of groves. These remain natural
        // ground/cover, not artificial roads. Offsets vary between clearings.
        if ((grove>>8)%100<density && ox>=0 && ox<3 && oy>=0 && oy<3)
            return PACK_METATILE(ATLAS_NATURE_TREE_FIRST+oy*3+ox)|PACK_ELEVATION(3);
    }
    variant = (detail & 15) < 4 ? detail & 3 : 0;
    id = ATLAS_NATURE_GRASS_FIRST + variant;
    if (c->biome == ATLAS_TUNDRA) id = ATLAS_NATURE_SNOW_FIRST + variant;
    else if (c->landform==ATLAS_DUNE || c->landform==ATLAS_DUNE_CREST || c->biome == ATLAS_DESERT || (c->landform == ATLAS_COAST && c->biome != ATLAS_WETLAND)) id = ATLAS_NATURE_SAND_FIRST + variant;
    else if (!clearCover && (c->biome == ATLAS_JUNGLE || c->biome == ATLAS_FOREST || c->biome == ATLAS_TAIGA || c->biome == ATLAS_GRASSLAND))
    {
        s32 threshold = c->biome == ATLAS_JUNGLE ? 125 : c->biome == ATLAS_TAIGA ? 165 : c->biome == ATLAS_FOREST ? 145 : 160;
        /* Reuse the existing 96-tile cover field. Coordinate dither breaks
         * solid rectangles inside a coherent patch, rather than filling a
         * whole local ridge with native encounter-grass artwork. */
        if (Noise(x, y, 6) > threshold + (s32)((detail >> 4) & 31)) id = METATILE_General_TallGrass;
    }
    else if (!clearCover && c->biome == ATLAS_WETLAND)
    {
        if ((detail & 7) < 5) id = ATLAS_NATURE_MARSH_GRASS;
    }
    else if (!clearCover && c->biome == ATLAS_SAVANNA && Noise(x, y, 6) < 85) id = ATLAS_NATURE_SAND_FIRST + variant;

    // Reuse native pass-floor details on sparse, walkable high ground. Preserve tall
    // grass, sand, water and every collision/elevation bit; no new sampler.
    if (c->elevation>=175 && ((detail>>16)&63)==0
     && ((id>=ATLAS_NATURE_GRASS_FIRST && id<ATLAS_NATURE_GRASS_FIRST+4)
      || (id>=ATLAS_NATURE_SNOW_FIRST && id<ATLAS_NATURE_SNOW_FIRST+4)))
        id=ATLAS_NATURE_PASS_FIRST+(c->biome==ATLAS_TUNDRA)+(((detail>>22)&1)<<1);

    if (!clearCover && c->elevation<175 && c->temperature>=85 && c->moisture>=100
     && id>=ATLAS_NATURE_GRASS_FIRST && id<ATLAS_NATURE_GRASS_FIRST+4
     && ((detail>>16)&31)==0)
        id=ATLAS_NATURE_FLOWER_FIRST+((detail>>22)&1);

    return PACK_METATILE(id) | PACK_ELEVATION(3);
}
u16 AtlasTerrain_Tile(s32 x, s32 y)
{
    struct AtlasCell c;
    AtlasTerrain_Sample(x, y, &c);
    return AtlasTiles_At(x, y, &c);
}
bool8 AtlasTerrain_Fill(u16 *map, s32 width, s32 height, s32 chunkX, s32 chunkY)
{
    s32 y, bx, by;
    if (!map || width != 35 || height != 34
        || chunkX < -ATLAS_WORLD_WIDTH / 20 || chunkX >= ATLAS_WORLD_WIDTH / 20
        || chunkY < -ATLAS_WORLD_HALF_HEIGHT / 20 || chunkY >= ATLAS_WORLD_HALF_HEIGHT / 20)
        return FALSE;
    bx = chunkX * 20 - 7; by = chunkY * 20 - 7;
    for (y = 0; y < height; y++)
        AtlasTiles_FillRow(map + y * width, bx, by + y, width);
    return TRUE;
}
bool8 AtlasTerrain_FindSpawn(s32 *x, s32 *y)
{
    s32 i, dx, dy, wx, wy;
    if (!x || !y) return FALSE;
    /* Prefer existing riverbanks so the new geography can be inspected on foot. */
    if (gAtlasPlanReady)
    {
        /* Normalize the seed offset before adding i: u32 wrap would skip nodes.
         * The bounded index is a full permutation of the 96x48 plan. */
        u16 start = Hash(0,0,123) % ATLAS_PLAN_COUNT;
        for (i = 0; i < ATLAS_PLAN_COUNT; i++)
        {
            u16 node = (i + start) % ATLAS_PLAN_COUNT;
            u16 width, level; s32 ax,ay,bx,by;
            if (gAtlasPlan[node].height < ATLAS_PLAN_SEA || gAtlasPlan[node].runoff < 600 || gAtlasPlan[node].runoff > 6000) continue;
            /* AtlasWater_Point reads the parent node; skip malformed plan links. */
            if (gAtlasPlan[node].parent >= ATLAS_PLAN_COUNT) continue;
            width = 24 + gAtlasPlan[node].runoff / 128; if (width > 400) width = 400;
            AtlasWater_Point(node,1,&ax,&ay,&level); AtlasWater_Point(node,3,&bx,&by,&level);
            {
                struct AtlasCell cell; bool8 safe = TRUE;
                AtlasWater_Point(node,2,&wx,&wy,&level);
                if (bx - ax > by - ay && bx - ax > ay - by) wy += (width + 6);
                else if (ax - bx > by - ay && ax - bx > ay - by) wy += (width + 6);
                else wx += (width + 6);
                wx = Wrap(wx); wx = AtlasTerrain_FloorDiv(wx,20)*20+10; wy = AtlasTerrain_FloorDiv(wy,20)*20+10;
                AtlasTerrain_Sample(wx,wy,&cell);
                if (cell.temperature < 85 || cell.waterform != ATLAS_DRY || cell.elevation > 185) continue;
                for (dy=-4;dy<=4&&safe;dy++)for(dx=-4;dx<=4;dx++)
                    if(AtlasTerrain_Tile(wx+dx,wy+dy)&0xC00){safe=FALSE;break;}
                for(dx=0;dx<=25&&safe;dx++)if(AtlasTerrain_Tile(wx+dx,wy)&0xC00)safe=FALSE;
                if(safe){*x=wx;*y=wy;return TRUE;}
            }
        }
    }
    for (i = 0; i < 4096; i++)
    {
        struct AtlasCell cell;
        bool8 safe = TRUE;
        wx = 33453850 + (i % 64) * 223; wy = 4307290 + (i / 64) * 197;
        AtlasTerrain_Sample(wx, wy, &cell);
        if (cell.biome == ATLAS_OCEAN || cell.biome == ATLAS_DESERT || cell.landform == ATLAS_MOUNTAIN) continue;
        for (dy = -4; dy <= 4 && safe; dy++)
            for (dx = -4; dx <= 4; dx++)
                if (AtlasTerrain_Tile(wx + dx, wy + dy) & 0x0C00) { safe = FALSE; break; }
        if (safe) { *x = wx; *y = wy; return TRUE; }
    }
    /* Global fallback surveys real land; no artificial starting terrain. */
    for (i = 0; i < 4096; i++)
    {
        wx = (s32)(Hash(i, 0, 99) % ATLAS_WORLD_WIDTH);
        wy = (s32)(Hash(i, 1, 99) % (2u * ATLAS_WORLD_HALF_HEIGHT)) - ATLAS_WORLD_HALF_HEIGHT;
        {
            bool8 safe = TRUE;
            for (dy = -4; dy <= 4 && safe; dy++)
                for (dx = -4; dx <= 4; dx++)
                    if (AtlasTerrain_Tile(wx + dx, wy + dy) & 0x0C00) { safe = FALSE; break; }
            if (safe) { *x = wx; *y = wy; return TRUE; }
        }
    }
    return FALSE;
}

/* Terrain revisions can cover an old standing tile with a new small channel.
 * Reject wet candidates with one sample instead of compiling nine neighbors. */
bool8 AtlasTerrain_RepairLanding(s32 *x,s32 *y)
{
    s32 dx,dy,r,baseX; s64 yy; struct AtlasCell c;
    if(!x || !y)return FALSE;
    if(!(AtlasTerrain_Tile(*x,*y)&0xC00))return TRUE;
    /* Normalize before adding the search offset: saved x may be extreme. */
    baseX=Wrap(*x);
    for(r=1;r<=32;r++)for(dy=-r;dy<=r;dy++)for(dx=-r;dx<=r;dx++)
    {
        if(dx!=-r && dx!=r && dy!=-r && dy!=r)continue;
        /* Widen before addition; a malformed old latitude must not overflow. */
        yy=(s64)*y+dy;
        if(yy<=-ATLAS_WORLD_HALF_HEIGHT || yy>=ATLAS_WORLD_HALF_HEIGHT)continue;
        AtlasTerrain_Sample(baseX+dx,(s32)yy,&c);
        if(c.waterform || (AtlasTerrain_Tile(baseX+dx,(s32)yy)&0xC00))continue;
        *x=Wrap(baseX+dx);*y=(s32)yy;return TRUE;
    }
    /* An isthmus can disappear into connected water on both sides. Prefer a
     * verified spawn over leaving an older save immobilized inside that water. */
    return AtlasTerrain_FindSpawn(x,y);
}
