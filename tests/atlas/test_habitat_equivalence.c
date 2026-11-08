/* Frozen pre-cache climate formula: independent of the interval calculation. */
#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
static u32 queries;
static u8 Clamp(s32 n) { return n < 0 ? 0 : n > 255 ? 255 : n; }
static struct AtlasHabitat Reference(s32 y,u8 elevation,u8 rain,u8 vapor,u8 water)
{
    struct AtlasHabitat out;
    s32 lat=y<0?-y:y;
    s32 temperature=238-lat*200/ATLAS_WORLD_HALF_HEIGHT;
    s32 humidity,moisture,seasonal;
    if(elevation>145)temperature-=(elevation-145)*2;
    temperature=Clamp(temperature);
    humidity=(s32)vapor*255/(90+temperature*2/3);
    moisture=(rain*3+vapor)/4-temperature/12;
    if(elevation>170)moisture-=(elevation-170)/3;
    seasonal=3+lat*24/ATLAS_WORLD_HALF_HEIGHT+(230-vapor)/12;
    if(water!=ATLAS_DRY){moisture=255;humidity+=30;seasonal=seasonal*2/3;}
    out.temperature=temperature;out.humidity=Clamp(humidity);
    out.moisture=Clamp(moisture);out.seasonalRange=Clamp(seasonal);
    return out;
}
static void Check(s32 y,u8 e,u8 r,u8 v,u8 water)
{
    struct AtlasHabitat expected=Reference(y,e,r,v,water),actual;
    AtlasHabitat_Sample(y,e,r,v,water,&actual);
    assert(!memcmp(&expected,&actual,sizeof(actual)));queries++;
}
int main(void)
{
    s32 y,k,q,delta,e;u32 random=0x6f73a2b1u,i;
    /* Every latitude, alternating nearby/mirrored/repeated queries and keys.
     * This also crosses all quotient boundaries in both hemispheres. */
    for(y=0;y<=ATLAS_WORLD_HALF_HEIGHT;y++)
    {
        u8 elevation=120+(y/17)%100,rain=(y/29)%256,vapor=(y/31)%256;
        u8 water=(y/37)%4;
        Check(y,elevation,rain,vapor,water);
        Check(-y,elevation,rain,vapor,water);
    }
    /* Jump across each heat/season boundary, then reverse over adjacent
     * tiles with all elevations and both wet/dry climate paths. */
    for(k=24;k<=200;k+=176)for(q=0;q<=k;q++)
    {
        s32 boundary=(q*ATLAS_WORLD_HALF_HEIGHT+k-1)/k;
        for(e=0;e<256;e++)for(delta=-2;delta<=2;delta++)
        {
            s32 lat=boundary+delta;
            if(lat<0 || lat>ATLAS_WORLD_HALF_HEIGHT)continue;
            Check(lat,e,e*7,e*13,ATLAS_DRY);
            Check(-lat,e,e*7,e*13,ATLAS_LAKE);
            Check(lat,e,e*7,e*13,ATLAS_DRY);
        }
    }
    /* Unrelated coordinates and parameters prevent order-dependent reuse. */
    for(i=0;i<1000000;i++)
    {
        random=random*1664525u+1013904223u;
        y=(s32)(random%(2*ATLAS_WORLD_HALF_HEIGHT))-ATLAS_WORLD_HALF_HEIGHT;
        Check(y,random>>24,random>>16,random>>8,random&3);
    }
    printf("PASS habitat: %u original-formula queries; all latitude tiles, both hemispheres, exact boundaries, wet/dry and random-order parameters\n",queries);
}
