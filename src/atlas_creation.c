#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_recipe.h"
#include "bg.h"
#include "gpu_regs.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "new_game.h"
#include "overworld.h"
#include "palette.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "constants/rgb.h"

EWRAM_DATA u32 gAtlasGenerationFrames = 0;
EWRAM_DATA u32 gAtlasGenerationStageFrames[ATLAS_PLAN_STAGES] = {0};
EWRAM_DATA u32 gAtlasGenerationStartFrame = 0;
EWRAM_DATA u8 gAtlasCreationState = 0;
static bool8 sRestoring, sFailed;
static u8 sLastPercent, sLastStage;
static const struct BgTemplate sBg[] = {{.bg=0, .charBaseIndex=0, .mapBaseIndex=31, .screenSize=0, .paletteMode=0, .priority=0, .baseTile=0}};
static const struct WindowTemplate sWindows[] = {{.bg=0, .tilemapLeft=1, .tilemapTop=1, .width=28, .height=18, .paletteNum=15, .baseBlock=1}, DUMMY_WIN_TEMPLATE};
static const u8 sTitle[] = _("ATLAS - CREATING WORLD");
static const u8 sRestore[] = _("ATLAS - RESTORING WORLD");
static const u8 sLand[] = _("Outlining land and oceans");
static const u8 sRanges[] = _("Shaping mountain ranges");
static const u8 sClimate[] = _("Calculating climate");
static const u8 sOutlets[] = _("Locating ocean outlets");
static const u8 sBasins[] = _("Connecting drainage basins");
static const u8 sRunoff[] = _("Accumulating river runoff");
static const u8 sIdentity[] = _("Identifying landmasses");
static const u8 sVerify[] = _("Checking world notes");
static const u8 sDrainageNames[] = _("Identifying river basins");
static const u8 sLakes[] = _("Resolving lakes and spillways");
static const u8 sHabitats[] = _("Deriving seasonal habitats");
static const u8 sStarting[] = _("Preparing starting terrain");
static const u8 sBadSave[] = _("Incompatible Atlas save.\nBack up this save before starting\na new world.");
static bool8 sInvalidSave;
static const u8 sFailure[] = _("Not enough free memory.\nRestart the game to retry.");
static const u8 sSeed[] = _("Seed: ");
static const u8 sPercent[] = _("%");
static const u8 sElapsed[] = _("Elapsed seconds: ");
static const u8 *const sStages[] = {sLand,sRanges,sClimate,sOutlets,sBasins,sRunoff,sDrainageNames,sIdentity,sLakes,sHabitats,sVerify,sStarting};
static const u8 sColors[] = {0,2,3};

static void VBlank(void) { LoadOam(); ProcessSpriteCopyRequests(); TransferPlttBuffer(); }
static void Print(u8 y, const u8 *text)
{
    AddTextPrinterParameterized3(0, FONT_NORMAL, 2, y, sColors, TEXT_SKIP_DRAW, text);
}
static void Draw(void)
{
    u8 line[64], *end;
    u8 stage = AtlasPlan_Stage(), percent = AtlasPlan_Progress();
    FillWindowPixelBuffer(0, PIXEL_FILL(1));
    Print(4, sRestoring ? sRestore : sTitle);
    Print(30, sFailed ? (sInvalidSave ? sBadSave : sFailure) : sStages[stage > ATLAS_PLAN_STAGES ? ATLAS_PLAN_STAGES : stage]);
    if (!sFailed)
    {
        end = ConvertIntToDecimalStringN(line, percent, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringCopy(end, sPercent); Print(54, line);
        FillWindowPixelRect(0, PIXEL_FILL(3), 2, 77, 214, 6);
        if (percent) FillWindowPixelRect(0, PIXEL_FILL(2), 2, 77, percent * 214 / 100, 6);
    }
    end = StringCopy(line, sSeed);
    ConvertUIntToDecimalStringN(end, gAtlasWorldSeed, STR_CONV_MODE_LEFT_ALIGN, 10); Print(94, line);
    end = StringCopy(line, sElapsed);
    ConvertIntToDecimalStringN(end, (gMain.vblankCounter1 - gAtlasGenerationStartFrame) / 60, STR_CONV_MODE_LEFT_ALIGN, 4); Print(114, line);
    CopyWindowToVram(0, COPYWIN_FULL);
    sLastPercent = percent; sLastStage = stage;
}
static void Run(void)
{
    u8 stage = AtlasPlan_Stage();
    u32 now = gMain.vblankCounter1;
    if (!sFailed)
    {
        if (AtlasPlan_Step(256))
        {
            if (!AtlasRecipe_VerifyRestore())
            {
                sFailed = !AtlasPlan_Begin(AtlasWorld_ReadSeed());
                Draw(); return;
            }
            if (gAtlasCreationState == 1)
            {
                gAtlasGenerationFrames = now - gAtlasGenerationStartFrame;
                gAtlasCreationState = 2; Draw(); return;
            }
            FreeAllWindowBuffers(); SetVBlankCallback(NULL); SetGpuReg(REG_OFFSET_DISPCNT, 0);
            if (!sRestoring) AtlasWorld_FinishNewGame();
            else AtlasWorld_RepairLanding();
            gAtlasGenerationFrames = gMain.vblankCounter1 - gAtlasGenerationStartFrame;
            gAtlasCreationState = 3; gMain.state = 0;
            SetMainCallback2(sRestoring ? CB2_ContinueSavedGame : CB2_NewGame);
            return;
        }
        if (stage < ATLAS_PLAN_STAGES) gAtlasGenerationStageFrames[stage]++;
    }
    if (sLastPercent != AtlasPlan_Progress() || sLastStage != AtlasPlan_Stage() || now % 60 == 0) Draw();
}
static void Begin(bool8 restoring)
{
    sRestoring = restoring;
    SetMainCallback1(NULL); SetVBlankCallback(NULL); StopMapMusic();
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    ResetSpriteData(); ResetPaletteFade(); ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBg, 1);
    ChangeBgX(0, 0, BG_COORD_SET); ChangeBgY(0, 0, BG_COORD_SET);
    SetGpuReg(REG_OFFSET_BLDCNT, 0); SetGpuReg(REG_OFFSET_WININ, 0); SetGpuReg(REG_OFFSET_WINOUT, 0);
    memset((void *)VRAM, 0, VRAM_SIZE);
    if (!restoring) NewGameInitData();
    AtlasPlan_ReserveHeap();
    AtlasRecipe_Reset();
    sInvalidSave = restoring && !AtlasWorld_SaveValid();
    sFailed = sInvalidSave || !AtlasPlan_Begin(AtlasWorld_ReadSeed());
    if (restoring && !sFailed) AtlasRecipe_Load(AtlasWorld_ReadSeed());
    InitWindows(sWindows); DeactivateAllTextPrinters(); PutWindowTilemap(0);
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    gPlttBufferUnfaded[0] = RGB(31,31,31); gPlttBufferFaded[0] = RGB(31,31,31);
    gAtlasGenerationStartFrame = gMain.vblankCounter1; gAtlasGenerationFrames = 0;
    memset(gAtlasGenerationStageFrames, 0, sizeof(gAtlasGenerationStageFrames));
    gAtlasCreationState = 1; Draw(); ShowBg(0); SetVBlankCallback(VBlank); SetMainCallback2(Run);
}
void CB2_AtlasCreateWorld(void) { Begin(FALSE); }
void CB2_AtlasRestoreWorld(void) { Begin(TRUE); }
