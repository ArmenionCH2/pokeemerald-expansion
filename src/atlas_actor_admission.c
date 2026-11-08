#include "global.h"
#include "atlas_actor_admission.h"
#include "constants/event_objects.h"

static void ClearSlot(struct AtlasActorAdmissionSlot *slot)
{
    slot->key.worldSeed = 0;
    slot->key.worldX = 0;
    slot->key.worldY = 0;
    slot->key.species = 0;
    slot->key.generationVersion = 0;
    slot->localId = 0;
    slot->binding.objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
    slot->binding.spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    slot->binding.mapNum = 0;
    slot->binding.mapGroup = 0;
    slot->bound = FALSE;
    slot->state = ATLAS_ACTOR_EMPTY;
    slot->generation = AtlasActor_NextGeneration(slot->generation);
}

void AtlasActorAdmission_Reset(struct AtlasActorAdmissionRoster *roster)
{
    u16 i;
    if (roster == NULL)
        return;
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
        ClearSlot(&roster->slots[i]);
}

static struct AtlasActorAdmissionSlot *FindSlot(struct AtlasActorAdmissionRoster *roster,
                                                struct AtlasActorHandle handle)
{
    struct AtlasActorAdmissionSlot *slot;
    if (roster == NULL || handle.slot >= ATLAS_ACTOR_ROSTER_CAPACITY)
        return NULL;
    slot = &roster->slots[handle.slot];
    if (!AtlasActor_HandleMatches(handle,
            (struct AtlasActorHandle){slot->generation, handle.slot},
            ATLAS_ACTOR_ROSTER_CAPACITY))
        return NULL;
    return slot;
}

bool8 AtlasActorAdmission_Reserve(struct AtlasActorAdmissionRoster *roster,
                                  const struct AtlasActorSpawnKey *key,
                                  u8 localId, u16 freeObjects, u16 freeSprites,
                                  u16 reserveObjects, u16 reserveSprites,
                                  struct AtlasActorHandle *out)
{
    u16 i, pending = 0;
    u16 vacant = ATLAS_ACTOR_ROSTER_CAPACITY;
    if (out != NULL)
    {
        out->generation = 0;
        out->slot = ATLAS_ACTOR_ROSTER_CAPACITY;
    }
    /* Dynamic Atlas actors must not claim the camera (127), link-player,
     * or player local-ID namespace. Reserve only 1..126. */
    if (roster == NULL || key == NULL || out == NULL || localId == 0 || localId >= LOCALID_CAMERA
     || key->species == 0 || key->generationVersion == 0
     || key->worldX < 0 || key->worldX >= ATLAS_WORLD_WIDTH
     || key->worldY < -ATLAS_WORLD_HALF_HEIGHT
     || key->worldY >= ATLAS_WORLD_HALF_HEIGHT)
        return FALSE;
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        const struct AtlasActorAdmissionSlot *slot = &roster->slots[i];
        if (slot->state == ATLAS_ACTOR_EMPTY)
        {
            if (vacant == ATLAS_ACTOR_ROSTER_CAPACITY)
                vacant = i;
        }
        else if (slot->state == ATLAS_ACTOR_PENDING || slot->state == ATLAS_ACTOR_LIVE)
        {
            if (slot->localId == localId || AtlasActor_SpawnKeyEquals(&slot->key, key))
                return FALSE;
            if (slot->state == ATLAS_ACTOR_PENDING)
                pending++;
        }
        else
            return FALSE;
    }
    if (vacant == ATLAS_ACTOR_ROSTER_CAPACITY
     || (u32)freeObjects <= (u32)reserveObjects + pending
     || (u32)freeSprites <= (u32)reserveSprites + pending)
        return FALSE;
    roster->slots[vacant].generation = AtlasActor_NextGeneration(roster->slots[vacant].generation);
    roster->slots[vacant].key = *key;
    roster->slots[vacant].localId = localId;
    roster->slots[vacant].binding.objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
    roster->slots[vacant].binding.spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    roster->slots[vacant].binding.mapNum = 0;
    roster->slots[vacant].binding.mapGroup = 0;
    roster->slots[vacant].bound = FALSE;
    roster->slots[vacant].state = ATLAS_ACTOR_PENDING;
    out->slot = vacant;
    out->generation = roster->slots[vacant].generation;
    return TRUE;
}

bool8 AtlasActorAdmission_Commit(struct AtlasActorAdmissionRoster *roster,
                                 struct AtlasActorHandle handle)
{
    struct AtlasActorAdmissionSlot *slot = FindSlot(roster, handle);
    if (slot == NULL || slot->state != ATLAS_ACTOR_PENDING)
        return FALSE;
    slot->state = ATLAS_ACTOR_LIVE;
    return TRUE;
}

/* The native caller must validate actual object/sprite ownership before
 * passing a binding. This contract only prevents invalid sentinels and
 * two Atlas actors claiming the same live native slot. */
