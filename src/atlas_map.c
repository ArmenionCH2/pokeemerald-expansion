#include "global.h"
#include "atlas_world.h"
#include "atlas_geo.h"
#include "atlas_plan.h"
#include "atlas_state.h"
#include "atlas_place.h"
#include "atlas_weather.h"
#include "bg.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/characters.h"

#define WIDTH 56
#define HEIGHT 25
#define TOP 28
enum {LAYER_TERRAIN,LAYER_ELEVATION,LAYER_TEMPERATURE,LAYER_HUMIDITY,LAYER_MOISTURE,LAYER_BASINS,LAYER_WEATHER,LAYER_COUNT};
static const u32 sScales[] = {1, 4, 20, 100, 1000, 10000, 100000, ATLAS_WORLD_WIDTH/WIDTH};
static const struct BgTemplate sBg[] = {{.bg=0, .charBaseIndex=0, .mapBaseIndex=31, .screenSize=0, .paletteMode=0, .priority=0, .baseTile=0}};
static const struct WindowTemplate sWindows[] = {{.bg=0,.tilemapLeft=1,.tilemapTop=0,.width=28,.height=20,.paletteNum=15,.baseBlock=1},DUMMY_WIN_TEMPLATE};
static const u16 sPalette[16] = {RGB_BLACK, RGB_WHITE, RGB(10,20,30), RGB(4,20,6), RGB(24,21,8), RGB(22,27,29), RGB(15,14,12), RGB(8,25,16), RGB(31,10,8), RGB(4,8,22), RGB(30,28,15), RGB(30,17,5), RGB(23,4,4), RGB(4,22,25), RGB(21,9,24), RGB(24,24,24)};
static const u8 sColors[] = {0,1,0};
static const u8 sTitle[] = _("ATLAS EXPLORATION");
static const u8 *const sAtlasMapLayers[] = {COMPOUND_STRING("ATLAS - TERRAIN"),COMPOUND_STRING("ATLAS - ELEVATION"),COMPOUND_STRING("ATLAS - TEMPERATURE"),COMPOUND_STRING("ATLAS - HUMIDITY"),COMPOUND_STRING("ATLAS - SOIL MOISTURE"),COMPOUND_STRING("ATLAS - DRAINAGE BASINS"),COMPOUND_STRING("ATLAS - WEATHER FRONTS")};
static const u8 sHelp[] = _("D-pad: pan  L/R: zoom  A: home  B: back");
static const u8 sAdminHelp[] = _("L/R zoom  START layer  SELECT player");
static const u8 sUnknown[] = _("Unexplored");
static const u8 *const sLegends[] = {COMPOUND_STRING(""),COMPOUND_STRING("Low: green   High: gray"),COMPOUND_STRING("Cold: blue   Hot: red"),COMPOUND_STRING("Dry: sand   Humid: blue"),COMPOUND_STRING("Dry: sand   Wet: blue"),COMPOUND_STRING("Colors separate drainage catchments"),COMPOUND_STRING("Dry front: sand   Wet front: blue")};
static const u8 sFull[] = _("Journal space low - visits retained");
static const u8 sClear[] = _("Clear");
static const u8 sRain[] = _("Rain");
static const u8 sSnow[] = _("Snow");
static const u8 sFog[] = _("Fog");
static const u8 *const sWeather[] = {sClear, sRain, sSnow, sFog};
static s32 sX, sY;
static u8 sAtlasMapZoom, sAtlasMapRow, sChunk, sAtlasMapLayer, sAtlasMapColumn;
static u16 sTile;
static bool8 sAdminView;
EWRAM_DATA bool8 gAtlasMapActive = FALSE;
static u8 sPainted[(WIDTH * HEIGHT + 7) / 8];

