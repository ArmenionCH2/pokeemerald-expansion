#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_tiles.h"

EWRAM_DATA struct AtlasGeoResult gAtlasGeoResult = {0};
EWRAM_DATA u8 gAtlasGeoStatus = 0;
EWRAM_DATA static u16 sNext[ATLAS_GEO_COUNT] = {0}, sVisited = 0, sNode = 0, sLastLand = 0, sTrial = 0;
EWRAM_DATA static u8 sType = 0, sPhase = 0, sDirection = 0, sCheck = 0, sAttempt = 0;
EWRAM_DATA static s32 sCX = 0, sCY = 0, sWX = 0, sWY = 0, sDX = 0, sDY = 0, sLX = 0, sLY = 0;
EWRAM_DATA static s32 sMouthDX = 0, sMouthDY = 0;
EWRAM_DATA static u32 sSeed = 0;
EWRAM_DATA static bool8 sInitialized = 0;
EWRAM_DATA static u8 sSceneAt = 0, sSceneWalls = 0, sSceneTarget = 0;
static s32 Abs(s32 n){return n<0?-n:n;}
static s32 Wrap(s32 x){x%=ATLAS_WORLD_WIDTH;return x<0?x+ATLAS_WORLD_WIDTH:x;}
static bool8 ReliefTarget(void){return sType==ATLAS_GEO_MOUNTAIN || sType==ATLAS_GEO_RIDGE || sType==ATLAS_GEO_FOOTHILLS;}
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
    return c.waterform==(sType==ATLAS_GEO_LAKE?ATLAS_LAKE:(sType==ATLAS_GEO_RIVER || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_MOUTH || sType==ATLAS_GEO_TRIBUTARY)?ATLAS_RIVER:ATLAS_SEA);
}
static void Direction(void)
{
    s32 distance = (sType==ATLAS_GEO_RIVER || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_MOUTH || sType==ATLAS_GEO_TRIBUTARY)?512:ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
    if(sType==ATLAS_GEO_MOUTH)sTrial=0;
    sDX=sCX;sDY=sCY;sWX=sCX;sWY=sCY;
    if(sDirection==0)sDX+=distance;
    if(sDirection==1)sDX-=distance;
    if(sDirection==2)sDY+=distance;
    if(sDirection==3)sDY-=distance;
    sPhase=2;
}
void AtlasGeo_Begin(u8 type)
{
    if(!sInitialized || sSeed!=gAtlasWorldSeed)
    {
        u8 i;
        for(i=0;i<ATLAS_GEO_COUNT;i++)sNext[i]=ATLAS_PLAN_WIDTH*(ATLAS_PLAN_HEIGHT/2)+(gAtlasWorldSeed%ATLAS_PLAN_WIDTH)+i*13;
        sLastLand=0;sSeed=gAtlasWorldSeed;sInitialized=TRUE;
    }
    sType=type;sPhase=0;sVisited=0;sDirection=0;
    gAtlasGeoStatus=type<ATLAS_GEO_COUNT && type!=ATLAS_GEO_VOLCANO && type!=ATLAS_GEO_RETURN?0:3;
}
static bool8 Candidate(u16 id)
{
    struct AtlasPlanNode *n=&gAtlasPlan[id];
    if (sType>=ATLAS_GEO_FOREST)
    {
        struct AtlasHabitat habitat;
        s32 y=(id/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
        if (n->height<ATLAS_PLAN_SEA || n->height>180) return FALSE;
        AtlasHabitat_Sample(y,n->height,n->rain,n->humidity,ATLAS_DRY,&habitat);
        if (sType==ATLAS_GEO_FOREST) return habitat.temperature>=85 && habitat.moisture>125;
        if (sType==ATLAS_GEO_DESERT) return habitat.temperature>155 && habitat.moisture<105;
        if (sType==ATLAS_GEO_TUNDRA) return habitat.temperature<60;
        return habitat.temperature>75 && habitat.moisture>150 && n->height<140;
    }
    if(sType==ATLAS_GEO_LAND)return n->height>125 && n->height<180 && n->continent && n->continent!=sLastLand;
    if(sType==ATLAS_GEO_MOUNTAIN)return n->height>=195;
    if(sType==ATLAS_GEO_RIDGE)return n->height>=218;
    if(sType==ATLAS_GEO_FOOTHILLS)return n->height>=172 && n->height<=184;
    if(sType==ATLAS_GEO_TRIBUTARY)return AtlasWater_IsTributary(id);
    if(sType==ATLAS_GEO_MOUTH)return n->height>=ATLAS_PLAN_SEA && n->runoff>=600 && n->parent!=id && n->parent<ATLAS_PLAN_COUNT && gAtlasPlan[n->parent].height<ATLAS_PLAN_SEA;
    if(sType==ATLAS_GEO_VALLEY)return n->height>=175 && n->runoff>=600 && n->parent!=id && n->parent<ATLAS_PLAN_COUNT;
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
    if(sPhase==0)
    {
        for(i=0;i<24;i++)
        {
            if(sVisited++>=ATLAS_PLAN_COUNT){gAtlasGeoStatus=2;return 2;}
            sNode=sNext[sType]++;if(sNext[sType]>=ATLAS_PLAN_COUNT)sNext[sType]=0;
            if(!Candidate(sNode))continue;
            sCX=(u32)(sNode%ATLAS_PLAN_WIDTH)*ATLAS_WORLD_WIDTH/ATLAS_PLAN_WIDTH;
            sCY=(sNode/ATLAS_PLAN_WIDTH)*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT;
            if(sType==ATLAS_GEO_RIVER || sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_MOUTH || sType==ATLAS_GEO_TRIBUTARY){u16 height;AtlasWater_Point(sNode,2,&sCX,&sCY,&height);}
            if(sType==ATLAS_GEO_MOUTH){sTrial=0;sPhase=11;return 0;}
            if(ReliefTarget()){sTrial=0;sPhase=sType==ATLAS_GEO_FOOTHILLS?10:6;return 0;}
            if (sType>=ATLAS_GEO_FOREST) {sTrial=0;sPhase=17;return 0;}
            sDirection=0;sPhase=1;return 0;
        }
        return 0;
    }
    if(sPhase==11)
    {
        u8 status=AtlasWater_MouthStep(sNode,&sCX,&sCY,&sMouthDX,&sMouthDY);
        if(!status)return 0;
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
         * camera rectangle, one resumable point per inspector update. */
        x=sLX+(s32)(sSceneAt%13)-6;y=sLY+(s32)(sSceneAt/13)-4;
        if(!Warm(x,y))return 0;
        AtlasTerrain_Sample(x,y,&c);
        if(c.waterform==ATLAS_SEA)sSceneWalls=1;
        if(c.waterform==ATLAS_RIVER)sSceneTarget=1;
        if(sSceneWalls && sSceneTarget){sPhase=9;return 0;}
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
            s32 offset=sType==ATLAS_GEO_MOUTH?2:4;
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
        u8 target=sType==ATLAS_GEO_FOREST?ATLAS_FOREST:sType==ATLAS_GEO_DESERT?ATLAS_DESERT:sType==ATLAS_GEO_TUNDRA?ATLAS_TUNDRA:ATLAS_WETLAND;
        if (c.waterform || c.biome!=target) {if (sTrial==1) sPhase=0;return 0;}
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
        if((tile&1023)>=924 && (tile&1023)<1018)sSceneWalls++;
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
            if(sSceneAt==22 && !sSceneWalls){sPhase=ReliefTarget()?6:0;return 0;}
            if(sSceneAt<(sType==ATLAS_GEO_MOUNTAIN?139:22))return 0;
            if(!sSceneWalls || ((sType==ATLAS_GEO_VALLEY || sType==ATLAS_GEO_MOUNTAIN) && !sSceneTarget))
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
        if (sType>=ATLAS_GEO_FOREST) {sPhase=17;return 0;}
        if(++sAttempt<4 && sType!=ATLAS_GEO_LAND && !ReliefTarget())
        {sLX+=sDX>sWX?3:sDX<sWX?-3:0;sLY+=sDY>sWY?3:sDY<sWY?-3:0;sCheck=0;return 0;}
        if(sType==ATLAS_GEO_MOUTH && ++sDirection<4)Direction();
        else sPhase=ReliefTarget()?6:0;
        return 0;
    }
    if(++sCheck==9)
    {
        if(sType==ATLAS_GEO_MOUTH){sSceneAt=sSceneWalls=sSceneTarget=0;sPhase=14;return 0;}
        if(ReliefTarget() || sType==ATLAS_GEO_VALLEY)
        {
            if(sType==ATLAS_GEO_VALLEY)
            {
                AtlasTerrain_Sample(sLX,sLY,&c);
                if(c.elevation<=175 || c.valley>1){sPhase=0;return 0;}
            }
            sSceneAt=sSceneWalls=sSceneTarget=0;sPhase=8;
        }
        else sPhase=9;
    }
    return gAtlasGeoStatus;
}
