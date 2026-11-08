#ifndef GUARD_ATLAS_ACTOR_SPAWN_H
#define GUARD_ATLAS_ACTOR_SPAWN_H

#include "atlas_actor_admission.h"

struct ObjectEventTemplate;

/* CLEANUP_BLOCKED means a native object may remain alive: the pending
 * reservation is deliberately retained for explicit recovery.
 * ROSTER_FAULT means native creation failed or cleanup succeeded, but
 * the pending reservation could not be invalidated. Both fail closed. */
enum AtlasActorSpawnResult
{
    ATLAS_ACTOR_SPAWN_REJECTED,
    ATLAS_ACTOR_SPAWN_ROLLED_BACK,
    ATLAS_ACTOR_SPAWN_COMMITTED,
    ATLAS_ACTOR_SPAWN_CLEANUP_BLOCKED,
    /* Native cleanup is complete (or native creation failed), but the
     * pending admission could not be invalidated. Do not claim rollback. */
    ATLAS_ACTOR_SPAWN_ROSTER_FAULT
};

/* Only for the active Atlas map with zero permanent object templates.
 * The caller must first Reserve a generation-checked actor and supply a
 * temporary, non-scripted, non-trainer template with the same local ID.
 * AtlasWildlife_Tick uses this after CameraUpdate. Actors are transient; native
 * culling and sprite-restore hooks maintain the runtime ledger.
 * outBinding is invalidated on every non-success return. */
enum AtlasActorSpawnResult AtlasActorSpawn_TryCommitPending(
    struct AtlasActorAdmissionRoster *roster,
    struct AtlasActorHandle handle,
    struct ObjectEventTemplate *objectTemplate,
    struct AtlasActorNativeBinding *outBinding);

/* Generation-checked retirement of a LIVE, BOUND transient actor.
 * No event flags are set. REJECTED means stale/unbound handle;
 * OWNERSHIP_LOST means current map/object/sprite cannot be proven;
 * REMOVE_BLOCKED means native flag-free removal refused;
 * ROSTER_FAULT means native removal succeeded but roster retirement failed.
 * Only COMPLETED proves both native removal and roster invalidation.
 * Failure never authorizes blind local-ID removal, retry, or save mutation.
 * Call before camera culling; culling/transition hooks maintain the ledger. */
enum AtlasActorRetireResult
{
    ATLAS_ACTOR_RETIRE_REJECTED,
    ATLAS_ACTOR_RETIRE_OWNERSHIP_LOST,
    ATLAS_ACTOR_RETIRE_REMOVE_BLOCKED,
    ATLAS_ACTOR_RETIRE_ROSTER_FAULT,
    ATLAS_ACTOR_RETIRE_COMPLETED
};

enum AtlasActorRetireResult AtlasActorSpawn_TryRetireBound(
    struct AtlasActorAdmissionRoster *roster, struct AtlasActorHandle handle);

#endif
