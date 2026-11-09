#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "random.h"
#include "main.h"
#include "script.h"
#include "fieldmap.h"
#include "overworld.h"
#include "event_data.h"
#include "string_util.h"
#include "wild_encounter.h"
#include "atlas_geo.h"
#include "atlas_state.h"
#include "atlas_weather.h"
#include "field_weather.h"
#include "field_screen_effect.h"
#include "constants/maps.h"
#include "constants/vars.h"
#include "constants/flags.h"

/* Dedicated Atlas world identity with no Emerald story events. */
static const u8 sNoScripts[] = {0};
static const u8 sPlayerName[] = _("ATLAS");
static const struct ObjectEventTemplate sNoObjects[OBJECT_EVENT_TEMPLATES_COUNT] = {{0}};
static const struct MapEvents sNoEvents = {0, 0, 0, 0, sNoObjects, NULL, NULL, NULL};
static const struct MapConnection sConnections[] = {
    {CONNECTION_NORTH, 0, MAP_GROUP(MAP_ATLAS_WORLD), MAP_NUM(MAP_ATLAS_WORLD)},
    {CONNECTION_SOUTH, 0, MAP_GROUP(MAP_ATLAS_WORLD), MAP_NUM(MAP_ATLAS_WORLD)},
    {CONNECTION_WEST, 0, MAP_GROUP(MAP_ATLAS_WORLD), MAP_NUM(MAP_ATLAS_WORLD)},
    {CONNECTION_EAST, 0, MAP_GROUP(MAP_ATLAS_WORLD), MAP_NUM(MAP_ATLAS_WORLD)}
};
static const struct MapConnections sConnectionHeader = {4, sConnections};
static struct MapHeader sHeader;
static u8 sAtlasWeatherCadence;
static s32 ReadCoord(u16 low)
{
    return (s32)((u32)VarGet(low) | ((u32)VarGet(low + 1) << 16));
}
static void WriteCoord(u16 low, s32 value)
{
    VarSet(low, (u16)value);
    VarSet(low + 1, (u16)((u32)value >> 16));
}
bool8 AtlasWorld_IsActive(void)
{
    return gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_ATLAS_WORLD)
        && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ATLAS_WORLD);
}
const struct MapHeader *AtlasWorld_Header(const struct MapHeader *base)
{
    sHeader = *base;
    sHeader.events = &sNoEvents;
    sHeader.mapScripts = sNoScripts;
    sHeader.connections = &sConnectionHeader;
    sHeader.showMapName = FALSE;
    sHeader.allowRunning = TRUE;
    if (gAtlasPlanReady)
    {
        s32 x,y;
        AtlasWorld_GetPosition(&x,&y);
        sHeader.weather=AtlasWeather_Native(AtlasWeather_Regional(&gSaveBlock3Ptr->atlas,x,y));
    }
    return &sHeader;
}
void AtlasWorld_Fill(void)
{
    u16 *map = gBackupMapLayout.map;
    s32 i;
    if (VarGet(VAR_ATLAS_MAGIC) != ATLAS_SAVE_MAGIC)
    {
        WriteCoord(VAR_ATLAS_CHUNK_X_LOW, 0);
        WriteCoord(VAR_ATLAS_CHUNK_Y_LOW, 0);
        VarSet(VAR_ATLAS_MAGIC, ATLAS_SAVE_MAGIC);
    }
    if (!AtlasStream_Load(map, ReadCoord(VAR_ATLAS_CHUNK_X_LOW), ReadCoord(VAR_ATLAS_CHUNK_Y_LOW)))
    {
        /* A rejected saved chunk must not expose stale terrain or retain an
         * earlier stream/map binding. Keep the save untouched for recovery. */
        AtlasStream_Reset();
        if (map != NULL)
            for (i = 0; i < 35 * 34; i++)
                map[i] = 0x3401; /* Same blocked pending tile as Stream_Load. */
    }
}
bool8 AtlasWorld_SaveValid(void)
{
    return AtlasWorld_IsActive() && VarGet(VAR_ATLAS_MAGIC) == ATLAS_SAVE_MAGIC
        && (VarGet(VAR_ATLAS_GENERATION) == ATLAS_GENERATION_VERSION
         || VarGet(VAR_ATLAS_GENERATION) == ATLAS_GENERATION_LEGACY)
        && AtlasState_Valid(&gSaveBlock3Ptr->atlas, AtlasWorld_ReadSeed());
}
void AtlasWorld_Tick(void)
{
    if (AtlasWorld_IsActive())
    {
        s32 x, y;
        AtlasStream_Tick(ReadCoord(VAR_ATLAS_CHUNK_X_LOW), ReadCoord(VAR_ATLAS_CHUNK_Y_LOW), gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y);
        AtlasWorld_GetPosition(&x, &y);
        if (!ArePlayerFieldControlsLocked())
        {
            AtlasState_Tick(&gSaveBlock3Ptr->atlas);
            // One cheap macro query per second, without touching hydrology.
            // Let the engine finish/release its previous effect normally.
            if (++sAtlasWeatherCadence>=60 && gAtlasPlanReady)
            {
                u8 weather=AtlasWeather_Native(AtlasWeather_Regional(&gSaveBlock3Ptr->atlas,x,y));
                sAtlasWeatherCadence=0;
                if (GetSavedWeather()!=weather) SetWeather(weather);
            }
        }
        AtlasState_Visit(&gSaveBlock3Ptr->atlas, x, y);
    }
}
void AtlasWorld_Move(u8 direction)
{
    s32 x = ReadCoord(VAR_ATLAS_CHUNK_X_LOW), y = ReadCoord(VAR_ATLAS_CHUNK_Y_LOW);
    /* Malformed saved chunks must not overflow signed increments before
     * existing longitude wrap and terrain landing recovery. */
    if (direction == CONNECTION_NORTH && y > (-2147483647 - 1)) y--;
    if (direction == CONNECTION_SOUTH && y < 2147483647) y++;
    if (direction == CONNECTION_WEST && x > (-2147483647 - 1)) x--;
    if (direction == CONNECTION_EAST && x < 2147483647) x++;
    if (x >= ATLAS_WORLD_SIZE / 20) x = 0;
    if (x < 0) x = ATLAS_WORLD_SIZE / 20 - 1;
    WriteCoord(VAR_ATLAS_CHUNK_X_LOW, x);
    WriteCoord(VAR_ATLAS_CHUNK_Y_LOW, y);
}
void AtlasWorld_NewGame(void)
{
    u32 seed = ((u32)Random() << 16) ^ Random() ^ gMain.vblankCounter1;
    AtlasStream_Reset();
    WriteCoord(VAR_ATLAS_SEED_LOW, seed);
    AtlasState_Init(&gSaveBlock3Ptr->atlas, seed);
    VarSet(VAR_ATLAS_GENERATION, ATLAS_GENERATION_VERSION);
    AtlasWorld_SetGeneration(ATLAS_GENERATION_VERSION);
    VarSet(VAR_ATLAS_MAGIC, ATLAS_SAVE_MAGIC);
    StringCopy(gSaveBlock2Ptr->playerName, sPlayerName);
    gSaveBlock2Ptr->playerGender = 0;
    FlagSet(FLAG_SYS_B_DASH);
}
u32 AtlasWorld_ReadSeed(void)
{
    return (u32)ReadCoord(VAR_ATLAS_SEED_LOW);
}
bool8 AtlasWorld_RestoreGeneration(void)
{
    return AtlasWorld_SetGeneration(VarGet(VAR_ATLAS_GENERATION));
}
void AtlasWorld_FinishNewGame(void)
{
    s32 x = 8, y = 8;
    AtlasTerrain_FindSpawn(&x, &y);
    WriteCoord(VAR_ATLAS_CHUNK_X_LOW, AtlasTerrain_FloorDiv(x, 20));
    WriteCoord(VAR_ATLAS_CHUNK_Y_LOW, AtlasTerrain_FloorDiv(y, 20));
    VarSet(VAR_ATLAS_MAGIC, ATLAS_SAVE_MAGIC);
    SetWarpDestination(MAP_GROUP(MAP_ATLAS_WORLD), MAP_NUM(MAP_ATLAS_WORLD), WARP_ID_NONE,
        x - AtlasTerrain_FloorDiv(x, 20) * 20, y - AtlasTerrain_FloorDiv(y, 20) * 20);
    WarpIntoMap();
}
void AtlasWorld_GetPosition(s32 *x, s32 *y)
{
    s64 worldX = (s64)ReadCoord(VAR_ATLAS_CHUNK_X_LOW) * ATLAS_CHUNK_SIZE + gSaveBlock1Ptr->pos.x;
    s64 worldY = (s64)ReadCoord(VAR_ATLAS_CHUNK_Y_LOW) * ATLAS_CHUNK_SIZE + gSaveBlock1Ptr->pos.y;

    /* Corrupt saved chunks must not overflow before landing repair can run.
     * Saturation only makes composition defined; it does not validate a save. */
    if (worldX > 2147483647LL)
        worldX = 2147483647LL;
    else if (worldX < (-2147483647LL - 1))
        worldX = -2147483647LL - 1;
    if (worldY > 2147483647LL)
        worldY = 2147483647LL;
    else if (worldY < (-2147483647LL - 1))
        worldY = -2147483647LL - 1;

    *x = (s32)worldX;
    *y = (s32)worldY;
}
void AtlasWorld_Teleport(s32 x, s32 y)
{
    s32 cx,cy;
    /* Do not persist a latitude that the streamer will reject or perform
     * signed chunk/local arithmetic on malformed extreme coordinates. */
    if (y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
        return;
    x %= ATLAS_WORLD_WIDTH; if(x<0)x+=ATLAS_WORLD_WIDTH;
    cx = x/20; cy = AtlasTerrain_FloorDiv(y,20);
    WriteCoord(VAR_ATLAS_CHUNK_X_LOW,cx);WriteCoord(VAR_ATLAS_CHUNK_Y_LOW,cy);
    AtlasStream_Reset();
    SetWarpDestination(MAP_GROUP(MAP_ATLAS_WORLD),MAP_NUM(MAP_ATLAS_WORLD),WARP_ID_NONE,x-cx*20,y-cy*20);
    WarpIntoMap();
    gFieldCallback = FieldCB_WarpExitFadeFromBlack;
    SetMainCallback2(CB2_LoadMap);
}
void AtlasWorld_RepairLanding(void)
{
    s32 x,y,cx,cy;
    AtlasWorld_GetPosition(&x,&y);
    if(!AtlasTerrain_RepairLanding(&x,&y))return;
    /* A passable saved position may return unchanged with a negative or
     * out-of-world longitude. Keep saved chunk and local X canonical. */
    x %= ATLAS_WORLD_WIDTH;
    if (x < 0)
        x += ATLAS_WORLD_WIDTH;
    cx = x / ATLAS_CHUNK_SIZE;
    cy = AtlasTerrain_FloorDiv(y, ATLAS_CHUNK_SIZE);
    WriteCoord(VAR_ATLAS_CHUNK_X_LOW,cx);WriteCoord(VAR_ATLAS_CHUNK_Y_LOW,cy);
    gSaveBlock1Ptr->pos.x = x - cx * ATLAS_CHUNK_SIZE;
    /* The floor-division chunk product may underflow s32 for INT32_MIN. */
    gSaveBlock1Ptr->pos.y = (s32)((s64)y - (s64)cy * ATLAS_CHUNK_SIZE);
}
