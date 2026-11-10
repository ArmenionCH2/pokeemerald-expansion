#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_volcano.h"
#include "atlas_mesa.h"
#include "atlas_karst.h"

/* Continuous periodic fields, not one independent mound per owner cell.
 * Macro elevation bounds the relief; existing rivers carve its cross-section.
 * Tiers are ordinal visual terraces, not metres or native GBA Z codes. */
static s32 Abs(s32 n){return n<0?-n:n;}
static s32 ReliefScore(s32 x,s32 y,u8 elevation)
{
    s32 local=AtlasTerrain_ReliefNoise(x,y,FALSE),broad=AtlasTerrain_ReliefNoise(x,y,TRUE);
    s32 height=(128-Abs(local-128))*3/2+(broad-128)/4;
    /* Continuous amplitude envelope over the existing physical height field.
     * High uplands retain full strength; plains taper to no raised surface. */
    if(elevation<205)height=height*(elevation-155)/50;
    return height;
}
u8 AtlasMountain_Relief(s32 x,s32 y,const struct AtlasCell *c)
{
    s32 local,broad,height,d;
    u8 tier,cap;
    bool8 pass;
    struct AtlasVolcano volcano;
    if(c->waterform!=ATLAS_DRY)return 0;
    if(c->landform==ATLAS_SINKHOLE){struct AtlasKarst karst;return AtlasKarst_Sample(x,y,&karst)?karst.relief:0;}
    if(c->landform==ATLAS_ROCK_COAST)return 0x60;
    if(c->landform==ATLAS_DUNE || c->landform==ATLAS_DUNE_CREST)return c->landform==ATLAS_DUNE_CREST?0x50:0x40;
    if(c->valley==255 && AtlasVolcano_Sample(x,y,&volcano))return volcano.relief;
    struct AtlasMesa mesa;
    if(c->valley==255 && AtlasMesa_Sample(x,y,&mesa))return mesa.relief;
    if(c->elevation<=155)return 0;
    if(c->valley==0)return 0x80;
    local=AtlasTerrain_ReliefNoise(x,y,FALSE);
    broad=AtlasTerrain_ReliefNoise(x,y,TRUE);
    height=(128-Abs(local-128))*3/2+(broad-128)/4;
    if(c->elevation<205)height=height*(c->elevation-155)/50;
    tier=height>=160?3:height>=112?2:height>=56?1:0;
    if(!tier)return 0;
    /* Broad dry saddles and narrow crest approaches provide connectors.
     * A connector yields only cliff collision; water is always protected. */
    d=Abs(broad-128);
    cap=d<=2?0:d<=5?1:d<=8?2:3;
    if(tier>cap)tier=cap;
    if(c->valley<tier)tier=c->valley;
    pass=Abs(local-128)<=4 || d<=2;
    return (tier<<4)|(pass?0x80:0);
}
bool8 AtlasMountain_Probe(s32 x,s32 y,u8 kind)
{
    s32 local=AtlasTerrain_ReliefNoise(x,y,FALSE),broad=AtlasTerrain_ReliefNoise(x,y,TRUE);
    if(kind)
    {
        s32 height=(128-Abs(local-128))*3/2+(broad-128)/4;
        if(kind==2)return height>=96 && height<=176;
        return height>=166 && height<=176 && Abs(local-128)>6 && Abs(broad-128)>8;
    }
    return Abs(broad-128)<=2 && Abs(local-128)<=28;
}
void AtlasMountain_Center(s32 x,s32 y,s32 *cx,s32 *cy)
{
    *cx=x;*cy=y;
}

bool8 AtlasMountain_Foothill(s32 x,s32 y,const struct AtlasCell *c)
{
    s32 score;
    if(c->waterform || c->elevation<168 || c->elevation>189)return FALSE;
    score=ReliefScore(x,y,c->elevation);
    /* Inspection needs the first contour inside the camera, not a point
     * deep inside a raised tier. Leave room for a safe 3x3 landing as well. */
    return score>=58 && score<=61;
}
