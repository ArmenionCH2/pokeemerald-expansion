#ifndef GUARD_ATLAS_ACTOR_RUNTIME_H
#define GUARD_ATLAS_ACTOR_RUNTIME_H

#include "atlas_actor_admission.h"

/* Engine-lifetime transient actor owner. The returned address is stable:
 * never retain a pointer to a caller's stack roster in field callbacks.
 * GetRoster lazily initializes zeroed EWRAM storage on first use.
 * This does NOT spawn objects, register callbacks, or save actors. */
struct AtlasActorAdmissionRoster *AtlasActorRuntime_GetRoster(void);

/* Only call AFTER Emerald has cleared its native object/sprite ownership
 * during a verified map reset. Invalidates every pending/live generation;
 * never call while an Atlas-owned native object remains active. No field
 * Map-load entrypoints call it only after ResumeMap resets sprites and\n * ResetObjectEvents clears native object records; return-to-field does not. */
void AtlasActorRuntime_ResetAfterNativeClear(void);

/* A camera-cull callback must not lazily create an unused roster. */
struct AtlasActorAdmissionRoster *AtlasActorRuntime_PeekRoster(void);

/* Snapshot native identity BEFORE culling and confirm only AFTER the
 * field engine has verified both object and sprite teardown. The receipt
 * contains a generation: reused native slots cannot retire another actor.
 * Capture rejects ambiguous LIVE claims to either native object or sprite,
 * even if one actor otherwise matches the requested map/local identity.
 * No event flags, sprite allocations, save data, or palette changes here. */
struct AtlasActorRemovalReceipt
{
    struct AtlasActorHandle handle;
    struct AtlasActorNativeBinding binding;
    u8 localId;
};
bool8 AtlasActorRuntime_CaptureBound(u8 objectEventId, u8 spriteId, u8 localId,
                                     u8 mapNum, u8 mapGroup,
                                     struct AtlasActorRemovalReceipt *out);
bool8 AtlasActorRuntime_ConfirmRemoval(const struct AtlasActorRemovalReceipt *receipt);

/* Bracket synchronous return-to-field recreation after ResetSpriteData.
 * Begin invalidates pre-reset Atlas sprite claims with sentinel 64; only
 * freshly verified native sprite IDs may be rebound. */
bool8 AtlasActorRuntime_BeginSpriteRestore(void);
void AtlasActorRuntime_EndSpriteRestore(void);

/* Capture a generation-bound object identity with stale sprite sentinel 64
 * while the phase is active. Reject any duplicate LIVE claim to the same
 * native object ID, even across different local/map labels. The caller must
 * still independently verify native object identity before CreateSprite. */
bool8 AtlasActorRuntime_CaptureForSpriteRestore(u8 objectEventId, u8 localId,
                                                u8 mapNum, u8 mapGroup,
                                                struct AtlasActorRemovalReceipt *out);
/* Commit only after independent native new-sprite ownership verification.
 * Reject stale generations, changed receipts and fresh-slot collisions. */
bool8 AtlasActorRuntime_CommitRestoredSprite(const struct AtlasActorRemovalReceipt *receipt,
                                              u8 newSpriteId);

/* Read-only generation and stale-sprite receipt authorization for cleanup
 * after failed CreateSprite OR after successful creation but failed rebind.
 * Not proof of native object/sprite ownership; never remove by slot alone. */
bool8 AtlasActorRuntime_CanAbandonFailedSpriteRestore(const struct AtlasActorRemovalReceipt *receipt);
/* Only after independent native teardown/deactivation, retire exactly the
 * same Atlas generation. No event flags or shared palette ownership here. */
bool8 AtlasActorRuntime_AbandonFailedSpriteRestore(const struct AtlasActorRemovalReceipt *receipt);

#endif
