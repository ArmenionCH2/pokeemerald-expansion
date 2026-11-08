#include "global.h"
#include "atlas_actor_runtime.h"
#include "constants/event_objects.h"

/* Stable EWRAM address for all native Atlas actors; no borrowed roster. */
static EWRAM_DATA struct AtlasActorAdmissionRoster sAtlasActorRoster = {0};
static EWRAM_DATA bool8 sAtlasActorRosterInitialized = FALSE;
static EWRAM_DATA bool8 sAtlasSpriteRestoreActive = FALSE;

struct AtlasActorAdmissionRoster *AtlasActorRuntime_GetRoster(void)
{
    if (!sAtlasActorRosterInitialized)
    {
        AtlasActorAdmission_Reset(&sAtlasActorRoster);
        sAtlasActorRosterInitialized = TRUE;
    }
    return &sAtlasActorRoster;
}

/* Field removal must not initialize an otherwise unused actor roster. */
struct AtlasActorAdmissionRoster *AtlasActorRuntime_PeekRoster(void)
{
    if (!sAtlasActorRosterInitialized)
        return NULL;
    return &sAtlasActorRoster;
}

void AtlasActorRuntime_ResetAfterNativeClear(void)
{
    sAtlasSpriteRestoreActive = FALSE;
    if (!sAtlasActorRosterInitialized)
        (void)AtlasActorRuntime_GetRoster();
    else
        AtlasActorAdmission_Reset(&sAtlasActorRoster);
}

/* Call ONLY after ResetSpriteData and before return-to-field CreateSprite.
 * Old sprite IDs have no ownership meaning after the reset. */
bool8 AtlasActorRuntime_BeginSpriteRestore(void)
{
    u16 i;

    if (!sAtlasActorRosterInitialized || sAtlasSpriteRestoreActive)
        return FALSE;
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        struct AtlasActorAdmissionSlot *slot = &sAtlasActorRoster.slots[i];
        if (slot->state == ATLAS_ACTOR_LIVE && slot->bound)
            slot->binding.spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    }
    sAtlasSpriteRestoreActive = TRUE;
    return TRUE;
}

void AtlasActorRuntime_EndSpriteRestore(void)
{
    sAtlasSpriteRestoreActive = FALSE;
}

/* Snapshot a matching LIVE binding before native removal; the caller must
 * independently verify the live native object and sprite before capture. */
bool8 AtlasActorRuntime_CaptureBound(u8 objectEventId, u8 spriteId, u8 localId,
                                     u8 mapNum, u8 mapGroup,
                                     struct AtlasActorRemovalReceipt *out)
{
    struct AtlasActorAdmissionRoster *roster = AtlasActorRuntime_PeekRoster();
    u16 i;
    u16 found = ATLAS_ACTOR_ROSTER_CAPACITY;

    if (out == NULL)
        return FALSE;
    out->handle.slot = ATLAS_ACTOR_ROSTER_CAPACITY;
    out->handle.generation = 0;
    out->binding.objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
    out->binding.spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    out->binding.mapNum = 0;
    out->binding.mapGroup = 0;
    out->localId = 0;
    if (roster == NULL || objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT
     || spriteId >= ATLAS_ACTOR_SPRITE_LIMIT || localId == 0
     || localId >= LOCALID_CAMERA)
        return FALSE;

    /* Native object and sprite slots are globally owned, regardless of
     * local/map labels. Any other LIVE claim to either native slot makes
     * removal ambiguous; never select the first matching generation. */
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        const struct AtlasActorAdmissionSlot *slot = &roster->slots[i];
        if (slot->state != ATLAS_ACTOR_LIVE || !slot->bound)
            continue;
        if (slot->binding.objectEventId != objectEventId
         && slot->binding.spriteId != spriteId)
            continue;
        if (found != ATLAS_ACTOR_ROSTER_CAPACITY
         || slot->localId != localId
         || slot->binding.objectEventId != objectEventId
         || slot->binding.spriteId != spriteId
         || slot->binding.mapNum != mapNum
         || slot->binding.mapGroup != mapGroup)
            return FALSE;
        found = i;
    }
    if (found == ATLAS_ACTOR_ROSTER_CAPACITY)
        return FALSE;

    out->handle.slot = found;
    out->handle.generation = roster->slots[found].generation;
    out->binding = roster->slots[found].binding;
    out->localId = localId;
    return TRUE;
}

/* Only the field engine calls this AFTER it has independently confirmed
 * that the captured object and sprite have both been removed. */
bool8 AtlasActorRuntime_ConfirmRemoval(const struct AtlasActorRemovalReceipt *receipt)
{
    struct AtlasActorAdmissionRoster *roster = AtlasActorRuntime_PeekRoster();
    if (roster == NULL || receipt == NULL)
        return FALSE;
    return AtlasActorAdmission_ConfirmNativeRemoval(roster, receipt->handle,
                                                    &receipt->binding, receipt->localId);
}

/* ResetSpriteData destroys sprite records, not object-event records.
 * Capture a generation-bound old receipt by object identity only; never
 * dereference the old sprite ID during return-to-field restoration. */
bool8 AtlasActorRuntime_CaptureForSpriteRestore(u8 objectEventId, u8 localId,
                                                u8 mapNum, u8 mapGroup,
                                                struct AtlasActorRemovalReceipt *out)
{
    struct AtlasActorAdmissionRoster *roster = AtlasActorRuntime_PeekRoster();
    u16 i;
    u16 found = ATLAS_ACTOR_ROSTER_CAPACITY;

