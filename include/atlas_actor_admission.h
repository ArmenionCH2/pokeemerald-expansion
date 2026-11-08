#ifndef GUARD_ATLAS_ACTOR_ADMISSION_H
#define GUARD_ATLAS_ACTOR_ADMISSION_H

#include "atlas_actor_identity.h"

/* Small transient admission ledger. No field objects, sprites, or save data
 * are owned by this module. The caller must zero-initialize the roster and
 * call Reset before its first use. */
#define ATLAS_ACTOR_ROSTER_CAPACITY 3

enum AtlasActorAdmissionState
{
    ATLAS_ACTOR_EMPTY,
    ATLAS_ACTOR_PENDING,
    ATLAS_ACTOR_LIVE
};

/* Native object/sprite slots are transient, never persistent actor identity.
 * Emerald's object-event failure sentinel is 16 and sprite failure sentinel
 * is 64. A caller MUST verify both slots are live and owned by this actor
 * before CommitBound; this pure roster does not read gObjectEvents/gSprites. */
#define ATLAS_ACTOR_OBJECT_EVENT_LIMIT 16
#define ATLAS_ACTOR_SPRITE_LIMIT 64

struct AtlasActorNativeBinding
{
    u8 objectEventId;
    u8 spriteId;
    u8 mapNum;
    u8 mapGroup;
};

struct AtlasActorAdmissionSlot
{
    struct AtlasActorSpawnKey key;
    u32 generation;
    struct AtlasActorNativeBinding binding;
    u8 localId;
    u8 state;
    bool8 bound;
};

struct AtlasActorAdmissionRoster
{
    struct AtlasActorAdmissionSlot slots[ATLAS_ACTOR_ROSTER_CAPACITY];
};

/* Reset invalidates all handles, including failed/pending reservations.
 * Call only on a zero-initialized or previously valid roster. */
void AtlasActorAdmission_Reset(struct AtlasActorAdmissionRoster *roster);

/* freeObjects/freeSprites are fresh native free-slot counts INCLUDING all
 * pending reservations but EXCLUDING live actors already allocated. Reserve
 * subtracts outstanding pending reservations to avoid double admission.
 * reserveObjects/reserveSprites protect non-Atlas owners. No allocation is
 * performed: the caller MUST revalidate real capacity and successful native
 * creation before Commit. Birthplace X must be canonical [0, WORLD_WIDTH),
 * and Y must be inside [-WORLD_HALF_HEIGHT, WORLD_HALF_HEIGHT); latitude
 * never wraps. Local IDs 0 and 127..255 are deliberately excluded (camera/link/player
 * namespace); only 1..126 can be admitted. */
bool8 AtlasActorAdmission_Reserve(struct AtlasActorAdmissionRoster *roster,
                                  const struct AtlasActorSpawnKey *key,
                                  u8 localId, u16 freeObjects, u16 freeSprites,
                                  u16 reserveObjects, u16 reserveSprites,
                                  struct AtlasActorHandle *out);
bool8 AtlasActorAdmission_Commit(struct AtlasActorAdmissionRoster *roster,
                                 struct AtlasActorHandle handle);
/* Atomically promote PENDING to LIVE with verified native slot IDs.
 * Reject invalid sentinels and collisions with other LIVE Atlas bindings.
 * Does not inspect or allocate native objects; caller verifies actual ownership.
 * Legacy Commit creates an UNBOUND live slot; never use that for cleanup. */
bool8 AtlasActorAdmission_CommitBound(struct AtlasActorAdmissionRoster *roster,
                                      struct AtlasActorHandle handle,
                                      const struct AtlasActorNativeBinding *binding);
/* A failed lookup invalidates the caller-owned output (16/64 sentinels),
 * preventing a stale receipt from being reused after culling/retirement.
 * Output must not alias any internal roster binding; such calls are rejected
 * without mutation. NULL output is rejected. This is a pure roster lookup,
 * not proof of native sprite existence or camera-culling reconciliation. */
bool8 AtlasActorAdmission_ResolveBound(const struct AtlasActorAdmissionRoster *roster,
                                       struct AtlasActorHandle handle,
                                       struct AtlasActorNativeBinding *binding);
/* Call ONLY after the engine has independently confirmed that this exact
 * native object/sprite was removed (e.g. camera culling). Match a snapshot
 * captured BEFORE removal, its local/map identity and the actor generation.
 * This pure roster operation does not inspect or remove engine objects,
 * set event flags, or prove native removal. Rejects internal receipt aliases
 * and stale generations, including native slot reuse (ABA).
 * Camera culling and bulk native removal call through the owned runtime;\n * reset and return-to-field have separate lifecycle contracts. */
bool8 AtlasActorAdmission_ConfirmNativeRemoval(
    struct AtlasActorAdmissionRoster *roster,
    struct AtlasActorHandle handle,
    const struct AtlasActorNativeBinding *expected,
    u8 localId);
bool8 AtlasActorAdmission_Abort(struct AtlasActorAdmissionRoster *roster,
                                struct AtlasActorHandle handle);
bool8 AtlasActorAdmission_Retire(struct AtlasActorAdmissionRoster *roster,
                                 struct AtlasActorHandle handle);
bool8 AtlasActorAdmission_Resolve(const struct AtlasActorAdmissionRoster *roster,
                                  struct AtlasActorHandle handle,
                                  struct AtlasActorSpawnKey *key, u8 *localId);

#endif
