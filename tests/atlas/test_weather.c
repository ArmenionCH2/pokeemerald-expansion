#include "global.h"
#include "atlas_world.h"
#include "atlas_state.h"
#include "atlas_weather.h"
#include "atlas_plan.h"
#include "constants/weather.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static unsigned Difference(u8 a,u8 b) {return a>b?a-b:b-a;}
bool8 gAtlasPlanReady;
static struct AtlasHabitat sClimate;
static struct AtlasPlanSample sPlan;
static unsigned sQueries;
void AtlasPlan_SampleDetail(s32 x,s32 y,struct AtlasPlanSample *out)
{
    assert(x>=0 && x<ATLAS_WORLD_WIDTH);
    assert(y>=-ATLAS_WORLD_HALF_HEIGHT && y<ATLAS_WORLD_HALF_HEIGHT);
    sQueries++;*out=sPlan;
}
void AtlasHabitat_Sample(s32 y,u8 elevation,u8 rain,u8 vapor,u8 waterform,struct AtlasHabitat *out)
{
    assert(elevation==sPlan.height && rain==sPlan.rain && vapor==sPlan.humidity);
    assert(waterform==(sPlan.height<ATLAS_PLAN_SEA?ATLAS_SEA:sPlan.lake==1?ATLAS_LAKE:ATLAS_DRY));
    *out=sClimate;
}
int main(void)
{
    struct AtlasSaveState state={0},before;
    unsigned i,rain=0,snow=0;
    u32 rng=123;
    state.seed=0xA756;
    for (i=0;i<20000;i++)
    {
        s32 x,y;
        rng=rng*1664525u+1013904223u;x=rng%ATLAS_WORLD_WIDTH;
        rng=rng*1664525u+1013904223u;y=(rng%(2*ATLAS_WORLD_HALF_HEIGHT))-ATLAS_WORLD_HALF_HEIGHT;
        state.seconds=rng;
        before=state;
        u8 score=AtlasWeather_Potential(&state,x,y);
        assert(score==AtlasWeather_Potential(&state,x-ATLAS_WORLD_WIDTH,y));
        assert(Difference(score,AtlasWeather_Potential(&state,x+1,y))<=2);
        if (y+1<ATLAS_WORLD_HALF_HEIGHT) assert(Difference(score,AtlasWeather_Potential(&state,x,y+1))<=2);
        assert(!memcmp(&state,&before,sizeof(state)));
        assert(AtlasState_Weather(&state,x,y,200,40)==ATLAS_WEATHER_CLEAR);
        if (AtlasState_Weather(&state,x,y,140,220)==ATLAS_WEATHER_RAIN) rain++;
        if (AtlasState_Weather(&state,x,y,40,220)==ATLAS_WEATHER_SNOW) snow++;
    }
    assert(rain>1000 && snow==rain);
    for (i=0;i<100;i++)
    {
        state.seconds=21600*(i+1)-1;
        u8 previous=AtlasWeather_Potential(&state,0,12345);
        state.seconds++;
        assert(Difference(previous,AtlasWeather_Potential(&state,0,12345))<=2);
        assert(Difference(AtlasWeather_Potential(&state,-1,12345),AtlasWeather_Potential(&state,0,12345))<=2);
    }
    state.seconds=UINT_MAX;
    assert(AtlasWeather_Potential(&state,INT_MIN,INT_MIN)==255);
    assert(AtlasState_Weather(NULL,0,0,100,255)==ATLAS_WEATHER_CLEAR);
    assert(AtlasWeather_Regional(&state,0,0)==ATLAS_WEATHER_CLEAR && !sQueries);
    gAtlasPlanReady=TRUE;
    assert(AtlasWeather_Regional(&state,0,INT_MIN)==ATLAS_WEATHER_CLEAR && !sQueries);
    assert(AtlasWeather_Regional(NULL,0,0)==ATLAS_WEATHER_CLEAR && !sQueries);
    for (i=0;i<1000;i++)
    {
        state.seconds=i*1799;sPlan.height=i%256;sPlan.lake=i%3;
        sClimate.temperature=i%256;sClimate.humidity=(i*7)%256;
        before=state;
        u8 expected=AtlasState_Weather(&state,0,0,sClimate.temperature,sClimate.humidity);
        assert(AtlasWeather_Regional(&state,-ATLAS_WORLD_WIDTH,0)==expected);
        assert(!memcmp(&before,&state,sizeof(state)));
    }
    assert(AtlasWeather_Native(ATLAS_WEATHER_CLEAR)==WEATHER_SUNNY);
    assert(AtlasWeather_Native(ATLAS_WEATHER_RAIN)==WEATHER_RAIN);
    assert(AtlasWeather_Native(ATLAS_WEATHER_SNOW)==WEATHER_SNOW);
    assert(AtlasWeather_Native(ATLAS_WEATHER_FOG)==WEATHER_FOG_HORIZONTAL);
    assert(AtlasWeather_Native(255)==WEATHER_SUNNY);
    puts("PASS weather spatial/time continuity, dateline wrapping, cold rain/snow, dry climate and read-only queries");
    return 0;
}
