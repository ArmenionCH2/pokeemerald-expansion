#include "global.h"
#include "atlas_plan.h"
#include "atlas_climate.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    u8 vapor,rain,coast,interior,windRain,leeRain,windVapor,leeVapor;
    for (unsigned row=0;row<ATLAS_PLAN_HEIGHT;row++)
    {
        u8 otherVapor,otherRain;
        unsigned mirror=ATLAS_PLAN_HEIGHT-1-row;
        assert(AtlasClimate_Upwind(row)==AtlasClimate_Upwind(mirror));
        AtlasClimate_Transport(row,140,140,200,FALSE,&vapor,&rain);
        AtlasClimate_Transport(mirror,140,140,200,FALSE,&otherVapor,&otherRain);
        assert(vapor==otherVapor && rain==otherRain);
        for (unsigned height=0;height<256;height++)
            AtlasClimate_Transport(row,height,255-height,height,FALSE,&vapor,&rain);
    }
    assert(AtlasClimate_Upwind(23)==0); // equatorial trades
    assert(AtlasClimate_Upwind(12)==1); // mid-latitude westerlies
    assert(AtlasClimate_Upwind(0)==0); // polar easterlies
    AtlasClimate_Transport(12,130,130,230,FALSE,&vapor,&coast);
    for (unsigned i=0;i<30;i++) AtlasClimate_Transport(12,130,130,vapor,FALSE,&vapor,&interior);
    assert(interior<coast && vapor<120);
    AtlasClimate_Transport(12,210,130,200,FALSE,&windVapor,&windRain);
    AtlasClimate_Transport(12,130,210,windVapor,FALSE,&leeVapor,&leeRain);
    assert(windRain>leeRain && leeVapor<196);
    u8 polarVapor,polarRain,equatorVapor,equatorRain;
    AtlasClimate_Transport(0,100,100,0,FALSE,&polarVapor,&polarRain);
    AtlasClimate_Transport(23,100,100,0,FALSE,&equatorVapor,&equatorRain);
    assert(polarVapor<equatorVapor && polarRain<equatorRain);
    AtlasClimate_Transport(23,130,130,230,TRUE,&vapor,&rain);
    assert(vapor==12 && rain<60); // convection cannot manufacture an inland wet belt
    puts("PASS climate hemispheres, wind belts, cold ocean supply, continental drying, rain shadow and dry-row bounds");
    return 0;
}
