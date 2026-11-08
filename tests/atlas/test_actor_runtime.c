#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "atlas_actor_runtime.h"

int main(void)
{
    struct AtlasActorAdmissionRoster *roster = AtlasActorRuntime_GetRoster();
    struct AtlasActorSpawnKey key = {0x1234, 2, -4, 25, 3};
    struct AtlasActorSpawnKey key2 = {0x1234, 5, -6, 26, 3};
    struct AtlasActorNativeBinding native = {3, 4, 5, 7};
    struct AtlasActorNativeBinding native2 = {6, 7, 5, 7};
    struct AtlasActorNativeBinding resolved;
    struct AtlasActorHandle old, current, second;
    struct AtlasActorRemovalReceipt receipt, receipt2, stale;
    unsigned i;

    assert(roster != NULL && roster == AtlasActorRuntime_GetRoster());
    assert(AtlasActorRuntime_PeekRoster() == roster);
    assert(AtlasActorRuntime_BeginSpriteRestore());
    AtlasActorRuntime_EndSpriteRestore();
    assert(!AtlasActorRuntime_CaptureBound(3, 4, 1, 5, 7, &receipt));
    for (i = 0; i < ATLAS_ACTOR_ROSTER_CAPACITY; i++)
        assert(roster->slots[i].state == ATLAS_ACTOR_EMPTY);
    assert(AtlasActorAdmission_Reserve(roster, &key, 1, 3, 3, 0, 0, &old));
    assert(AtlasActorAdmission_CommitBound(roster, old, &native));
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &receipt));
    assert(AtlasActorRuntime_BeginSpriteRestore());
    assert(!AtlasActorRuntime_BeginSpriteRestore());
    assert(AtlasActorAdmission_ResolveBound(roster, old, &resolved));
    assert(resolved.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 8, &receipt));
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 127, 5, 7, &receipt));
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(16, 1, 5, 7, &receipt));
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, NULL));
    assert(AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &receipt));
    assert(receipt.binding.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
    assert(!AtlasActorRuntime_CommitRestoredSprite(&receipt, 64));
    stale = receipt;
    stale.handle.generation++;
    assert(!AtlasActorRuntime_CommitRestoredSprite(&stale, 8));
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&stale));
    assert(AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt));
    assert(AtlasActorRuntime_CommitRestoredSprite(&receipt, 8));
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt));
    assert(!AtlasActorRuntime_CommitRestoredSprite(&receipt, 9));
    AtlasActorRuntime_EndSpriteRestore();
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &receipt2));
    assert(!AtlasActorRuntime_CommitRestoredSprite(&receipt, 9));
    assert(AtlasActorAdmission_ResolveBound(roster, old, &resolved));
    assert(resolved.spriteId == 8);

    AtlasActorRuntime_ResetAfterNativeClear();
    assert(!AtlasActorAdmission_ResolveBound(roster, old, &resolved));
    assert(!AtlasActorRuntime_CommitRestoredSprite(&receipt, 9));
    assert(AtlasActorAdmission_Reserve(roster, &key, 1, 3, 3, 0, 0, &current));
    assert(current.generation != old.generation);
    assert(!AtlasActorAdmission_CommitBound(roster, old, &native));
    assert(AtlasActorAdmission_Abort(roster, current));

    /* Actor one can reuse actor two's OLD sprite ID after ResetSpriteData. */
    assert(AtlasActorAdmission_Reserve(roster, &key, 1, 3, 3, 0, 0, &current));
    assert(AtlasActorAdmission_CommitBound(roster, current, &native));
    assert(AtlasActorAdmission_Reserve(roster, &key2, 2, 3, 3, 0, 0, &second));
    assert(AtlasActorAdmission_CommitBound(roster, second, &native2));
    /* Corrupt duplicate LIVE native owners must not yield a destructive
     * camera/bulk-removal receipt, even if one actor matches exactly.
     * Normal admission rejects these collisions; inject them here to
     * verify the runtime's fail-closed recovery behavior. */
    roster->slots[second.slot].binding.objectEventId = native.objectEventId;
    assert(!AtlasActorRuntime_CaptureBound(3, 4, 1, 5, 7, &receipt));
    assert(receipt.handle.slot == ATLAS_ACTOR_ROSTER_CAPACITY);
    assert(!AtlasActorRuntime_ConfirmRemoval(&receipt));
    roster->slots[second.slot].binding = native2;

    roster->slots[second.slot].binding.spriteId = native.spriteId;
    assert(!AtlasActorRuntime_CaptureBound(3, 4, 1, 5, 7, &receipt));
    assert(receipt.binding.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
    roster->slots[second.slot].binding = native2;

    roster->slots[second.slot].binding = native;
    roster->slots[second.slot].localId = 1;
    assert(!AtlasActorRuntime_CaptureBound(3, 4, 1, 5, 7, &receipt));
    assert(receipt.handle.slot == ATLAS_ACTOR_ROSTER_CAPACITY);
    roster->slots[second.slot].binding = native2;
    roster->slots[second.slot].localId = 2;

    assert(AtlasActorAdmission_ResolveBound(roster, current, &resolved));
    assert(resolved.objectEventId == 3 && resolved.spriteId == 4);
    assert(AtlasActorAdmission_ResolveBound(roster, second, &resolved));
    assert(resolved.objectEventId == 6 && resolved.spriteId == 7);
    assert(AtlasActorRuntime_CaptureBound(3, 4, 1, 5, 7, &receipt));
    assert(AtlasActorRuntime_BeginSpriteRestore());
    assert(!AtlasActorRuntime_CaptureBound(3, 4, 1, 5, 7, &receipt2));
    assert(AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &receipt));
    assert(AtlasActorRuntime_CaptureForSpriteRestore(6, 2, 5, 7, &receipt2));
    /* Return-to-field receipt capture must reject any duplicate global
     * native object owner BEFORE CreateSprite, not only identical map/local
     * labels. Admission normally prevents this; inject corrupted state to
     * exercise the fail-closed recovery contract. */
    roster->slots[second.slot].binding.objectEventId = native.objectEventId;
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &stale));
    assert(stale.handle.slot == ATLAS_ACTOR_ROSTER_CAPACITY);
    assert(stale.binding.objectEventId == ATLAS_ACTOR_OBJECT_EVENT_LIMIT);
    assert(stale.binding.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt));
    /* A conflicting map label must not make the duplicate safe. */
    roster->slots[second.slot].binding.mapNum = 6;
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &stale));
    assert(stale.handle.slot == ATLAS_ACTOR_ROSTER_CAPACITY);
    roster->slots[second.slot].binding.mapNum = native2.mapNum;
    /* Even two exact labels cannot authorize either generation. */
    roster->slots[second.slot].localId = 1;
    assert(!AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &stale));
    assert(stale.handle.slot == ATLAS_ACTOR_ROSTER_CAPACITY);
    roster->slots[second.slot].localId = 2;
    roster->slots[second.slot].binding.objectEventId = native2.objectEventId;
    assert(AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &stale));
    assert(stale.handle.slot == receipt.handle.slot);
    assert(stale.handle.generation == receipt.handle.generation);
    assert(AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt));
    assert(AtlasActorRuntime_CommitRestoredSprite(&receipt, 7));
    assert(!AtlasActorRuntime_CommitRestoredSprite(&receipt2, 7));
    /* A conflicting fresh sprite claim must not retire the other actor.
     * The field caller may clean up its independently verified new sprite. */
    assert(AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt2));
    stale = receipt2;
    stale.handle.generation++;
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&stale));
    assert(AtlasActorRuntime_CommitRestoredSprite(&receipt2, 8));
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt2));
    AtlasActorRuntime_EndSpriteRestore();
    assert(AtlasActorRuntime_CaptureBound(3, 7, 1, 5, 7, &receipt));
    stale = receipt;
    stale.handle.generation++;
    assert(!AtlasActorRuntime_ConfirmRemoval(&stale));
    assert(AtlasActorRuntime_ConfirmRemoval(&receipt));
    assert(!AtlasActorRuntime_ConfirmRemoval(&receipt));
    assert(AtlasActorAdmission_ResolveBound(roster, second, &resolved));
    assert(resolved.objectEventId == 6 && resolved.spriteId == 8);
    assert(AtlasActorAdmission_Retire(roster, second));

    /* Engine has separately deactivated an Atlas object after CreateSprite
     * failed. Pure roster abort requires the exact restoring generation. */
    assert(AtlasActorAdmission_Reserve(roster, &key, 1, 3, 3, 0, 0, &current));
    assert(AtlasActorAdmission_CommitBound(roster, current, &native));
    assert(AtlasActorRuntime_BeginSpriteRestore());
    assert(AtlasActorRuntime_CaptureForSpriteRestore(3, 1, 5, 7, &receipt));
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&stale));
    assert(AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt));
    assert(!AtlasActorRuntime_AbandonFailedSpriteRestore(&stale));
    assert(AtlasActorRuntime_AbandonFailedSpriteRestore(&receipt));
    assert(!AtlasActorRuntime_CanAbandonFailedSpriteRestore(&receipt));
    assert(!AtlasActorRuntime_AbandonFailedSpriteRestore(&receipt));
    assert(!AtlasActorAdmission_ResolveBound(roster, current, &resolved));
    AtlasActorRuntime_EndSpriteRestore();
    assert(!AtlasActorRuntime_AbandonFailedSpriteRestore(&receipt));

    for (i = 0; i < 10000; i++)
    {
        assert(AtlasActorAdmission_Reserve(roster, &key, 1, 3, 3, 0, 0, &current));
        AtlasActorRuntime_ResetAfterNativeClear();
        assert(!AtlasActorAdmission_CommitBound(roster, current, &native));
        assert(!AtlasActorAdmission_ResolveBound(roster, current, &resolved));
        assert(roster == AtlasActorRuntime_GetRoster());
    }
    puts("PASS: unique native removal ownership, sprite restore, 10000 reset generations");
    return 0;
}
