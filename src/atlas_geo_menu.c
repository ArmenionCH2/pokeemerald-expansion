#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "bg.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "field_screen_effect.h"
#include "palette.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "constants/rgb.h"

static const struct BgTemplate sBg[]={{.bg=0,.charBaseIndex=0,.mapBaseIndex=31,.screenSize=0,.paletteMode=0,.priority=0,.baseTile=0}};
static const struct WindowTemplate sWindows[]={{.bg=0,.tilemapLeft=1,.tilemapTop=1,.width=27,.height=17,.paletteNum=15,.baseBlock=1},DUMMY_WIN_TEMPLATE};
static const u8 sTitle[] = _("GEO TP - TERRAIN INSPECTOR");
static const u8 sLand[] = _("Landmass (continent / island)");
static const u8 sMountain[] = _("Mountain / natural pass");
static const u8 sLake[] = _("Lake shore");
static const u8 sRiver[] = _("River bank");
static const u8 sCoast[] = _("Coast");
static const u8 sVolcano[] = _("Volcano - coming soon");
static const u8 sReturn[] = _("Return to previous location");
static const u8 sRidge[] = _("Ridge outlook");
static const u8 sValley[] = _("River valley / terraces");
static const u8 sFoothills[] = _("Foothills / low ridges");
static const u8 sMouth[] = _("River mouth");
static const u8 sTributary[] = _("Tributary / small river");
static const u8 *const sLabels[]={sLand,sMountain,sLake,sRiver,sCoast,sVolcano,sReturn,sRidge,sValley,sFoothills,sMouth,sTributary};
static const u8 sX[] = _("X: ");
static const u8 sY[] = _("  Y: ");
static const u8 sPage1[] = _("1/2");
static const u8 sPage2[] = _("2/2");
static const u8 sArrow[] = _(">");
static const u8 sHelp[] = _("A: find and go   B: back / cancel");
static const u8 sSearching[] = _("Searching real terrain...");
static const u8 sMissing[] = _("No safe landing found. Try again.");
static const u8 sSoon[] = _("This geography is not generated yet.");
static const u8 sNoReturn[] = _("No previous teleport in this world.");
static const u8 sColors[]={1,2,3};
EWRAM_DATA static u8 sGeoChoice = 0, sMessage = 0;
EWRAM_DATA static bool8 sSearchingNow = 0, sBookmark = 0;
EWRAM_DATA static s32 sReturnX = 0, sReturnY = 0;
EWRAM_DATA static u32 sReturnSeed = 0;
static void VBlank(void){TransferPlttBuffer();}
static void Print(u8 x,u8 y,const u8 *text){AddTextPrinterParameterized3(0,FONT_SMALL,x,y,sColors,TEXT_SKIP_DRAW,text);}
static void Draw(void)
{
    u8 i,first=(sGeoChoice/6)*6;
    u8 line[48],*end;
    s32 x,y;
    FillWindowPixelBuffer(0,PIXEL_FILL(1));Print(2,0,sTitle);
    Print(196,0,first?sPage2:sPage1);
    for(i=first;i<ATLAS_GEO_COUNT && i<first+6;i++)
    {Print(14,22+13*(i-first),sLabels[i]);if(i==sGeoChoice)Print(2,22+13*(i-first),sArrow);}
    AtlasWorld_GetPosition(&x,&y);end=StringCopy(line,sX);
    end=ConvertIntToDecimalStringN(end,x,STR_CONV_MODE_LEFT_ALIGN,8);
    end=StringCopy(end,sY);ConvertIntToDecimalStringN(end,y,STR_CONV_MODE_LEFT_ALIGN,9);Print(2,103,line);
    Print(2,116,sHelp);
    if(sSearchingNow)Print(2,127,sSearching);
    else if(sMessage)Print(2,127,sMessage==1?sMissing:sMessage==2?sSoon:sNoReturn);
    CopyWindowToVram(0,COPYWIN_FULL);
}
static void Leave(void)
{
    FreeAllWindowBuffers();SetVBlankCallback(NULL);
    gFieldCallback=FieldCB_ReturnToFieldNoScript;
    SetMainCallback1(CB1_Overworld);
    SetMainCallback2(CB2_ReturnToField);
}
static void Go(s32 x,s32 y)
{
    /* The menu must remain alive if the teleport's latitude gate rejects
     * a malformed result; teardown before a failed warp would strand UI. */
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
    {
        sSearchingNow = FALSE;
        sMessage = 1;
        Draw();
        return;
    }
    AtlasWorld_GetPosition(&sReturnX,&sReturnY);sReturnSeed=gAtlasWorldSeed;sBookmark=TRUE;
    FreeAllWindowBuffers();SetVBlankCallback(NULL);
    AtlasWorld_Teleport(x,y);
}
static void GeoMenu_Run(void)
{
    if(JOY_NEW(B_BUTTON))
    {
        if(sSearchingNow){sSearchingNow=FALSE;Draw();}else Leave();
        return;
    }
    if(sSearchingNow)
    {
        u8 status=AtlasGeo_Step();
        if(status==1){Go(gAtlasGeoResult.x,gAtlasGeoResult.y);return;}
        if(status){sSearchingNow=FALSE;sMessage=status==3?2:1;Draw();}
        return;
    }
    if(JOY_NEW(DPAD_UP)){sGeoChoice=sGeoChoice?sGeoChoice-1:ATLAS_GEO_COUNT-1;sMessage=0;Draw();}
    if(JOY_NEW(DPAD_DOWN)){sGeoChoice=(sGeoChoice+1)%ATLAS_GEO_COUNT;sMessage=0;Draw();}
    if(JOY_NEW(A_BUTTON))
    {
        if(sGeoChoice==ATLAS_GEO_RETURN)
        {
            if(sBookmark && sReturnSeed==gAtlasWorldSeed){s32 x=sReturnX,y=sReturnY;Go(x,y);return;}
            sMessage=3;Draw();return;
        }
        if(sGeoChoice==ATLAS_GEO_VOLCANO){sMessage=2;Draw();return;}
        AtlasGeo_Begin(sGeoChoice);sSearchingNow=TRUE;sMessage=0;Draw();
    }
}
void CB2_AtlasGeoMenu(void)
{
    SetMainCallback1(NULL);SetVBlankCallback(NULL);SetGpuReg(REG_OFFSET_DISPCNT,0);
    ResetSpriteData();ResetPaletteFade();ResetBgsAndClearDma3BusyFlags(0);InitBgsFromTemplates(0,sBg,1);
    ChangeBgX(0,0,BG_COORD_SET);ChangeBgY(0,0,BG_COORD_SET);
    SetGpuReg(REG_OFFSET_BLDCNT,0);SetGpuReg(REG_OFFSET_WININ,0);SetGpuReg(REG_OFFSET_WINOUT,0);
    memset((void *)VRAM,0,VRAM_SIZE);
    InitWindows(sWindows);DeactivateAllTextPrinters();PutWindowTilemap(0);
    LoadPalette(gStandardMenuPalette,BG_PLTT_ID(15),PLTT_SIZE_4BPP);
    gPlttBufferUnfaded[0]=RGB(31,31,31);gPlttBufferFaded[0]=RGB(31,31,31);
    sSearchingNow=FALSE;sMessage=0;Draw();ShowBg(0);SetVBlankCallback(VBlank);SetMainCallback2(GeoMenu_Run);
}
