#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"
#include "atlas_region.h"
#include "atlas_vegetation.h"
#include "atlas_volcano.h"
#include "atlas_dune.h"
#include "atlas_mesa.h"
#include "atlas_karst.h"

EWRAM_DATA struct AtlasGeoResult gAtlasGeoResult = {0};
EWRAM_DATA u8 gAtlasGeoStatus = 0;
EWRAM_DATA static u16 sNext[ATLAS_GEO_COUNT] = {0}, sVisited = 0, sNode = 0, sLastLand = 0, sTrial = 0;
EWRAM_DATA static u16 sWork=0;
EWRAM_DATA static u16 sGeneration = 0;
EWRAM_DATA static u8 sType = 0, sPhase = 0, sDirection = 0, sCheck = 0, sAttempt = 0;
EWRAM_DATA static s32 sCX = 0, sCY = 0, sWX = 0, sWY = 0, sDX = 0, sDY = 0, sLX = 0, sLY = 0;
EWRAM_DATA static s32 sMouthDX = 0, sMouthDY = 0;
EWRAM_DATA static u32 sSeed = 0;
EWRAM_DATA static bool8 sInitialized = 0;
EWRAM_DATA static u8 sSceneAt = 0, sSceneWalls = 0, sSceneTarget = 0;
static s32 Abs(s32 n){return n<0?-n:n;}
static s32 Wrap(s32 x){x%=ATLAS_WORLD_WIDTH;return x<0?x+ATLAS_WORLD_WIDTH:x;}
static bool8 MouthTarget(void){return sType==ATLAS_GEO_MOUTH || sType==ATLAS_GEO_ESTUARY;}
static bool8 ReliefTarget(void){return sType==ATLAS_GEO_MOUNTAIN || sType==ATLAS_GEO_RIDGE || sType==ATLAS_GEO_FOOTHILLS;}
static bool8 Sand(const struct AtlasCell *c)
{
    return !c->waterform && c->biome!=ATLAS_TUNDRA
        && (c->biome==ATLAS_DESERT || (c->landform==ATLAS_COAST && c->biome!=ATLAS_WETLAND));
}
static bool8 Green(const struct AtlasCell *c)
{
    return !c->waterform && c->elevation<175 && c->biome!=ATLAS_TUNDRA && !Sand(c);
}
static bool8 GroundBoundary(void)
{return sType==ATLAS_GEO_SAND_EDGE || sType==ATLAS_GEO_ROCK_EDGE || sType==ATLAS_GEO_SNOW_EDGE || sType==ATLAS_GEO_ROCK_SAND || sType==ATLAS_GEO_ROCK_SNOW;}
static bool8 BoundaryMaterial(s32 x,s32 y,const struct AtlasCell *c)
{
    if(sType==ATLAS_GEO_SAND_EDGE)return Sand(c);
    if(c->waterform)return FALSE;
    u16 id=AtlasTerrain_BaseTile(x,y,c,TRUE)&1023;
    unsigned first=(sType==ATLAS_GEO_ROCK_EDGE || sType==ATLAS_GEO_ROCK_SAND || sType==ATLAS_GEO_ROCK_SNOW)?ATLAS_ROCK_FLOOR_FIRST:ATLAS_NATURE_SNOW_FIRST;
    return id>=first && id<first+4;
}
static bool8 BoundaryGreen(s32 x,s32 y,const struct AtlasCell *c)
{
    if(sType==ATLAS_GEO_SAND_EDGE)return Green(c);
    if(c->waterform)return FALSE;
    u16 id=AtlasTerrain_BaseTile(x,y,c,TRUE)&1023;
    if(sType==ATLAS_GEO_ROCK_SNOW)return id>=ATLAS_NATURE_SNOW_FIRST && id<ATLAS_NATURE_SNOW_FIRST+4;
    if(sType==ATLAS_GEO_ROCK_SAND)return id>=ATLAS_NATURE_SAND_FIRST && id<ATLAS_NATURE_SAND_FIRST+4;
    return id>=ATLAS_NATURE_GRASS_FIRST && id<ATLAS_NATURE_GRASS_FIRST+4;
}
static void SandDirection(void)
{
    s32 distance=ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
    sWX=sCX;sWY=sCY;sDX=sCX;sDY=sCY;
    if(sDirection==0)sDX+=distance;
    if(sDirection==1)sDX-=distance;
    if(sDirection==2)sDY+=distance;
    if(sDirection==3)sDY-=distance;
    sPhase=sDirection<4?21:0;
}
static bool8 Warm(s32 x,s32 y)
{
    u8 i;
    for(i=0;i<8;i++)if(AtlasWater_Prewarm(x,y))return TRUE;
    return FALSE;
}
static bool8 ReadyPoint(s32 x,s32 y)
{
    return Warm(x-1,y-1) && Warm(x+1,y-1) && Warm(x-1,y+1) && Warm(x+1,y+1);
}
static bool8 Wet(s32 x,s32 y)
{
    struct AtlasCell c;
    AtlasTerrain_Sample(x,y,&c);
    return c.waterform==(sType==ATLAS_GEO_LAKE?ATLAS_LAKE:(sType==ATLAS_GEO_RIVER || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_FLOODPLAIN || sType==ATLAS_GEO_CANYON || sType==ATLAS_GEO_CREEK || MouthTarget() || sType==ATLAS_GEO_TRIBUTARY)?ATLAS_RIVER:ATLAS_SEA);
}
static void Direction(void)
{
    s32 distance = (sType==ATLAS_GEO_RIVER || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_FLOODPLAIN || sType==ATLAS_GEO_CANYON || sType==ATLAS_GEO_CREEK || MouthTarget() || sType==ATLAS_GEO_TRIBUTARY)?512:ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
    if(MouthTarget())sTrial=0;
    sDX=sCX;sDY=sCY;sWX=sCX;sWY=sCY;
    if(sDirection==0)sDX+=distance;
    if(sDirection==1)sDX-=distance;
    if(sDirection==2)sDY+=distance;
    if(sDirection==3)sDY-=distance;
    sPhase=2;
}
void AtlasGeo_Begin(u8 type)
{
    if(!sInitialized || sSeed!=gAtlasWorldSeed || sGeneration!=AtlasWorld_Generation())
    {
        u8 i;
        for(i=0;i<ATLAS_GEO_COUNT;i++)sNext[i]=ATLAS_PLAN_WIDTH*(ATLAS_PLAN_HEIGHT/2)+(gAtlasWorldSeed%ATLAS_PLAN_WIDTH)+i*13;
        sLastLand=0;sSeed=gAtlasWorldSeed;sGeneration=AtlasWorld_Generation();sInitialized=TRUE;
    }
    sType=type;sPhase=0;sVisited=0;sDirection=0;sWork=0;
    gAtlasGeoStatus=type<ATLAS_GEO_COUNT && type!=ATLAS_GEO_RETURN?0:3;
    if(type==ATLAS_GEO_CANYON && AtlasWorld_Generation()<24)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_SINKHOLE && AtlasWorld_Generation()<23)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_SEA_CLIFF && AtlasWorld_Generation()<22)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_HEADWATER && AtlasWorld_Generation()<21)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_MESA && AtlasWorld_Generation()<20)gAtlasGeoStatus=3;
    if((type==ATLAS_GEO_ROCK_EDGE || type==ATLAS_GEO_ROCK_SAND || type==ATLAS_GEO_ROCK_SNOW) && AtlasWorld_Generation()<14)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_DUNES && AtlasWorld_Generation()<19)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_CREEK && AtlasWorld_Generation()<18)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_FLOODPLAIN && AtlasWorld_Generation()<17)gAtlasGeoStatus=3;
    if(type==ATLAS_GEO_ESTUARY && AtlasWorld_Generation()<15)gAtlasGeoStatus=3;
    if (type==ATLAS_GEO_VOLCANO && AtlasWorld_Generation()<13) gAtlasGeoStatus=3;
    if (type==ATLAS_GEO_GROVE && AtlasWorld_Generation()<7) gAtlasGeoStatus=3;
    if (type==ATLAS_GEO_ISLAND && AtlasWorld_Generation()<12) gAtlasGeoStatus=3;
}
static bool8 Candidate(u16 id)
{
    struct AtlasPlanNode *n=&gAtlasPlan[id];
    if(sType==ATLAS_GEO_DUNES)
    {
        struct AtlasPlanSample plan={n->height,n->rain,n->humidity,n->water,(n->basin&ATLAS_LAKE_FLAG)?1:0};
        s32 x=(u32)(id%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
        s32 y=(id/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        return AtlasDune_Envelope(x,y,&plan)>=64;
    }
    if(sType==ATLAS_GEO_CANYON)return n->height>=132 && (n->runoff>=600 || AtlasWater_IsSmallRiver(id)) && n->runoff<=9200 && n->parent<ATLAS_PLAN_COUNT && n->parent!=id;
    if(sType==ATLAS_GEO_CREEK || sType==ATLAS_GEO_HEADWATER)return AtlasWater_CreekSource(id);
    if(sType==ATLAS_GEO_FLOODPLAIN)return n->height>=132 && n->height<155
        && n->parent<ATLAS_PLAN_COUNT && n->parent!=id && gAtlasPlan[n->parent].height>=ATLAS_PLAN_SEA
        && (n->runoff>=600 || AtlasWater_IsSmallRiver(id));
    if(sType==ATLAS_GEO_ESTUARY)return AtlasWater_IsEstuary(id);
    if(sType==ATLAS_GEO_VOLCANO){struct AtlasVolcano v;return AtlasVolcano_Site(id,&v);}
    if(sType==ATLAS_GEO_ISLAND)return n->height>=ATLAS_PLAN_SEA && AtlasPlan_IsIsland(n->continent);
    if(sType==ATLAS_GEO_SEA_CLIFF)return n->height>=ATLAS_PLAN_SEA && n->height<136 && !(n->basin&ATLAS_LAKE_FLAG);
    if(sType==ATLAS_GEO_SINKHOLE){struct AtlasKarst karst;return AtlasKarst_Site(id,&karst);}
    if(sType==ATLAS_GEO_MESA){struct AtlasMesa mesa;return AtlasMesa_Site(id,&mesa);}
    if(sType==ATLAS_GEO_ROCK_SAND)
    {struct AtlasVolcano volcano;struct AtlasMesa mesa;return n->height>205 || AtlasVolcano_Site(id,&volcano) || AtlasMesa_Site(id,&mesa);}
    if(sType==ATLAS_GEO_ROCK_EDGE || sType==ATLAS_GEO_ROCK_SNOW)
    {struct AtlasVolcano volcano;return n->height>205 || AtlasVolcano_Site(id,&volcano);}
    if(sType==ATLAS_GEO_SNOW_EDGE)
    {
        struct AtlasHabitat habitat;
        s32 y=(id/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        AtlasHabitat_Sample(y,n->height,n->rain,n->humidity,ATLAS_DRY,&habitat);
        return n->height>=ATLAS_PLAN_SEA && habitat.temperature<75;
    }
    if (sType==ATLAS_GEO_SAND_EDGE) return n->height>=121 && n->height<128;
    if (sType==ATLAS_GEO_PLATEAU) return n->height>=170 && n->height<230;
    if (sType>=ATLAS_GEO_FOREST)
    {
        struct AtlasHabitat habitat;
        s32 y=(id/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        if (n->height<ATLAS_PLAN_SEA || n->height>180) return FALSE;
        if (sType==ATLAS_GEO_GROVE && n->height>165) return FALSE;
        AtlasHabitat_Sample(y,n->height,n->rain,n->humidity,ATLAS_DRY,&habitat);
        if (sType==ATLAS_GEO_FOREST || sType==ATLAS_GEO_GROVE) return habitat.temperature>=85 && habitat.moisture>125;
        if (sType==ATLAS_GEO_DESERT) return habitat.temperature>155 && habitat.moisture<105;
        if (sType==ATLAS_GEO_TUNDRA) return habitat.temperature<60;
        return habitat.temperature>75 && habitat.moisture>150 && n->height<140;
    }
    if(sType==ATLAS_GEO_LAND)return n->height>125 && n->height<180 && n->continent && n->continent!=sLastLand;
    if(sType==ATLAS_GEO_MOUNTAIN)return n->height>=195;
    if(sType==ATLAS_GEO_RIDGE)return n->height>=218;
    if(sType==ATLAS_GEO_FOOTHILLS)return n->height>=172 && n->height<=184;
    if(sType==ATLAS_GEO_TRIBUTARY)return AtlasWater_IsSmallRiver(id);
    if(MouthTarget())return n->height>=ATLAS_PLAN_SEA && n->runoff>=600 && n->parent!=id && n->parent<ATLAS_PLAN_COUNT && gAtlasPlan[n->parent].height<ATLAS_PLAN_SEA;
    if(sType==ATLAS_GEO_VALLEY)return n->height>=(AtlasWorld_Generation()>=9?ATLAS_PLAN_SEA:175) && n->runoff>=600 && n->parent!=id && n->parent<ATLAS_PLAN_COUNT;
    if(sType==ATLAS_GEO_LAKE)return (n->basin&ATLAS_LAKE_FLAG) && n->water>ATLAS_PLAN_SEA && n->water>n->height+2;
    if(sType==ATLAS_GEO_RIVER)return n->height>=ATLAS_PLAN_SEA && n->runoff>=600 && n->parent!=id && n->parent<ATLAS_PLAN_COUNT;
    return n->height>=ATLAS_PLAN_SEA && n->height<145;
}
u8 AtlasGeo_Step(void)
{
    s32 x,y;
    u8 i;
    struct AtlasCell c;
    if(gAtlasGeoStatus)return gAtlasGeoStatus;
    if(++sWork>=ATLAS_GEO_SEARCH_STEPS){gAtlasGeoStatus=2;return 2;}
    /* Bisect a dry material/receiving-ground interval, yielding after each prepared point.
     * A water interruption rejects the ray rather than treating an aquatic
     * bank as a ground transition. The final normal 3x3 collision gate still applies. */
    if(sPhase==20)
    {
        if(!Warm(sCX,sCY))return 0;
        AtlasTerrain_Sample(sCX,sCY,&c);
        if(!BoundaryMaterial(sCX,sCY,&c)){sPhase=0;return 0;}
        sDirection=0;SandDirection();return 0;
    }
    if(sPhase==21)
    {
        if(sDY<=-ATLAS_WORLD_HALF_HEIGHT+12 || sDY>=ATLAS_WORLD_HALF_HEIGHT-12)
        {sDirection++;SandDirection();return 0;}
        if(!Warm(sDX,sDY))return 0;
        AtlasTerrain_Sample(sDX,sDY,&c);
        if(!BoundaryGreen(sDX,sDY,&c)){sDirection++;SandDirection();return 0;}
        sPhase=22;return 0;
    }
    if(sPhase==22)
    {
        if(Abs(sDX-sWX)+Abs(sDY-sWY)<=1)
        {sLX=sWX;sLY=sWY;sCheck=sAttempt=0;sPhase=4;return 0;}
        x=sWX+(sDX-sWX)/2;y=sWY+(sDY-sWY)/2;
        if(!Warm(x,y))return 0;
        AtlasTerrain_Sample(x,y,&c);
        if(BoundaryMaterial(x,y,&c)){sWX=x;sWY=y;}
        else if(BoundaryGreen(x,y,&c)){sDX=x;sDY=y;}
        else{sDirection++;SandDirection();}
        return 0;
    }
    if (sPhase==18)
    {
        x=sLX+(s32)(sSceneAt%13)-6;y=sLY+(s32)(sSceneAt/13)-4;
        if (!ReadyPoint(x,y)) return 0;
        if (AtlasVegetation_IsTree(AtlasTerrain_Tile(x,y))) sSceneTarget++;
        if (sSceneTarget>=9) {sPhase=9;return 0;}
        if (++sSceneAt>=117) sPhase=17;
        return 0;
    }
    if(sPhase==39)
    {
        struct AtlasPlanSample plan;unsigned height=AtlasTerrain_GroundHeight(sCX,sCY,&plan);
        sPhase=height>=143 && height>=(unsigned)sMouthDX+14?37:0;return 0;
    }
    if(sPhase==37)
    {
        if(sTrial>=1024){sPhase=0;return 0;}
        unsigned distance=4*(1+sTrial/2);s32 offset=(sTrial&1)?-(s32)distance:(s32)distance;
        x=sCX+(sDirection?0:offset);y=sCY+(sDirection?offset:0);
        if(!ReadyPoint(x,y))return 0;
        sTrial++;AtlasTerrain_Sample(x,y,&c);
        if(c.waterform || c.landform!=ATLAS_CANYON || (AtlasTerrain_Tile(x,y)&0xC00))return 0;
        sLX=x;sLY=y;sSceneAt=0;sPhase=38;return 0;
    }
    if(sPhase==38)
    {
        static const s8 offsets[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
        x=sLX+offsets[sSceneAt][0]*3;y=sLY+offsets[sSceneAt][1]*3;
        if(!ReadyPoint(x,y))return 0;
        if(AtlasTiles_IsCliff(AtlasTerrain_Tile(x,y)&1023))
        {sCheck=sAttempt=0;sPhase=4;return 0;}
        if(++sSceneAt==4)sPhase=37;
        return 0;
    }
    if(sPhase==24)
    {
        x=sLX+(s32)(sSceneAt%13)-6;y=sLY+(s32)(sSceneAt/13)-4;
        if(!ReadyPoint(x,y))return 0;
        u16 id=AtlasTerrain_Tile(x,y)&1023;
        if(AtlasTiles_IsCliff(id))
        {sPhase=9;return 0;}
        if(++sSceneAt>=117)sPhase=sType==ATLAS_GEO_CANYON?37:0;
        return 0;
    }
    if(sPhase==35)
    {
        if(sDX-sWX<=1)
        {sLX=sCX+3;sLY=sCY+sDX;sCheck=sAttempt=0;sPhase=4;return 0;}
        /* Bisect the first rock column outside the walkable axis. Its warped
         * contour may be several tiles away from the centreline contour. */
        s32 distance=sWX+(sDX-sWX)/2;struct AtlasPlanSample plan;
        unsigned height=AtlasTerrain_GroundHeight(sCX+6,sCY+distance,&plan);
        if(height/8<(unsigned)sMouthDX)sWX=distance;else sDX=distance;
        return 0;
    }
    if(sPhase==31)
    {
        if(sDX-sWX<=1)
        {sLX=sCX+3;sLY=sCY+sWX;sCheck=sAttempt=0;sPhase=4;return 0;}
        s32 distance=sWX+(sDX-sWX)/2;struct AtlasMesa mesa;
        if(AtlasMesa_Sample(sCX,sCY+distance,&mesa) && mesa.uplift>=12)sWX=distance;
        else sDX=distance;
        return 0;
    }
    if(sPhase==30)
    {
        static const s8 offsets[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
        unsigned direction=sSceneAt%4,distance=1+sSceneAt/4;
        x=sLX+offsets[direction][0]*(s32)distance;y=sLY+offsets[direction][1]*(s32)distance;
        if(!Warm(x,y))return 0;
        AtlasTerrain_Sample(x,y,&c);
        if(c.waterform)
        {if(++sDirection<4)Direction();else sPhase=0;return 0;}
        if(++sSceneAt==12)sPhase=9;
        return 0;
    }
    if(sPhase==27)
    {
        for(i=0;i<4 && sTrial<4096;i++)
        {
            u16 trial=sTrial++;
            x=sCX+((s32)(trial%64)-32)*16;y=sCY+((s32)(trial/64)-32)*16;
            if(!AtlasDune_Probe(x,y))continue;
            sLX=x;sLY=y;sPhase=28;return 0;
        }
        if(sTrial>=4096)sPhase=0;
        return 0;
    }
    if(sPhase==28)
    {
        if(!Warm(sLX,sLY))return 0;
        AtlasTerrain_Sample(sLX,sLY,&c);
        if(c.waterform || (c.landform!=ATLAS_DUNE && c.landform!=ATLAS_DUNE_CREST)){sPhase=27;return 0;}
        sCheck=sAttempt=0;sPhase=4;return 0;
    }
    if(sPhase==29)
    {
        x=sLX+(s32)(sSceneAt%13)-6;y=sLY+(s32)(sSceneAt/13)-4;
        if(!ReadyPoint(x,y))return 0;
        u16 id=AtlasTerrain_Tile(x,y)&1023;
        if(id>=ATLAS_DUNE_EDGE_FIRST && id<ATLAS_DUNE_EDGE_FIRST+ATLAS_DUNE_EDGE_COUNT){sPhase=9;return 0;}
        if(++sSceneAt>=117)sPhase=27;
        return 0;
    }
    if(sPhase==34)
    {
        if(!Warm(sCX,sCY))return 0;
        AtlasTerrain_Sample(sCX,sCY,&c);
        if(c.waterform){sPhase=0;return 0;}
        sDirection=0;Direction();sPhase=5;return 0;
    }
    if(sPhase==33)
    {
        x=sLX+(s32)(sSceneAt%13)-6;y=sLY+(s32)(sSceneAt/13)-4;
        if(!ReadyPoint(x,y))return 0;
        u16 tile=AtlasTerrain_Tile(x,y);sSceneWalls+=AtlasTiles_IsCliff(tile&1023);
        AtlasTerrain_Sample(x,y,&c);sSceneTarget+=c.waterform==ATLAS_SEA;
        if(++sSceneAt>=117)
        {
            if(sSceneWalls && sSceneTarget)sPhase=9;
            else if(++sDirection<4){Direction();sPhase=5;}else sPhase=0;
        }
        return 0;
    }
    if(sPhase==32)
    {
        if(!Warm(sLX,sLY))return 0;
        AtlasTerrain_Sample(sLX,sLY,&c);
        if(c.waterform || !AtlasWater_HeadwaterNear(sLX,sLY,8)){sPhase=25;return 0;}
        sCheck=sAttempt=0;sPhase=4;return 0;
    }
    if(sPhase==25)
    {
        for(i=0;i<4 && sTrial<128;i++)
        {
            if(!AtlasWater_CreekProbe(sNode,sTrial++,&sCX,&sCY))continue;
            sPhase=26;return 0;
        }
        if(sTrial>=128)sPhase=0;
        return 0;
    }
    if(sPhase==26)
    {
        if(!Warm(sCX,sCY))return 0;
        if(sType==ATLAS_GEO_HEADWATER)
        {
            sPhase=AtlasWater_HeadwaterProbe(sCX,sCY,&sLX,&sLY)?32:25;return 0;
        }
        AtlasTerrain_Sample(sCX,sCY,&c);
        if(c.waterform!=ATLAS_RIVER || !AtlasWater_CreekNear(sCX,sCY,0)){sPhase=25;return 0;}
        sDirection=0;Direction();return 0;
    }
    if(sPhase==0)
    {
        for(i=0;i<24;i++)
        {
            if(sVisited++>=ATLAS_PLAN_COUNT){gAtlasGeoStatus=2;return 2;}
            sNode=sNext[sType]++;if(sNext[sType]>=ATLAS_PLAN_COUNT)sNext[sType]=0;
            if(!Candidate(sNode))continue;
            sCX=(u32)(sNode%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
            sCY=(sNode/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
            if(sType==ATLAS_GEO_VOLCANO)
            {
                struct AtlasVolcano v;
                if(!AtlasVolcano_Site(sNode,&v))continue;
                /* Approach the second cone terrace through its radial gap. */
                sLX=v.x;sLY=v.y+v.radius-32*(v.radius-v.radius/4)/v.uplift;
                sCheck=sAttempt=0;sPhase=4;return 0;
            }
            if(sType==ATLAS_GEO_SINKHOLE)
            {
                struct AtlasKarst karst;struct AtlasPlanSample plan;
                AtlasKarst_Site(sNode,&karst);sCX=karst.x;sCY=karst.y;
                sMouthDX=AtlasTerrain_GroundHeight(sCX+6,sCY,&plan)/8+1;
                sWX=0;sDX=karst.radius+128;sPhase=35;return 0;
            }
            if(sType==ATLAS_GEO_MESA)
            {
                struct AtlasMesa mesa;AtlasMesa_Site(sNode,&mesa);
                sCX=mesa.x;sCY=mesa.y;sWX=0;sDX=mesa.radius+512;sPhase=31;return 0;
            }
            if(sType==ATLAS_GEO_SEA_CLIFF)
            {sPhase=34;return 0;}
            if(GroundBoundary())
            {
                if(sType==ATLAS_GEO_ROCK_SAND)
                {
                    struct AtlasVolcano volcano;struct AtlasMesa mesa;
                    if(AtlasVolcano_Site(sNode,&volcano)){sCX=volcano.x;sCY=volcano.y;}
                    else if(AtlasMesa_Site(sNode,&mesa)){sCX=mesa.x;sCY=mesa.y;}
                }
                sPhase=20;return 0;
            }
            if(sType==ATLAS_GEO_RIVER || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_FLOODPLAIN || sType==ATLAS_GEO_CANYON || sType==ATLAS_GEO_CREEK || MouthTarget() || sType==ATLAS_GEO_TRIBUTARY){u16 height;AtlasWater_Point(sNode,sType==ATLAS_GEO_CANYON?1:2,&sCX,&sCY,&height);}
            if(sType==ATLAS_GEO_CANYON){sDirection=sNode/ATLAS_PLAN_WIDTH==gAtlasPlan[sNode].parent/ATLAS_PLAN_WIDTH;sTrial=0;u16 level;AtlasWater_Point(sNode,1,&sCX,&sCY,&level);sMouthDX=level/256;sPhase=39;return 0;}
            if(MouthTarget()){sTrial=0;sPhase=11;return 0;}
            if(ReliefTarget()){sTrial=0;sPhase=sType==ATLAS_GEO_FOOTHILLS?10:6;return 0;}
            if(sType==ATLAS_GEO_DUNES){sTrial=0;sPhase=27;return 0;}
            if(sType==ATLAS_GEO_CREEK || sType==ATLAS_GEO_HEADWATER){sTrial=0;sPhase=25;return 0;}
            if(sType==ATLAS_GEO_FLOODPLAIN){sDirection=0;sPhase=1;return 0;}
            if (sType>=ATLAS_GEO_FOREST) {sTrial=0;sPhase=17;return 0;}
            sDirection=0;sPhase=1;return 0;
        }
        return 0;
    }
    if(sPhase==11)
    {
        u8 status=AtlasWater_MouthStep(sNode,&sCX,&sCY,&sMouthDX,&sMouthDY);
        if(!status)return 0;
        if(status==1 && sType==ATLAS_GEO_ESTUARY)
        {
            /* Inspect the funnel upstream of its sea cut, where a dry bank
             * need not fit inside the rounded mouth's receiving-water cap. */
            sCX-=sMouthDX;sCY-=sMouthDY;
        }
        sPhase=status==1?12:0;return 0;
    }
    if(sPhase==12)
    {
        if(!Warm(sCX,sCY))return 0;
        AtlasTerrain_Sample(sCX,sCY,&c);
        if(c.waterform!=ATLAS_RIVER){sPhase=0;return 0;}
        sDirection=0;Direction();return 0;
    }
    if(sPhase==14)
    {
        /* Require both receiving sea and upstream freshwater in the actual
         * camera rectangle, one resumable point per inspector update. The
         * estuary inspector may use the flared bank upstream of the sea edge;
         * the narrower river-mouth inspector still requires both waters. */
        x=sLX+(s32)(sSceneAt%13)-6;y=sLY+(s32)(sSceneAt/13)-4;
        if(!Warm(x,y))return 0;
        AtlasTerrain_Sample(x,y,&c);
        if(c.waterform==ATLAS_SEA)sSceneWalls=1;
        if(c.waterform==ATLAS_RIVER)sSceneTarget=1;
        if(sSceneTarget && (sSceneWalls || sType==ATLAS_GEO_ESTUARY)){sPhase=9;return 0;}
        if(++sSceneAt<117)return 0;
        if(!sTrial && sSceneTarget && !sSceneWalls)
        {
            /* The coast may cross an oblique bank away from the centerline
             * cut. Follow this dry bank downstream to its receiving shore. */
            sTrial=1;sWX=sLX;sWY=sLY;
            sDX=sLX+sMouthDX;sDY=sLY+sMouthDY;
            sPhase=15;return 0;
        }
        if(++sDirection<4)Direction();else sPhase=0;
        return 0;
    }
    if(sPhase==15)
    {
        if(!Warm(sDX,sDY))return 0;
        AtlasTerrain_Sample(sDX,sDY,&c);
        if(c.waterform==ATLAS_SEA){sPhase=16;return 0;}
        if(++sDirection<4)Direction();else sPhase=0;
        return 0;
    }
    if(sPhase==16)
    {
        if(Abs(sDX-sWX)<=1 && Abs(sDY-sWY)<=1)
        {
            sLX=sWX+(sWX>sDX?2:sWX<sDX?-2:0);
            sLY=sWY+(sWY>sDY?2:sWY<sDY?-2:0);
            sCheck=0;sAttempt=0;sPhase=4;return 0;
        }
        x=sWX+(sDX-sWX)/2;y=sWY+(sDY-sWY)/2;
        if(!Warm(x,y))return 0;
        AtlasTerrain_Sample(x,y,&c);
        if(c.waterform){sDX=x;sDY=y;}else{sWX=x;sWY=y;}
        return 0;
    }
    if(sPhase==1)
    {
        if(!Warm(sCX,sCY))return 0;
        AtlasTerrain_Sample(sCX,sCY,&c);
        if(sType==ATLAS_GEO_LAND)
        {
            if(c.waterform){sPhase=0;return 0;}
            sLX=sCX;sLY=sCY;sCheck=0;sAttempt=0;sPhase=4;return 0;
        }
        if(sType==ATLAS_GEO_COAST)
        {
            /* Search for a sea edge around this low coastal candidate. */
            if(c.waterform){sPhase=0;return 0;}
            sWX=sCX;sWY=sCY;sDX=sCX+ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;sDY=sCY;sPhase=5;return 0;
        }
        if(!Wet(sCX,sCY)){sPhase=0;return 0;}
        Direction();return 0;
    }
    if(sPhase==5)
    {
        if(!Warm(sDX,sDY))return 0;
        if(!Wet(sDX,sDY))
        {
            if(++sDirection>=4){sPhase=0;return 0;}
            Direction();sPhase=5;return 0;
        }
        x=sWX;y=sWY;sWX=sDX;sWY=sDY;sDX=x;sDY=y;sPhase=3;return 0;
    }
    if(sPhase==2)
    {
        if(!Warm(sDX,sDY))return 0;
        AtlasTerrain_Sample(sDX,sDY,&c);
        if(c.waterform)
        {
            if(++sDirection>=4){sPhase=0;return 0;}
            Direction();return 0;
        }
        sPhase=3;return 0;
    }
    if(sPhase==3)
    {
        if(Abs(sDX-sWX)+Abs(sDY-sWY)<=1)
        {
            s32 offset=sType==ATLAS_GEO_FLOODPLAIN?40:(MouthTarget() || sType==ATLAS_GEO_SEA_CLIFF)?2:4;
            sLX=sDX+(sDX>sWX?offset:sDX<sWX?-offset:0);sLY=sDY+(sDY>sWY?offset:sDY<sWY?-offset:0);
            sCheck=0;sAttempt=0;sPhase=4;return 0;
        }
        x=sWX+(sDX-sWX)/2;y=sWY+(sDY-sWY)/2;
        if(!Warm(x,y))return 0;
        if(Wet(x,y)){sWX=x;sWY=y;}else{sDX=x;sDY=y;}
        return 0;
    }
    if (sPhase==17)
    {
        if (sTrial>=256) {sPhase=0;return 0;}
        x=sCX+((s32)(sTrial%16)-8)*8;y=sCY+((s32)(sTrial/16)-8)*8;
        if (!Warm(x,y)) return 0;
        sTrial++;AtlasTerrain_Sample(x,y,&c);
        u8 target=sType==ATLAS_GEO_FOREST || sType==ATLAS_GEO_GROVE?ATLAS_FOREST:sType==ATLAS_GEO_DESERT?ATLAS_DESERT:sType==ATLAS_GEO_TUNDRA?ATLAS_TUNDRA:ATLAS_WETLAND;
        if (c.waterform || (sType==ATLAS_GEO_GROVE && (c.landform==ATLAS_COAST || c.elevation>=175 || AtlasMountain_Relief(x,y,&c))) || (sType==ATLAS_GEO_PLATEAU
            ? AtlasRegion_At(x,y)!=ATLAS_REGION_PLATEAU : sType==ATLAS_GEO_ISLAND?FALSE:c.biome!=target))
        {if (sTrial==1) sPhase=0;return 0;}
        sLX=x;sLY=y;sCheck=sAttempt=0;sPhase=4;return 0;
    }
    if(sPhase==10)
    {
        /* Macro notes are interpolated at runtime. Reject weak/steep regions
         * before spending thousands of local probes on a missing low contour. */
        if(!Warm(sCX,sCY))return 0;
        AtlasTerrain_Sample(sCX,sCY,&c);
        sPhase=c.waterform || c.elevation<174 || c.elevation>188?0:6;
        return 0;
    }
    if(sPhase==6)
    {
        for(i=0;i<32 && sTrial<4096;i++)
        {
            u16 trial=sTrial++;
            x=sCX+((s32)(trial%64)-32)*8;y=sCY+((s32)(trial/64)-32)*8;
            if(!AtlasMountain_Probe(x,y,sType==ATLAS_GEO_FOOTHILLS?2:sType==ATLAS_GEO_RIDGE))continue;
            sLX=x;sLY=y;sPhase=7;return 0;
        }
        if(sTrial>=4096)sPhase=0;
        return 0;
    }
    if(sPhase==7)
    {
        u8 relief;
        if(!Warm(sLX,sLY))return 0;
        AtlasTerrain_Sample(sLX,sLY,&c);relief=AtlasMountain_Relief(sLX,sLY,&c);
        if(c.waterform || (sType==ATLAS_GEO_FOOTHILLS
            ? ((relief&0x70)!=0x10 || (relief&0x80) || !AtlasMountain_Foothill(sLX,sLY,&c))
            : (c.elevation<=175 || (sType==ATLAS_GEO_RIDGE?(relief&0x70)!=0x30:!(relief&0x80)))))
        {sPhase=6;return 0;}
        sCheck=0;sAttempt=0;sPhase=4;return 0;
    }
    if(sPhase==8)
    {
        u16 tile;
        if(sType==ATLAS_GEO_MOUNTAIN && sSceneAt>=22)
        {
            /* After the cheap cliff cross-ray filter, look off-axis for
             * sparse stones, one viewport point per menu update. */
            u8 at=sSceneAt-22;
            x=sLX+(s32)(at%13)-6;y=sLY+(s32)(at/13)-4;
        }
        else
        {
            x=sLX+(sSceneAt<13?(s32)sSceneAt-6:0);
            y=sLY+(sSceneAt<13?0:(s32)sSceneAt-17);
        }
        if(!ReadyPoint(x,y))return 0;
        tile=AtlasTerrain_Tile(x,y);
        if(AtlasTiles_IsCliff(tile&1023))sSceneWalls++;
        if(sType==ATLAS_GEO_MOUNTAIN)
        {
            u16 id=tile&1023;
            if(id>=ATLAS_NATURE_PASS_FIRST && id<ATLAS_NATURE_PASS_FIRST+ATLAS_NATURE_PASS_COUNT)sSceneTarget++;
        }
        else if(sType==ATLAS_GEO_VALLEY)
        {AtlasTerrain_Sample(x,y,&c);if(c.waterform==ATLAS_RIVER)sSceneTarget++;}
        if(sType==ATLAS_GEO_MOUNTAIN && sSceneWalls && sSceneTarget)
        {sPhase=9;}
        else
        {
            sSceneAt++;
            if(sSceneAt==22 && !sSceneWalls && !(sType==ATLAS_GEO_VALLEY && AtlasWorld_Generation()>=9)){sPhase=ReliefTarget()?6:0;return 0;}
            if(sSceneAt<(sType==ATLAS_GEO_MOUNTAIN?139:22))return 0;
            if((!sSceneWalls && !(sType==ATLAS_GEO_VALLEY && AtlasWorld_Generation()>=9)) || ((sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_MOUNTAIN) && !sSceneTarget))
            {sPhase=ReliefTarget()?6:0;return 0;}
        }
        sPhase=9;
    }
    if(sPhase==9)
    {
        gAtlasGeoResult.x=Wrap(sLX);gAtlasGeoResult.y=sLY;
        gAtlasGeoResult.landmass=gAtlasPlan[sNode].continent;gAtlasGeoResult.node=sNode;gAtlasGeoResult.type=sType;
        if(sType==ATLAS_GEO_LAND)sLastLand=gAtlasGeoResult.landmass;
        gAtlasGeoStatus=1;return 1;
    }
    /* Validate the actual 3x3 walking patch using final art/collision, not
     * macro classification. Each call yields after one point query. */
    x=sLX+sCheck%3-1;y=sLY+sCheck/3-1;
    if(!ReadyPoint(x,y))return 0;
    if(y<=-ATLAS_WORLD_HALF_HEIGHT+12 || y>=ATLAS_WORLD_HALF_HEIGHT-12 || (AtlasTerrain_Tile(x,y)&0xC00))
    {
        if(sType==ATLAS_GEO_CANYON){sPhase=37;return 0;}
        if(sType==ATLAS_GEO_SEA_CLIFF)
        {if(++sDirection<4){Direction();sPhase=5;}else sPhase=0;return 0;}
        if(sType==ATLAS_GEO_VOLCANO || sType==ATLAS_GEO_MESA || sType==ATLAS_GEO_SINKHOLE){sPhase=0;return 0;}
        if(GroundBoundary()){sDirection++;SandDirection();return 0;}
        if(sType==ATLAS_GEO_DUNES){sPhase=27;return 0;}
        if(sType==ATLAS_GEO_CREEK || sType==ATLAS_GEO_HEADWATER){sPhase=25;return 0;}
        if(sType==ATLAS_GEO_FLOODPLAIN)
        {if(++sDirection<4)Direction();else sPhase=0;return 0;}
        if (sType>=ATLAS_GEO_FOREST) {sPhase=17;return 0;}
        if(++sAttempt<4 && sType!=ATLAS_GEO_LAND && !ReliefTarget())
        {sLX+=sDX>sWX?3:sDX<sWX?-3:0;sLY+=sDY>sWY?3:sDY<sWY?-3:0;sCheck=0;return 0;}
        if(MouthTarget() && ++sDirection<4)Direction();
        else sPhase=ReliefTarget()?6:0;
        return 0;
    }
    if(++sCheck==9)
    {
        if(sType==ATLAS_GEO_SEA_CLIFF)
        {
            AtlasTerrain_Sample(sLX,sLY,&c);
            if(c.waterform || c.landform!=ATLAS_ROCK_COAST)
            {if(++sDirection<4){Direction();sPhase=5;}else sPhase=0;return 0;}
            sSceneAt=sSceneWalls=sSceneTarget=0;sPhase=33;return 0;
        }
        if(sType==ATLAS_GEO_CANYON)
        {
            AtlasTerrain_Sample(sLX,sLY,&c);
            if(c.waterform || c.landform!=ATLAS_CANYON){sPhase=37;return 0;}
            sSceneAt=0;sPhase=24;return 0;
        }
        if(sType==ATLAS_GEO_SINKHOLE)
        {
            AtlasTerrain_Sample(sLX,sLY,&c);
            if(c.waterform || c.landform!=ATLAS_SINKHOLE){sPhase=0;return 0;}
            sSceneAt=0;sPhase=24;return 0;
        }
        if(sType==ATLAS_GEO_MESA)
        {
            AtlasTerrain_Sample(sLX,sLY,&c);
            if(c.waterform || c.landform!=ATLAS_MESA){sPhase=0;return 0;}
            sSceneAt=0;sPhase=24;return 0;
        }
        if(sType==ATLAS_GEO_ROCK_EDGE || sType==ATLAS_GEO_SNOW_EDGE || sType==ATLAS_GEO_ROCK_SAND || sType==ATLAS_GEO_ROCK_SNOW)
        {
            u16 id=AtlasTerrain_Tile(sLX,sLY)&1023;
            unsigned first=sType==ATLAS_GEO_ROCK_SNOW?ATLAS_ROCK_SNOW_EDGE_FIRST:sType==ATLAS_GEO_ROCK_SAND?ATLAS_ROCK_SAND_EDGE_FIRST:sType==ATLAS_GEO_ROCK_EDGE?ATLAS_ROCK_GROUND_EDGE_FIRST:ATLAS_SNOW_GROUND_EDGE_FIRST;
            if(id>=first && id<first+ATLAS_MATERIAL_GROUND_EDGE_COUNT)sPhase=9;
            else if(++sDirection<4)SandDirection();else sPhase=0;
            return 0;
        }
        if(sType==ATLAS_GEO_DUNES){sSceneAt=0;sPhase=29;return 0;}
        if(sType==ATLAS_GEO_HEADWATER)
        {sPhase=AtlasWater_HeadwaterNear(sLX,sLY,8)?9:25;return 0;}
        if(sType==ATLAS_GEO_CREEK)
        {sPhase=AtlasWater_CreekNear(sLX,sLY,8)?9:25;return 0;}
        if(sType==ATLAS_GEO_FLOODPLAIN)
        {
            AtlasTerrain_Sample(sLX,sLY,&c);
            struct AtlasVolcano volcano;
            if(c.valley==0 && c.elevation<155 && c.moisture>=132 && c.landform!=ATLAS_COAST
             && !AtlasVolcano_Sample(sLX,sLY,&volcano)){sSceneAt=0;sPhase=30;}
            else if(++sDirection<4)Direction();else sPhase=0;
            return 0;
        }
        if(sType==ATLAS_GEO_VOLCANO){sSceneAt=0;sPhase=24;return 0;}
        if (sType==ATLAS_GEO_GROVE) {sSceneAt=sSceneTarget=0;sPhase=18;return 0;}
        if(MouthTarget()){sSceneAt=sSceneWalls=sSceneTarget=0;sPhase=14;return 0;}
        if(ReliefTarget() || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_VOLCANO)
        {
            if(sType==ATLAS_GEO_VALLEY)
            {
                AtlasTerrain_Sample(sLX,sLY,&c);
                if((AtlasWorld_Generation()<9 && c.elevation<=175) || c.valley>1){sPhase=0;return 0;}
            }
            sSceneAt=sSceneWalls=sSceneTarget=0;sPhase=8;
        }
        else sPhase=9;
    }
    return gAtlasGeoStatus;
}