    if (out == NULL)
        return FALSE;
    out->handle.slot = ATLAS_ACTOR_ROSTER_CAPACITY;
    out->handle.generation = 0;
    out->binding.objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
    out->binding.spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    out->binding.mapNum = 0;
    out->binding.mapGroup = 0;
    out->localId = 0;
    if (!sAtlasSpriteRestoreActive || roster == NULL
     || objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT
     || localId == 0 || localId >= LOCALID_CAMERA)
        return FALSE;

    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        const struct AtlasActorAdmissionSlot *slot = &roster->slots[i];
        /* Native object IDs are globally owned. A second LIVE claimant
         * with another map/local label is still a collision, not a
         * different actor that can safely receive a restored sprite. */
        if (slot->state != ATLAS_ACTOR_LIVE || !slot->bound
         || slot->binding.objectEventId != objectEventId)
            continue;
        if (found != ATLAS_ACTOR_ROSTER_CAPACITY
         || slot->localId != localId
         || slot->binding.spriteId != ATLAS_ACTOR_SPRITE_LIMIT
         || slot->binding.mapNum != mapNum
         || slot->binding.mapGroup != mapGroup)
            return FALSE;
        found = i;
    }
    if (found == ATLAS_ACTOR_ROSTER_CAPACITY)
        return FALSE;

    out->handle.slot = found;
    out->handle.generation = roster->slots[found].generation;
    out->binding = roster->slots[found].binding;
    out->localId = localId;
    return TRUE;
}

/* The field engine must verify gObjectEvents/gSprites independently before
 * this pure roster mutation. A receipt captured before CreateSprite prevents
 * stale generations or changed native object identities from being rebound. */
bool8 AtlasActorRuntime_CommitRestoredSprite(const struct AtlasActorRemovalReceipt *receipt,
                                              u8 newSpriteId)
{
    struct AtlasActorAdmissionRoster *roster = AtlasActorRuntime_PeekRoster();
    struct AtlasActorNativeBinding current;
    struct AtlasActorAdmissionSlot *slot;
    u16 i;

    if (!sAtlasSpriteRestoreActive || roster == NULL || receipt == NULL
     || newSpriteId >= ATLAS_ACTOR_SPRITE_LIMIT
     || receipt->handle.slot >= ATLAS_ACTOR_ROSTER_CAPACITY
     || receipt->localId == 0 || receipt->localId >= LOCALID_CAMERA
     || receipt->binding.objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT
     || receipt->binding.spriteId != ATLAS_ACTOR_SPRITE_LIMIT)
        return FALSE;
    if (!AtlasActorAdmission_ResolveBound(roster, receipt->handle, &current))
        return FALSE;
    slot = &roster->slots[receipt->handle.slot];
    if (slot->localId != receipt->localId
     || current.objectEventId != receipt->binding.objectEventId
     || current.spriteId != receipt->binding.spriteId
     || current.mapNum != receipt->binding.mapNum
     || current.mapGroup != receipt->binding.mapGroup)
        return FALSE;

    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        const struct AtlasActorAdmissionSlot *other = &roster->slots[i];
        if (i != receipt->handle.slot && other->state == ATLAS_ACTOR_LIVE
         && other->bound
         && (other->binding.spriteId == newSpriteId
          || other->binding.objectEventId == current.objectEventId))
            return FALSE;
    }
    slot->binding.spriteId = newSpriteId;
    return TRUE;
}

/* Read-only generation/identity authorization before removing a freshly
 * created sprite after failed rebind. This does NOT prove native sprite
 * ownership: the field caller must independently verify gObjectEvents and
 * gSprites before flag-free teardown. Another roster's collision cannot
 * authorize retiring that other generation. */
bool8 AtlasActorRuntime_CanAbandonFailedSpriteRestore(const struct AtlasActorRemovalReceipt *receipt)
{
    struct AtlasActorAdmissionRoster *roster = AtlasActorRuntime_PeekRoster();
    struct AtlasActorNativeBinding current;
    const struct AtlasActorAdmissionSlot *slot;
    u16 i;

    if (!sAtlasSpriteRestoreActive || roster == NULL || receipt == NULL
     || receipt->handle.slot >= ATLAS_ACTOR_ROSTER_CAPACITY
     || receipt->localId == 0 || receipt->localId >= LOCALID_CAMERA
     || receipt->binding.objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT
     || receipt->binding.spriteId != ATLAS_ACTOR_SPRITE_LIMIT)
        return FALSE;
    if (!AtlasActorAdmission_ResolveBound(roster, receipt->handle, &current))
        return FALSE;
    slot = &roster->slots[receipt->handle.slot];
    if (slot->localId != receipt->localId
     || current.objectEventId != receipt->binding.objectEventId
     || current.spriteId != ATLAS_ACTOR_SPRITE_LIMIT
     || current.mapNum != receipt->binding.mapNum
     || current.mapGroup != receipt->binding.mapGroup)
        return FALSE;

    /* A second live actor claiming this object makes destructive cleanup
     * ambiguous, even when the receipt generation itself is valid. */
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        const struct AtlasActorAdmissionSlot *other = &roster->slots[i];
        if (i != receipt->handle.slot && other->state == ATLAS_ACTOR_LIVE
         && other->bound && other->binding.objectEventId == current.objectEventId)
            return FALSE;
    }
    return TRUE;
}

/* Call ONLY after the native sprite/object were independently removed, or
 * after CreateSprite failed and the object record was deactivated. */
bool8 AtlasActorRuntime_AbandonFailedSpriteRestore(const struct AtlasActorRemovalReceipt *receipt)
{
    if (!AtlasActorRuntime_CanAbandonFailedSpriteRestore(receipt))
        return FALSE;
    return AtlasActorAdmission_Retire(AtlasActorRuntime_PeekRoster(), receipt->handle);
}
