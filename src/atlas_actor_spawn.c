#include "global.h"
#include "atlas_actor_spawn.h"
#include "atlas_world.h"
#include "event_object_movement.h"
#include "constants/event_objects.h"

static void InvalidateBinding(struct AtlasActorNativeBinding *binding)
{
    if (binding != NULL)
    {
        binding->objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
        binding->spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
        binding->mapNum = 0;
        binding->mapGroup = 0;
    }
}

/* Never retire a reservation when native ownership cannot be proven and
 * cleaned. Blocked cleanup is a hard lifecycle fault, not a retry signal. */
static enum AtlasActorSpawnResult RollBackSpawn(
    struct AtlasActorAdmissionRoster *roster,
    struct AtlasActorHandle handle,
    u8 objectEventId, u8 localId, u8 mapNum, u8 mapGroup,
    u8 spriteId)
{
    if (!AtlasTryRemoveTransientObjectEvent(objectEventId, localId,
                                             mapNum, mapGroup, spriteId))
        return ATLAS_ACTOR_SPAWN_CLEANUP_BLOCKED;
    if (!AtlasActorAdmission_Abort(roster, handle))
        return ATLAS_ACTOR_SPAWN_ROSTER_FAULT;
    return ATLAS_ACTOR_SPAWN_ROLLED_BACK;
}

enum AtlasActorSpawnResult AtlasActorSpawn_TryCommitPending(
    struct AtlasActorAdmissionRoster *roster,
    struct AtlasActorHandle handle,
    struct ObjectEventTemplate *objectTemplate,
    struct AtlasActorNativeBinding *outBinding)
{
    const struct AtlasActorAdmissionSlot *slot;
    struct AtlasActorNativeBinding binding;
    u8 existingObjectId;
    u8 objectEventId;
    u8 spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    u8 mapNum;
    u8 mapGroup;

    InvalidateBinding(outBinding);
    if (roster == NULL || objectTemplate == NULL || gSaveBlock1Ptr == NULL
     || !AtlasWorld_IsActive() || gMapHeader.events == NULL
     || gMapHeader.events->objectEventCount != 0
     || handle.slot >= ATLAS_ACTOR_ROSTER_CAPACITY)
        return ATLAS_ACTOR_SPAWN_REJECTED;

    slot = &roster->slots[handle.slot];
    if (slot->state != ATLAS_ACTOR_PENDING
     || !AtlasActor_HandleMatches(handle,
            (struct AtlasActorHandle){slot->generation, handle.slot},
            ATLAS_ACTOR_ROSTER_CAPACITY)
     || objectTemplate->localId != slot->localId
     || objectTemplate->kind != OBJ_KIND_NORMAL
     || objectTemplate->trainerType != 0
     || objectTemplate->script != NULL || objectTemplate->flagId != 0)
        return ATLAS_ACTOR_SPAWN_REJECTED;

    mapNum = gSaveBlock1Ptr->location.mapNum;
    mapGroup = gSaveBlock1Ptr->location.mapGroup;
    /* TRUE means no matching native object was found. Reject an occupied
     * local/map identity rather than touching someone else's sprite. */
    if (!TryGetObjectEventIdByLocalIdAndMap(slot->localId, mapNum,
                                            mapGroup, &existingObjectId))
        return ATLAS_ACTOR_SPAWN_REJECTED;

    objectEventId = SpawnSpecialObjectEvent(objectTemplate);
    if (objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT)
    {
        /* Emerald deactivates its object slot on CreateSprite failure.
         * No successful object receipt exists to clean up here. */
        if (!AtlasActorAdmission_Abort(roster, handle))
            return ATLAS_ACTOR_SPAWN_ROSTER_FAULT;
        return ATLAS_ACTOR_SPAWN_ROLLED_BACK;
    }

    if (!AtlasTryCaptureTransientSpawnReceipt(objectEventId, slot->localId,
                                               mapNum, mapGroup, &spriteId))
    {
        /* Receipt failure may mean ownership changed. Use only the slot's
         * reported sprite with owner-checked cleanup. If ownership cannot
         * be proven, retain PENDING and return CLEANUP_BLOCKED. */
        spriteId = gObjectEvents[objectEventId].spriteId;
        return RollBackSpawn(roster, handle, objectEventId, slot->localId,
                             mapNum, mapGroup, spriteId);
    }

    binding.objectEventId = objectEventId;
    binding.spriteId = spriteId;
    binding.mapNum = mapNum;
    binding.mapGroup = mapGroup;
    if (!AtlasActorAdmission_CommitBound(roster, handle, &binding))
        return RollBackSpawn(roster, handle, objectEventId, slot->localId,
                             mapNum, mapGroup, spriteId);

    if (outBinding != NULL)
        *outBinding = binding;
    return ATLAS_ACTOR_SPAWN_COMMITTED;
}

/* Stage 2B7: retire only a LIVE actor whose generation and native object/sprite
 * ownership are still verified. A culled or reused slot is NOT evidence that
 * cleanup completed; the caller must handle that lifecycle separately. */
enum AtlasActorRetireResult AtlasActorSpawn_TryRetireBound(
    struct AtlasActorAdmissionRoster *roster, struct AtlasActorHandle handle)
{
    struct AtlasActorNativeBinding binding;
    u8 localId;
    u8 actualSpriteId = ATLAS_ACTOR_SPRITE_LIMIT;

    if (!AtlasActorAdmission_ResolveBound(roster, handle, &binding)
     || !AtlasActorAdmission_Resolve(roster, handle, NULL, &localId))
        return ATLAS_ACTOR_RETIRE_REJECTED;

    /* A stale map transition must never remove a newly reused object slot.
     * Culling before this call is deliberately NOT treated as cleanup proof. */
    if (gSaveBlock1Ptr == NULL || !AtlasWorld_IsActive()
     || gSaveBlock1Ptr->location.mapNum != binding.mapNum
     || gSaveBlock1Ptr->location.mapGroup != binding.mapGroup
     || !AtlasTryCaptureTransientSpawnReceipt(binding.objectEventId, localId,
                                               binding.mapNum, binding.mapGroup,
                                               &actualSpriteId)
     || actualSpriteId != binding.spriteId)
        return ATLAS_ACTOR_RETIRE_OWNERSHIP_LOST;

    if (!AtlasTryRemoveTransientObjectEvent(binding.objectEventId, localId,
                                             binding.mapNum, binding.mapGroup,
                                             binding.spriteId))
        return ATLAS_ACTOR_RETIRE_REMOVE_BLOCKED;

    if (!AtlasActorAdmission_Retire(roster, handle))
        return ATLAS_ACTOR_RETIRE_ROSTER_FAULT;

    return ATLAS_ACTOR_RETIRE_COMPLETED;
}
