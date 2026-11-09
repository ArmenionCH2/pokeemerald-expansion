#include "global.h"
#include "atlas_world.h"
#include "atlas_state.h"
#include "atlas_weather.h"
#include "atlas_plan.h"
#include "constants/weather.h"

static u32 Mix(u32 n)
{
    n ^= n >> 16; n *= 0x7FEB352Du; n ^= n >> 15;
    n *= 0x846CA68Bu; return n ^ (n >> 16);
}
static u32 Smooth(u32 f) {return (f*f*(768-2*f))>>16;}
static u8 Blend(u32 a,u32 b,u32 fraction) {return (a*(256-fraction)+b*fraction)>>8;}
static u8 Field(u32 seed,s32 x,s32 y,u32 epoch)
{
    // A longitude-periodic 64x32 regional lattice, with capped polar rows.
    u32 sx=(u32)x*64,sy=(u32)(y+ATLAS_WORLD_HALF_HEIGHT)*31;
    u32 col=sx/ATLAS_WORLD_WIDTH,row=sy/(2*ATLAS_WORLD_HALF_HEIGHT);
    u32 fx=(sx%ATLAS_WORLD_WIDTH)/(ATLAS_WORLD_WIDTH/256);
    u32 fy=(sy%(2*ATLAS_WORLD_HALF_HEIGHT))/(2*ATLAS_WORLD_HALF_HEIGHT/256);
    u32 next=(col+1)%64,lower=row<31?row+1:row;
    u32 salt=seed^Mix(epoch^0x57454154u);
    u32 a=Mix(salt^col*0x9E3779B1u^row*0x85EBCA77u)&255;
    u32 b=Mix(salt^next*0x9E3779B1u^row*0x85EBCA77u)&255;
    u32 c=Mix(salt^col*0x9E3779B1u^lower*0x85EBCA77u)&255;
    u32 d=Mix(salt^next*0x9E3779B1u^lower*0x85EBCA77u)&255;
    if (fx>255) fx=255;
    if (fy>255) fy=255;
    fx=Smooth(fx);fy=Smooth(fy);
    return Blend(Blend(a,b,fx),Blend(c,d,fx),fy);
}
u8 AtlasWeather_Potential(const struct AtlasSaveState *state,s32 x,s32 y)
{
    u32 drift,phase,epoch;
    if (!state || y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT) return 255;
    x%=ATLAS_WORLD_WIDTH;if (x<0) x+=ATLAS_WORLD_WIDTH;
    // Advect the regional field cyclically. Prevailing wind and rain shadow
    // remain in macro climate; this is not a numerical atmosphere solver.
    drift=((unsigned long long)state->seconds*20)%ATLAS_WORLD_WIDTH;
    x=(x+ATLAS_WORLD_WIDTH-drift)%ATLAS_WORLD_WIDTH;
    epoch=state->seconds/21600;
    phase=Smooth((state->seconds%21600)*256/21600);
    return Blend(Field(state->seed,x,y,epoch),Field(state->seed,x,y,epoch+1),phase);
}
u8 AtlasState_Weather(const struct AtlasSaveState *state,s32 x,s32 y,u8 temperature,u8 humidity)
{
    u8 potential;
    if (!state || y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT) return ATLAS_WEATHER_CLEAR;
    potential=AtlasWeather_Potential(state,x,y);
    if (humidity>=120 && potential<humidity/2)
        return temperature<80?ATLAS_WEATHER_SNOW:ATLAS_WEATHER_RAIN;
    if (humidity>=180 && temperature<150 && potential>195) return ATLAS_WEATHER_FOG;
    return ATLAS_WEATHER_CLEAR;
}

u8 AtlasWeather_Regional(const struct AtlasSaveState *state,s32 x,s32 y)
{
    struct AtlasPlanSample plan;
    struct AtlasHabitat habitat;
    u8 waterform;
    if (!gAtlasPlanReady || !state || y < -ATLAS_WORLD_HALF_HEIGHT || y >= ATLAS_WORLD_HALF_HEIGHT)
        return ATLAS_WEATHER_CLEAR;
    x%=ATLAS_WORLD_WIDTH;if (x<0) x+=ATLAS_WORLD_WIDTH;
    AtlasPlan_SampleDetail(x,y,&plan);
    waterform=plan.height<ATLAS_PLAN_SEA?ATLAS_SEA:plan.lake==1?ATLAS_LAKE:ATLAS_DRY;
    AtlasHabitat_Sample(y,plan.height,plan.rain,plan.humidity,waterform,&habitat);
    return AtlasState_Weather(state,x,y,habitat.temperature,habitat.humidity);
}

u8 AtlasWeather_Native(u8 weather)
{
    switch (weather)
    {
    case ATLAS_WEATHER_RAIN: return WEATHER_RAIN;
    case ATLAS_WEATHER_SNOW: return WEATHER_SNOW;
    case ATLAS_WEATHER_FOG: return WEATHER_FOG_HORIZONTAL;
    default: return WEATHER_SUNNY;
    }
}
