#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_place.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

struct AtlasPlanNode gAtlasPlan[ATLAS_PLAN_COUNT];
bool8 gAtlasPlanReady;
u32 gAtlasWorldSeed = 123;
u16 AtlasPlan_LakeId(s32 x,s32 y){return 0;} /* No planner in this hierarchy fixture. */

int main(void)
{
    struct AtlasCell cell = {0};
    struct AtlasPlace a, b, c;
    struct AtlasPlaceTracker tracker = {0};
    char name[ATLAS_PLACE_NAME_BYTES], again[ATLAS_PLACE_NAME_BYTES];
    unsigned i, j;
    cell.biome = ATLAS_FOREST; cell.landform = ATLAS_PLAIN;
    assert(!AtlasPlace_Identify(0, 0, &cell, &a));
    gAtlasPlanReady = TRUE;
    for (i = 0; i < ATLAS_PLAN_COUNT; i++)
    {
        gAtlasPlan[i].continent = 1;
        gAtlasPlan[i].basin = 37 | ATLAS_LAKE_FLAG;
    }
    assert(AtlasPlace_Identify(0, 0, &cell, &a));
    assert(a.landmass == 1 && a.basin == 37 && a.kind == ATLAS_PLACE_FOREST);
    for (i = 0; i < 400; i++)
    {
        assert(AtlasPlace_Identify(i, 0, &cell, &b));
        assert(!memcmp(&a, &b, sizeof(a))); // crosses nineteen chunk edges
        assert(AtlasPlace_Identify((s32)i - ATLAS_WORLD_WIDTH, 0, &cell, &b));
        assert(!memcmp(&a, &b, sizeof(a)));
    }
    assert(!AtlasPlace_Identify(0, INT_MIN, &cell, &b));
    assert(!AtlasPlace_Identify(0, ATLAS_WORLD_HALF_HEIGHT, &cell, &b));
    assert(AtlasPlace_Identify(INT_MIN, -ATLAS_WORLD_HALF_HEIGHT, &cell, &b));
    assert(AtlasPlace_Name(&a, 0, name));
    assert(strstr(name, "Continent"));
    assert(AtlasPlace_Name(&a, 1, name));assert(strstr(name, "Basin"));
    b=a;AtlasPlace_ApplyShore(&b,1<<ATLAS_LAKE);
    assert(b.kind==ATLAS_PLACE_LAKE && b.landmass==a.landmass && b.basin==a.basin);
    assert(AtlasPlace_Name(&b,2,name) && !strcmp(name,"Lake shore"));
    b=a;AtlasPlace_ApplyShore(&b,1<<ATLAS_RIVER);assert(b.kind==ATLAS_PLACE_RIVER);
    b=a;AtlasPlace_ApplyShore(&b,1<<ATLAS_SEA);assert(b.kind==ATLAS_PLACE_COAST);
    b=a;AtlasPlace_ApplyShore(&b,0);assert(!memcmp(&a,&b,sizeof(a)));
    b.kind=ATLAS_PLACE_OCEAN;AtlasPlace_ApplyShore(&b,1<<ATLAS_RIVER);assert(b.kind==ATLAS_PLACE_OCEAN);
    for (i = 1; i <= ATLAS_PLAN_COUNT; i++)
    {
        b = a; b.basin = i;
        assert(AtlasPlace_Name(&b, 1, name));
        assert(strlen(name) < sizeof(name));
        assert(AtlasPlace_Name(&b, 1, again));assert(!strcmp(name, again));
    }
    // Distinct adjacent catchment IDs retain distinct names.
    for (i = 1; i < 100; i++)
    {
        b = a; b.basin = i; assert(AtlasPlace_Name(&b, 1, name));
        b.basin++; assert(AtlasPlace_Name(&b, 1, again));assert(strcmp(name, again));
    }
    for (i = 0; i < 3; i++) assert(!AtlasPlace_Update(&tracker, &a));
    assert(AtlasPlace_Update(&tracker, &a) == 7);
    b = a; b.kind = ATLAS_PLACE_COAST;
    // Rapid boundary oscillation does not generate repeated announcements.
    for (i = 0; i < 100; i++) assert(!AtlasPlace_Update(&tracker, i & 1 ? &a : &b));
    for (i = 0; i < 3; i++) assert(!AtlasPlace_Update(&tracker, &b));
    assert(AtlasPlace_Update(&tracker, &b) == 4);
    c = b; c.basin++;
    for (i = 0; i < 19; i++) assert(!AtlasPlace_Update(&tracker, &c));
    assert(AtlasPlace_Update(&tracker, &c) == 6);
    c.landmass++;
    for (i = 0; i < 19; i++) assert(!AtlasPlace_Update(&tracker, &c));
    assert(AtlasPlace_Update(&tracker, &c) == 7);
    cell.waterform = ATLAS_SEA;
    assert(AtlasPlace_Identify(0, 0, &cell, &b));
    assert(AtlasPlace_Name(&b, 0, name) && !strcmp(name, "Open Ocean"));
    assert(!AtlasPlace_Name(&b, 1, name));
    // No query or notification changes the underlying geographic labels.
    for (j = 0; j < ATLAS_PLAN_COUNT; j++) assert(gAtlasPlan[j].basin == (37 | ATLAS_LAKE_FLAG));
    for (j = 0; j < ATLAS_PLAN_COUNT; j++) gAtlasPlan[j].continent=ATLAS_OCEAN_COMPONENT|1;
    assert(AtlasPlace_Identify(0,0,&cell,&b));
    assert(b.landmass==(ATLAS_OCEAN_COMPONENT|1));
    assert(AtlasPlace_Name(&b,0,name) && strstr(name,"Ocean"));
    for (j=1;j<ATLAS_PLAN_COUNT;j++) gAtlasPlan[j].continent=ATLAS_OCEAN_COMPONENT|2;
    assert(AtlasPlace_Name(&b,0,name) && strstr(name," Sea"));
    // A different bounded landmark changes only the leaf announcement.
    a=(struct AtlasPlace){.landmass=1,.basin=37,.kind=ATLAS_PLACE_MESA};
    AtlasPlace_SetLandmark(&a,0,0);assert(a.landmark);
    assert(AtlasPlace_Name(&a,2,name) && strstr(name," Mesa"));
    b=a;AtlasPlace_SetLandmark(&b,ATLAS_WORLD_WIDTH,0);assert(a.landmark==b.landmark);
    AtlasPlace_SetLandmark(&b,-1000,0);assert(a.landmark==b.landmark);
    memset(&tracker,0,sizeof(tracker));for(i=0;i<4;i++)AtlasPlace_Update(&tracker,&a);
    b=a;AtlasPlace_SetLandmark(&b,ATLAS_WORLD_WIDTH/96,0);assert(b.landmark!=a.landmark);
    for(i=0;i<19;i++)assert(!AtlasPlace_Update(&tracker,&b));
    assert(AtlasPlace_Update(&tracker,&b)==4);
    assert(AtlasPlace_Name(&b,2,again) && strcmp(name,again));
    // Cone and caldera share a root; every site name fits the existing buffer.
    for(i=1;i<=ATLAS_PLAN_COUNT;i++)
    {
        a.landmark=i;a.kind=ATLAS_PLACE_VOLCANO;assert(AtlasPlace_Name(&a,2,name));
        a.kind=ATLAS_PLACE_CALDERA;assert(AtlasPlace_Name(&a,2,again));
        char *tail=strstr(name," Cone");assert(tail);*tail=0;
        tail=strstr(again," Caldera");assert(tail);*tail=0;assert(!strcmp(name,again));
        a.kind=ATLAS_PLACE_SINKHOLE;assert(AtlasPlace_Name(&a,2,name) && strstr(name," Sinkhole"));assert(strlen(name)<sizeof(name));
    }
    AtlasPlace_ApplyShore(&a,1<<ATLAS_RIVER);assert(!a.landmark && a.kind==ATLAS_PLACE_RIVER);
    puts("PASS physical names, geographic IDs, chunk/dateline continuity, poles, hierarchy and border debounce");
    return 0;
}
