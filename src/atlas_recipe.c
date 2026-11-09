#include "global.h"
#include "atlas_plan.h"
#include "atlas_world.h"
#include "atlas_recipe.h"
#include "malloc.h"

/* Explicit little-endian format; no compiler struct padding enters flash.
 * 128-byte header, 4608 height bytes, 2304 class + 576 policy bytes reserved,
 * 320 padding bytes. Reserved fields must be zero in codec version 1.
 * CRC covers both sector payloads with the CRC field treated as zero. */
EWRAM_DATA u8 gAtlasRecipeStatus = 0;
EWRAM_DATA u16 gAtlasRecipeWrites = 0;
EWRAM_DATA static u32 sExpectedPlan = 0, sExpectedHabitat = 0;
static const u32 sCrc[] = {
    0x00000000,0x1DB71064,0x3B6E20C8,0x26D930AC,
    0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
    0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,
    0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C
};
static u32 Get(const u8 *p)
{
    return p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24);
}
static void Put(u8 *p, u32 n)
{
    p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;
}
static u32 Crc(const u8 *p)
{
    u16 i;
    u32 crc=0xFFFFFFFF;
    for(i=0;i<ATLAS_RECIPE_BYTES;i++)
    {
        crc ^= i>=28 && i<32 ? 0 : p[i];
        crc=(crc>>4)^sCrc[crc&15];
        crc=(crc>>4)^sCrc[crc&15];
    }
    return ~crc;
}
void AtlasRecipe_Reset(void)
{
    gAtlasRecipeStatus=0;gAtlasRecipeWrites=0;sExpectedPlan=sExpectedHabitat=0;
}
bool8 AtlasRecipe_Build(u8 *data,u32 counter)
{
    u16 i;
    if(!data || !gAtlasPlanReady || gAtlasPlanStats.land==ATLAS_PLAN_COUNT)return FALSE;
    memset(data,0,ATLAS_RECIPE_BYTES);
    Put(data,0x41544C52);Put(data+4,1);Put(data+8,gAtlasWorldSeed);
    Put(data+12,gAtlasPlanStats.checksum);Put(data+16,gAtlasPlanStats.habitatChecksum);
    Put(data+20,counter);Put(data+24,AtlasWorld_Generation());
    Put(data+32,ATLAS_PLAN_WIDTH|((u32)ATLAS_PLAN_HEIGHT<<16));
    for(i=0;i<ATLAS_PLAN_COUNT;i++)data[ATLAS_RECIPE_HEADER+i]=gAtlasPlan[i].height;
    Put(data+28,Crc(data));
    return TRUE;
}
bool8 AtlasRecipe_Valid(const u8 *data)
{
    u16 i;
    if(!data || Get(data)!=0x41544C52 || Get(data+4)!=1
        || Get(data+24)!=AtlasWorld_Generation()
        || Get(data+32)!=(ATLAS_PLAN_WIDTH|((u32)ATLAS_PLAN_HEIGHT<<16))
        || Get(data+28)!=Crc(data))return FALSE;
    for(i=36;i<ATLAS_RECIPE_HEADER;i++)if(data[i])return FALSE;
    for(i=ATLAS_RECIPE_HEADER+ATLAS_PLAN_COUNT;i<ATLAS_RECIPE_BYTES;i++)if(data[i])return FALSE;
    return TRUE;
}
bool8 AtlasRecipe_Load(u32 seed)
{
    u8 slot,best=2;
    u32 newest=0,counter;
    u8 *data=AllocZeroed(ATLAS_RECIPE_BYTES);
    if(!data)return FALSE;
    for(slot=0;slot<2;slot++)
        if(AtlasRecipeIO_Read(slot,data) && AtlasRecipe_Valid(data) && Get(data+8)==seed)
        {
            counter=Get(data+20);
            if(best==2 || (s32)(counter-newest)>0){best=slot;newest=counter;}
        }
    if(best<2 && AtlasRecipeIO_Read(best,data) && AtlasRecipe_Valid(data)
        && Get(data+8)==seed && AtlasPlan_ImportHeights(data+ATLAS_RECIPE_HEADER))
    {
        sExpectedPlan=Get(data+12);sExpectedHabitat=Get(data+16);
        gAtlasRecipeStatus=1;Free(data);return TRUE;
    }
    Free(data);return FALSE;
}
bool8 AtlasRecipe_VerifyRestore(void)
{
    if(gAtlasRecipeStatus!=1)return TRUE;
    if(gAtlasPlanStats.checksum==sExpectedPlan && gAtlasPlanStats.habitatChecksum==sExpectedHabitat)
    {gAtlasRecipeStatus=2;return TRUE;}
    gAtlasRecipeStatus=3;return FALSE;
}
void AtlasRecipe_Save(u32 counter)
{
    u8 slot,target=0;
    u16 i;
    bool8 valid[2];
    u32 counters[2];
    u8 *data;
    if(!gAtlasPlanReady || gAtlasPlanStats.land==ATLAS_PLAN_COUNT)return;
    data=AllocZeroed(ATLAS_RECIPE_BYTES);
    if(!data){gAtlasRecipeStatus=6;return;}
    for(slot=0;slot<2;slot++)
    {
        valid[slot]=AtlasRecipeIO_Read(slot,data) && AtlasRecipe_Valid(data);
        counters[slot]=valid[slot]?Get(data+20):0;
        if(valid[slot] && Get(data+8)==gAtlasWorldSeed)
        {
            for(i=0;i<ATLAS_PLAN_COUNT;i++)
                if(data[ATLAS_RECIPE_HEADER+i]!=gAtlasPlan[i].height)break;
            if(i==ATLAS_PLAN_COUNT && Get(data+12)==gAtlasPlanStats.checksum
                && Get(data+16)==gAtlasPlanStats.habitatChecksum)
            {gAtlasRecipeStatus=5;Free(data);return;}
            valid[slot]=FALSE;
        }
    }
    if(valid[0])target=!valid[1] || (s32)(counters[0]-counters[1])>=0?1:0;
    if(AtlasRecipe_Build(data,counter) && AtlasRecipeIO_Write(target,data))
    {gAtlasRecipeStatus=4;gAtlasRecipeWrites++;}
    else gAtlasRecipeStatus=6;
    Free(data);
}
