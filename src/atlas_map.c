#include "global.h"
#include "atlas_world.h"
#include "atlas_geo.h"
#include "atlas_plan.h"
#include "atlas_state.h"
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

#define WIDTH 56
#define HEIGHT 28
#define TOP 20
static const u32 sScales[] = {1, 4, 20, 100, 1000, 10000, 100000, 1000000};
static const struct BgTemplate sBg[] = {{.bg=0, .charBaseIndex=0, .mapBaseIndex=31, .screenSize=0, .paletteMode=0, .priority=0, .baseTile=0}};
static const struct WindowTemplate sWindows[] = {{.bg=0,.tilemapLeft=1,.tilemapTop=0,.width=28,.height=20,.paletteNum=15,.baseBlock=1},DUMMY_WIN_TEMPLATE};
static const u16 sPalette[16] = {RGB_BLACK, RGB_WHITE, RGB(10,20,30), RGB(4,20,6), RGB(24,21,8), RGB(22,27,29), RGB(15,14,12), RGB(8,25,16), RGB(31,10,8)};
static const u8 sColors[] = {0,1,0};
static const u8 sTitle[] = _("ATLAS EXPLORATION");
static const u8 sAdmin[] = _("ATLAS ADMIN MAP");
static const u8 sHelp[] = _("D-pad: pan  L/R: zoom  A: home  B: back");
static const u8 sFull[] = _("Journal space low - visits retained");
static const u8 sClear[] = _("Clear");
static const u8 sRain[] = _("Rain");
static const u8 sSnow[] = _("Snow");
static const u8 sFog[] = _("Fog");
static const u8 *const sWeather[] = {sClear, sRain, sSnow, sFog};
static s32 sX, sY;
static u8 sZoom, sRow, sChunk;
static bool8 sAdminView;
EWRAM_DATA bool8 gAtlasMapActive = FALSE;
static u8 sPainted[(WIDTH * HEIGHT + 7) / 8];

static void VBlank(void) { LoadOam(); ProcessSpriteCopyRequests(); TransferPlttBuffer(); }
static s32 Wrap(s32 x) { x %= ATLAS_WORLD_WIDTH; return x < 0 ? x + ATLAS_WORLD_WIDTH : x; }
static u8 Color(s32 x, s32 y)
{
    struct AtlasCell cell;
    AtlasTerrain_Sample(Wrap(x), y, &cell);
    if (cell.waterform != ATLAS_DRY) return 2;
    if (cell.biome == ATLAS_TUNDRA || cell.biome == ATLAS_TAIGA) return 5;
    if (cell.landform == ATLAS_MOUNTAIN) return 6;
    if (cell.biome == ATLAS_DESERT || cell.landform == ATLAS_COAST) return 4;
    return cell.biome == ATLAS_WETLAND ? 7 : 3;
}
static void Pixel(u32 px, u32 py, s32 x, s32 y)
{
    u32 bit = py * WIDTH + px;
    if (sPainted[bit / 8] & (1 << (bit % 8))) return;
    sPainted[bit / 8] |= 1 << (bit % 8);
    FillWindowPixelRect(0, PIXEL_FILL(Color(x,y)), px*4, TOP+py*4, 4, 4);
}
static void Marker(s32 x, s32 y, u8 color)
{
    s32 dx = Wrap(x) - sX, px, py;
    if (dx > ATLAS_WORLD_WIDTH/2) dx -= ATLAS_WORLD_WIDTH;
    if (dx < -ATLAS_WORLD_WIDTH/2) dx += ATLAS_WORLD_WIDTH;
    px = AtlasTerrain_FloorDiv(dx, sScales[sZoom]) + WIDTH/2;
    py = AtlasTerrain_FloorDiv(y-sY, sScales[sZoom]) + HEIGHT/2;
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
    AddTextPrinterParameterized3(0,FONT_SMALL,0,0,sColors,TEXT_SKIP_DRAW,sAdminView?sAdmin:sTitle);
    AddTextPrinterParameterized3(0,FONT_SMALL,0,144,sColors,TEXT_SKIP_DRAW,sHelp);
    AtlasWorld_GetPosition(&x,&y);
    AtlasTerrain_Sample(x,y,&cell);
    AddTextPrinterParameterized3(0,FONT_SMALL,0,132,sColors,TEXT_SKIP_DRAW,
        AtlasState_UsedBytes(state) > ATLAS_JOURNAL_BYTES - 57 ? sFull : sWeather[AtlasState_Weather(state,x,y,cell.temperature,cell.humidity)]);
    sRow = sChunk = 0;
    CopyWindowToVram(0,COPYWIN_FULL);
}
static void Paint(void)
{
    struct AtlasSaveState *state = &gSaveBlock3Ptr->atlas;
    s32 x,y,px,py,dx;
    u32 bit;
    if (sAdminView)
    {
        if (sRow >= HEIGHT) return;
        y = sY + ((s32)sRow-HEIGHT/2) * (s32)sScales[sZoom];
        if (y >= -ATLAS_WORLD_HALF_HEIGHT && y < ATLAS_WORLD_HALF_HEIGHT)
            for (px=0;px<WIDTH;px++)
                Pixel(px,sRow,Wrap(sX+(px-WIDTH/2)*(s32)sScales[sZoom]),y);
        sRow++;
    }
    else
    {
        if (sChunk >= state->count) return;
        /* Only project genuinely visited tiles. Coarse zoom shows their marks,
         * never samples an unseen representative point for a whole region. */
        struct AtlasVisitedChunk decoded;
        const struct AtlasVisitedChunk *chunk = &decoded;
        if (!AtlasState_GetChunk(state,sChunk++,&decoded)) return;
        for (bit=0;bit<400;bit++)
        {
            if (!(chunk->tiles[bit/8] & (1 << (bit%8)))) continue;
            x=chunk->x*20+bit%20; y=chunk->y*20+bit/20;
            dx=x-sX;
            if (dx>ATLAS_WORLD_WIDTH/2) dx-=ATLAS_WORLD_WIDTH;
            if (dx< -ATLAS_WORLD_WIDTH/2) dx+=ATLAS_WORLD_WIDTH;
            px=AtlasTerrain_FloorDiv(dx,sScales[sZoom])+WIDTH/2;
            py=AtlasTerrain_FloorDiv(y-sY,sScales[sZoom])+HEIGHT/2;
            if (px>=0 && px<WIDTH && py>=0 && py<HEIGHT) Pixel(px,py,x,y);
        }
    }
    AtlasWorld_GetPosition(&x,&y);
    Marker(x,y,8);
    FillWindowPixelRect(0,PIXEL_FILL(1),WIDTH*2,TOP+HEIGHT*2,1,3);
    CopyWindowToVram(0,COPYWIN_GFX);
}
static void Run(void)
{
    bool8 changed = FALSE;
    s32 step = sScales[sZoom]*8;
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
    if (JOY_NEW(L_BUTTON) && sZoom>0) sZoom--,changed=TRUE;
    if (JOY_NEW(R_BUTTON) && sZoom+1<ARRAY_COUNT(sScales)) sZoom++,changed=TRUE;
    if (JOY_NEW(A_BUTTON)) AtlasWorld_GetPosition(&sX,&sY),changed=TRUE;
#ifndef NDEBUG
    if (JOY_NEW(SELECT_BUTTON)) sAdminView=!sAdminView,changed=TRUE;
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
    sZoom=0; sAdminView=FALSE;
    AtlasWorld_GetPosition(&sX,&sY);
    Draw(); ShowBg(0); SetVBlankCallback(VBlank); SetMainCallback2(Run);
}