static void VBlank(void) { LoadOam(); ProcessSpriteCopyRequests(); TransferPlttBuffer(); }
static s32 Wrap(s32 x) { x %= ATLAS_WORLD_WIDTH; return x < 0 ? x + ATLAS_WORLD_WIDTH : x; }
static struct AtlasCell Sample(s32 x, s32 y)
{
    struct AtlasCell cell;
    if (sAdminView && sScales[sAtlasMapZoom]>=100)
    {
        struct AtlasPlanSample plan;
        struct AtlasHabitat habitat;
        // A continental pixel summarizes macro notes, not a remote metre of
        // detailed riverbank geometry. Never use this shortcut for player fog.
        AtlasPlan_SampleDetail(Wrap(x),y,&plan);
        cell.elevation=plan.height;
        cell.waterform=plan.height<ATLAS_PLAN_SEA?ATLAS_SEA:plan.lake?ATLAS_LAKE:ATLAS_DRY;
        AtlasHabitat_Sample(y,plan.height,plan.rain,plan.humidity,cell.waterform,&habitat);
        cell.temperature=habitat.temperature;cell.humidity=habitat.humidity;cell.moisture=habitat.moisture;
        cell.landform=plan.height<128?ATLAS_COAST:plan.height>185?ATLAS_MOUNTAIN:plan.height>155?ATLAS_HILL:ATLAS_PLAIN;
        cell.biome=cell.waterform==ATLAS_SEA?ATLAS_OCEAN:cell.waterform!=ATLAS_DRY?ATLAS_WETLAND
            :cell.temperature<45?ATLAS_TUNDRA:cell.temperature<85?ATLAS_TAIGA
            :plan.height<132 && cell.moisture>160?ATLAS_WETLAND
            :cell.temperature>160 && cell.moisture<98?ATLAS_DESERT
            :cell.temperature>175 && cell.moisture>155?ATLAS_JUNGLE
            :cell.moisture>132?ATLAS_FOREST:cell.temperature>150 && cell.moisture<125?ATLAS_SAVANNA:ATLAS_GRASSLAND;
    }
    else AtlasTerrain_Sample(Wrap(x), y, &cell);
    return cell;
}
static u8 Color(s32 x, s32 y)
{
    struct AtlasCell cell=Sample(x,y);
    if (sAtlasMapLayer==LAYER_TEMPERATURE)
        return cell.temperature<45?5:cell.temperature<85?9:cell.temperature<135?7:cell.temperature<175?4:cell.temperature<210?11:8;
    if (sAtlasMapLayer==LAYER_HUMIDITY || sAtlasMapLayer==LAYER_MOISTURE || sAtlasMapLayer==LAYER_WEATHER)
    {
        u8 value=sAtlasMapLayer==LAYER_HUMIDITY?cell.humidity:sAtlasMapLayer==LAYER_MOISTURE?cell.moisture
            :255-AtlasWeather_Potential(&gSaveBlock3Ptr->atlas,x,y);
        return value<70?4:value<110?10:value<150?3:value<190?7:2;
    }
    if (sAtlasMapLayer==LAYER_ELEVATION)
        return cell.waterform!=ATLAS_DRY?2:cell.elevation<145?3:cell.elevation<170?4:cell.elevation<195?11:6;
    if (sAtlasMapLayer==LAYER_BASINS)
    {
        struct AtlasPlace place;
        if (cell.waterform!=ATLAS_DRY) return cell.waterform==ATLAS_SEA?2:9;
        if (!AtlasPlace_Identify(x,y,&cell,&place) || !place.basin) return 6;
        return 3+place.basin%13;
    }
    if (cell.waterform != ATLAS_DRY) return 2;
    if (cell.biome == ATLAS_TUNDRA || cell.biome == ATLAS_TAIGA) return 5;
    if (cell.landform == ATLAS_MOUNTAIN) return 6;
    if (cell.biome == ATLAS_DESERT || cell.landform == ATLAS_COAST) return 4;
    return cell.biome == ATLAS_WETLAND ? 7 : 3;
}
static bool8 Pixel(u32 px, u32 py, s32 x, s32 y)
{
    u32 bit = py * WIDTH + px;
    if (sPainted[bit / 8] & (1 << (bit % 8))) return TRUE;
    if ((!sAdminView || sScales[sAtlasMapZoom]<100) && !AtlasWater_Prewarm(x,y)) return FALSE;
    sPainted[bit / 8] |= 1 << (bit % 8);
    FillWindowPixelRect(0, PIXEL_FILL(Color(x,y)), px*4, TOP+py*4, 4, 4);
    return TRUE;
}
static void Marker(s32 x, s32 y, u8 color)
{
    s32 dx = Wrap(x) - sX, px, py;
    if (dx > ATLAS_WORLD_WIDTH/2) dx -= ATLAS_WORLD_WIDTH;
    if (dx < -ATLAS_WORLD_WIDTH/2) dx += ATLAS_WORLD_WIDTH;
    px = AtlasTerrain_FloorDiv(dx, sScales[sAtlasMapZoom]) + WIDTH/2;
    py = AtlasTerrain_FloorDiv(y-sY, sScales[sAtlasMapZoom]) + HEIGHT/2;
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
        FillWindowPixelRect(0, PIXEL_FILL(color), px*4+1, TOP+py*4+1, 2, 2);
}
static void Draw(void)
{
    s32 x,y;
    struct AtlasCell cell;
    struct AtlasSaveState *state = &gSaveBlock3Ptr->atlas;
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    memset(sPainted,0,sizeof(sPainted));
    AddTextPrinterParameterized3(0,FONT_SMALL,0,0,sColors,TEXT_SKIP_DRAW,sAdminView?sAtlasMapLayers[sAtlasMapLayer]:sTitle);
    AddTextPrinterParameterized3(0,FONT_SMALL,0,144,sColors,TEXT_SKIP_DRAW,sAdminView?sAdminHelp:sHelp);
    if (sAdminView || AtlasState_Explored(state,sX,sY))
    {
        struct AtlasPlace place;
        char name[ATLAS_PLACE_NAME_BYTES];
        u8 encoded[ATLAS_PLACE_NAME_BYTES];unsigned i;
        cell=Sample(sX,sY);
        if (AtlasPlace_Identify(sX,sY,&cell,&place)
         && (AtlasPlace_Name(&place,1,name) || AtlasPlace_Name(&place,0,name)))
        {
            for (i=0;name[i] && i+1<sizeof(encoded);i++)
                encoded[i]=name[i]>='A' && name[i]<='Z'?CHAR_A+name[i]-'A':name[i]>='a' && name[i]<='z'?CHAR_a+name[i]-'a':CHAR_SPACE;
            encoded[i]=EOS;
            AddTextPrinterParameterized3(0,FONT_SMALL,0,12,sColors,TEXT_SKIP_DRAW,encoded);
        }
    }
    else AddTextPrinterParameterized3(0,FONT_SMALL,0,12,sColors,TEXT_SKIP_DRAW,sUnknown);
    AtlasWorld_GetPosition(&x,&y);
    AtlasTerrain_Sample(x,y,&cell);
    AddTextPrinterParameterized3(0,FONT_SMALL,0,132,sColors,TEXT_SKIP_DRAW,
        AtlasState_UsedBytes(state) > ATLAS_JOURNAL_BYTES - 57 ? sFull
        : sAdminView && sAtlasMapLayer!=LAYER_TERRAIN ? sLegends[sAtlasMapLayer]
        : sWeather[AtlasState_Weather(state,x,y,cell.temperature,cell.humidity)]);
    sAtlasMapRow = sAtlasMapColumn = sChunk = sTile = 0;
    CopyWindowToVram(0,COPYWIN_FULL);
}
static void Paint(void)
{
    struct AtlasSaveState *state = &gSaveBlock3Ptr->atlas;
    s32 x,y,px,py,dx;
    u32 bit;
    if (sAdminView)
    {
        if (sAtlasMapRow >= HEIGHT) return;
        unsigned budget=sScales[sAtlasMapZoom]>=100?8:1;
        while (budget-- && sAtlasMapRow<HEIGHT)
        {
            y = sY + ((s32)sAtlasMapRow-HEIGHT/2) * (s32)sScales[sAtlasMapZoom];
            if (y >= -ATLAS_WORLD_HALF_HEIGHT && y < ATLAS_WORLD_HALF_HEIGHT)
                if (!Pixel(sAtlasMapColumn,sAtlasMapRow,Wrap(sX+((s32)sAtlasMapColumn-WIDTH/2)*(s32)sScales[sAtlasMapZoom]),y)) break;
            if (++sAtlasMapColumn==WIDTH) {sAtlasMapColumn=0;sAtlasMapRow++;}
        }
    }
    else
    {
        if (sChunk >= state->count) return;
        /* Only project genuinely visited tiles. Coarse zoom shows their marks,
         * never samples an unseen representative point for a whole region. */
        struct AtlasVisitedChunk decoded;
        const struct AtlasVisitedChunk *chunk = &decoded;
        if (!AtlasState_GetChunk(state,sChunk,&decoded)) return;
        for (bit=sTile;bit<400;bit++)
        {
            if (!(chunk->tiles[bit/8] & (1 << (bit%8)))) continue;
            x=chunk->x*20+bit%20; y=chunk->y*20+bit/20;
            dx=x-sX;
            if (dx>ATLAS_WORLD_WIDTH/2) dx-=ATLAS_WORLD_WIDTH;
            if (dx< -ATLAS_WORLD_WIDTH/2) dx+=ATLAS_WORLD_WIDTH;
            px=AtlasTerrain_FloorDiv(dx,sScales[sAtlasMapZoom])+WIDTH/2;
            py=AtlasTerrain_FloorDiv(y-sY,sScales[sAtlasMapZoom])+HEIGHT/2;
            if (px>=0 && px<WIDTH && py>=0 && py<HEIGHT && !Pixel(px,py,x,y)) break;
        }
        sTile=bit;
        if (sTile==400) {sTile=0;sChunk++;}
    }
    AtlasWorld_GetPosition(&x,&y);
    Marker(x,y,8);
    FillWindowPixelRect(0,PIXEL_FILL(1),WIDTH*2,TOP+HEIGHT*2,1,3);
    CopyWindowToVram(0,COPYWIN_GFX);
}
static void Run(void)
{
    bool8 changed = FALSE;
    s32 step = sScales[sAtlasMapZoom]*8;
    if (JOY_NEW(B_BUTTON))
    {
        gAtlasMapActive = FALSE;
        FreeAllWindowBuffers(); SetVBlankCallback(NULL);
        SetMainCallback2(CB2_ReturnToFieldWithOpenMenu); return;
    }
    if (JOY_REPEAT(DPAD_LEFT)) sX=Wrap(sX-step),changed=TRUE;
    if (JOY_REPEAT(DPAD_RIGHT)) sX=Wrap(sX+step),changed=TRUE;
    if (JOY_REPEAT(DPAD_UP)) sY-=step,changed=TRUE;
    if (JOY_REPEAT(DPAD_DOWN)) sY+=step,changed=TRUE;
    if (sY < -ATLAS_WORLD_HALF_HEIGHT) sY=-ATLAS_WORLD_HALF_HEIGHT;
    if (sY >= ATLAS_WORLD_HALF_HEIGHT) sY=ATLAS_WORLD_HALF_HEIGHT-1;
    if (JOY_NEW(L_BUTTON) && sAtlasMapZoom>0) sAtlasMapZoom--,changed=TRUE;
    if (JOY_NEW(R_BUTTON) && sAtlasMapZoom+1<ARRAY_COUNT(sScales)) sAtlasMapZoom++,changed=TRUE;
    if (JOY_NEW(A_BUTTON)) AtlasWorld_GetPosition(&sX,&sY),changed=TRUE;
#ifndef NDEBUG
    if (JOY_NEW(SELECT_BUTTON)) {sAdminView=!sAdminView;if (!sAdminView) sAtlasMapLayer=LAYER_TERRAIN;changed=TRUE;}
    if (sAdminView && JOY_NEW(START_BUTTON)) sAtlasMapLayer=(sAtlasMapLayer+1)%LAYER_COUNT,changed=TRUE;
#endif
    if (changed) Draw();
    Paint();
}
void CB2_AtlasMap(void)
{
    gAtlasMapActive = TRUE;
    SetVBlankCallback(NULL); SetGpuReg(REG_OFFSET_DISPCNT,0);
    ResetSpriteData(); ResetPaletteFade(); ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0,sBg,1); InitWindows(sWindows);
    DeactivateAllTextPrinters(); PutWindowTilemap(0);
    LoadPalette(sPalette,BG_PLTT_ID(15),sizeof(sPalette));
    sAtlasMapZoom=0; sAtlasMapLayer=LAYER_TERRAIN; sAdminView=FALSE;
    AtlasWorld_GetPosition(&sX,&sY);
    Draw(); ShowBg(0); SetVBlankCallback(VBlank); SetMainCallback2(Run);
}