bool8 AtlasActorAdmission_CommitBound(struct AtlasActorAdmissionRoster *roster,
                                      struct AtlasActorHandle handle,
                                      const struct AtlasActorNativeBinding *binding)
{
    struct AtlasActorAdmissionSlot *slot = FindSlot(roster, handle);
    u16 i;
    if (slot == NULL || slot->state != ATLAS_ACTOR_PENDING || binding == NULL
     || binding->objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT
     || binding->spriteId >= ATLAS_ACTOR_SPRITE_LIMIT)
        return FALSE;
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
    {
        const struct AtlasActorAdmissionSlot *other = &roster->slots[i];
        if (other != slot && other->state == ATLAS_ACTOR_LIVE && other->bound
         && (other->binding.objectEventId == binding->objectEventId
          || other->binding.spriteId == binding->spriteId))
            return FALSE;
    }
    slot->binding = *binding;
    slot->bound = TRUE;
    slot->state = ATLAS_ACTOR_LIVE;
    return TRUE;
}

bool8 AtlasActorAdmission_ResolveBound(const struct AtlasActorAdmissionRoster *roster,
                                       struct AtlasActorHandle handle,
                                       struct AtlasActorNativeBinding *binding)
{
    u16 i;
    /* Output must not alias internal roster storage: a stale lookup must
     * never corrupt another actor's live binding. */
    if (binding == NULL)
        return FALSE;
    if (roster != NULL)
    {
        for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
        {
            if (binding == &roster->slots[i].binding)
                return FALSE;
        }
    }
    /* Fail closed: never leave a previously valid object/sprite receipt in
     * a caller's output after a stale, pending or unbound lookup. */
    binding->objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
    binding->spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
    binding->mapNum = 0;
    binding->mapGroup = 0;
    if (!AtlasActorAdmission_Resolve(roster, handle, NULL, NULL)
     || !roster->slots[handle.slot].bound)
        return FALSE;
    *binding = roster->slots[handle.slot].binding;
    return TRUE;
}

/* A camera/removal owner must provide its pre-removal receipt and
 * generation. Native slot numbers alone are never durable identity. */
bool8 AtlasActorAdmission_ConfirmNativeRemoval(
    struct AtlasActorAdmissionRoster *roster,
    struct AtlasActorHandle handle,
    const struct AtlasActorNativeBinding *expected,
    u8 localId)
{
    struct AtlasActorAdmissionSlot *slot;
    u16 i;

    if (expected == NULL || localId == 0 || localId >= LOCALID_CAMERA
     || expected->objectEventId >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT
     || expected->spriteId >= ATLAS_ACTOR_SPRITE_LIMIT)
        return FALSE;
    /* The receipt must be an independent snapshot, not a pointer into the
     * mutable roster. It is NOT itself proof that native removal occurred. */
    if (roster != NULL)
    {
        for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
        {
            if (expected == &roster->slots[i].binding)
                return FALSE;
        }
    }
    slot = FindSlot(roster, handle);
    if (slot == NULL || slot->state != ATLAS_ACTOR_LIVE || !slot->bound
     || slot->localId != localId
     || slot->binding.objectEventId != expected->objectEventId
     || slot->binding.spriteId != expected->spriteId
     || slot->binding.mapNum != expected->mapNum
     || slot->binding.mapGroup != expected->mapGroup)
        return FALSE;
    ClearSlot(slot);
    return TRUE;
}

bool8 AtlasActorAdmission_Abort(struct AtlasActorAdmissionRoster *roster,
                                struct AtlasActorHandle handle)
{
    struct AtlasActorAdmissionSlot *slot = FindSlot(roster, handle);
    if (slot == NULL || slot->state != ATLAS_ACTOR_PENDING)
        return FALSE;
    ClearSlot(slot);
    return TRUE;
}

bool8 AtlasActorAdmission_Retire(struct AtlasActorAdmissionRoster *roster,
                                 struct AtlasActorHandle handle)
{
    struct AtlasActorAdmissionSlot *slot = FindSlot(roster, handle);
    if (slot == NULL || slot->state != ATLAS_ACTOR_LIVE)
        return FALSE;
    ClearSlot(slot);
    return TRUE;
}

bool8 AtlasActorAdmission_Resolve(const struct AtlasActorAdmissionRoster *roster,
                                  struct AtlasActorHandle handle,
                                  struct AtlasActorSpawnKey *key, u8 *localId)
{
    const struct AtlasActorAdmissionSlot *slot;
    if (roster == NULL || handle.slot >= ATLAS_ACTOR_ROSTER_CAPACITY)
        return FALSE;
    slot = &roster->slots[handle.slot];
    if (slot->state != ATLAS_ACTOR_LIVE || !AtlasActor_HandleMatches(handle,
        (struct AtlasActorHandle){slot->generation, handle.slot},
        ATLAS_ACTOR_ROSTER_CAPACITY))
        return FALSE;
    if (key != NULL)
        *key = slot->key;
    if (localId != NULL)
        *localId = slot->localId;
    return TRUE;
}
