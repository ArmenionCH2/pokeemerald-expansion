#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include "atlas_pokemon.h"
#include "atlas_actor_runtime.h"
#include "atlas_actor_spawn.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "script.h"
#include "sprite.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"

/* One candidate per simulation tick; the recovered admission ledger owns at
 * most three transient actors. This is habitat-based wandering wildlife, not
 * a persistent population, trust model or off-screen ecology simulation. */
static u16 sScan;
void AtlasWildlife_Tick(void)
{
    struct AtlasActorAdmissionRoster *roster;
    struct AtlasPokemonSpawn spawn;
    struct AtlasActorSpawnKey key;
    struct AtlasActorHandle handle;
    struct AtlasActorNativeBinding binding;
    struct ObjectEventTemplate object = {0};
    struct AtlasCell cell;
    s32 x,y,dx,dy;
    u16 freeObjects=0,freeSprites=0;
    u32 i, slot;
    if (!AtlasWorld_IsActive() || !gAtlasPlanReady || ArePlayerFieldControlsLocked()
     || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT
     || !gObjectEvents[gPlayerAvatar.objectEventId].active) return;
    roster=AtlasActorRuntime_GetRoster();
    for (slot=0;slot<ATLAS_ACTOR_ROSTER_CAPACITY;slot++)
        if (roster->slots[slot].state==ATLAS_ACTOR_EMPTY) break;
    if (slot==ATLAS_ACTOR_ROSTER_CAPACITY) return;
    dx=(sScan%15)-7; dy=(sScan/15)-5;
    sScan=(sScan+1)%165;
    if (dx>=-1 && dx<=1 && dy>=-1 && dy<=1) return;
    AtlasWorld_GetPosition(&x,&y); x+=dx; y+=dy;
    if (!AtlasPokemon_IsCandidate(x,y,gAtlasWorldSeed)) return;
    AtlasTerrain_Sample(x,y,&cell);
    if (!AtlasPokemon_Select(x,y,gAtlasWorldSeed,&cell,&spawn)) return;
    object.x=gSaveBlock1Ptr->pos.x+dx;
    object.y=gSaveBlock1Ptr->pos.y+dy;
    if (MapGridGetCollisionAt(object.x+MAP_OFFSET,object.y+MAP_OFFSET)) return;
    for (i=0;i<OBJECT_EVENTS_COUNT;i++)
    {
        if (!gObjectEvents[i].active) freeObjects++;
        else if (gObjectEvents[i].currentCoords.x==object.x+MAP_OFFSET
              && gObjectEvents[i].currentCoords.y==object.y+MAP_OFFSET) return;
    }
    for (i=0;i<MAX_SPRITES;i++) if (!gSprites[i].inUse) freeSprites++;
    if (!AtlasActor_MakeSpawnKey(&key,gAtlasWorldSeed,x,y,spawn.species,ATLAS_GENERATION_VERSION)) return;
    object.localId=100+slot;
    object.graphicsId=spawn.graphicsId;
    object.kind=OBJ_KIND_NORMAL;
    object.elevation=MapGridGetElevationAt(object.x+MAP_OFFSET,object.y+MAP_OFFSET);
    object.movementType=MOVEMENT_TYPE_WANDER_AROUND_SLOWER;
    object.movementRangeX=object.movementRangeY=1;
    if (!AtlasActorAdmission_Reserve(roster,&key,object.localId,freeObjects,freeSprites,2,4,&handle)) return;
    if (AtlasActorSpawn_TryCommitPending(roster,handle,&object,&binding)==ATLAS_ACTOR_SPAWN_REJECTED)
        AtlasActorAdmission_Abort(roster,handle);
    // Other failures retain the transaction's conservative ownership policy.
}
