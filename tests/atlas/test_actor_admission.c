#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "atlas_actor_admission.h"
#include "constants/event_objects.h"

static struct AtlasActorSpawnKey Key(u32 n)
{
    struct AtlasActorSpawnKey key;
    assert(AtlasActor_MakeSpawnKey(&key, 0x1234, (s32)n, -20, 25, 3));
    return key;
}

int main(void)
{
    struct AtlasActorAdmissionRoster roster = {0};
    struct AtlasActorSpawnKey a = Key(1), b = Key(2), c = Key(3), copy;
    struct AtlasActorHandle h1, h2, h3, invalid;
    u8 local;
    unsigned i;
    AtlasActorAdmission_Reset(&roster);
    /* An untrusted birthplace key can bypass MakeSpawnKey and arrive from
     * climate/streaming callers. Polar latitude is never cyclic. Rejection
     * must preserve every empty slot and return an invalid handle. */
    {
        const s32 invalidLatitudes[] = {
            (-2147483647 - 1), -ATLAS_WORLD_HALF_HEIGHT - 1,
            ATLAS_WORLD_HALF_HEIGHT, 2147483647
        };
        const s32 validLatitudes[] = {
            -ATLAS_WORLD_HALF_HEIGHT, ATLAS_WORLD_HALF_HEIGHT - 1
        };
        struct AtlasActorSpawnKey polar = a;
        unsigned j;
        for (j = 0; j < sizeof(invalidLatitudes) / sizeof(invalidLatitudes[0]); j++)
        {
            polar.worldY = invalidLatitudes[j];
            assert(!AtlasActorAdmission_Reserve(&roster, &polar, 1,
                                                3, 3, 0, 0, &invalid));
            assert(invalid.slot == ATLAS_ACTOR_ROSTER_CAPACITY);
            assert(invalid.generation == 0);
            assert(roster.slots[0].state == ATLAS_ACTOR_EMPTY);
            assert(roster.slots[0].generation == 1);
        }
        for (j = 0; j < sizeof(validLatitudes) / sizeof(validLatitudes[0]); j++)
        {
            polar.worldY = validLatitudes[j];
            assert(AtlasActorAdmission_Reserve(&roster, &polar, 1,
                                               3, 3, 0, 0, &invalid));
            assert(AtlasActorAdmission_Abort(&roster, invalid));
        }
    }
    /* Exhaustively pin all 256 u8 local IDs: 1..126 only. Each accepted
     * reservation is aborted to ensure the next trial starts vacant. */
    for (i = 0; i < 256; i++)
    {
        bool8 accepted = AtlasActorAdmission_Reserve(&roster, &a, (u8)i,
                                                     3, 3, 1, 1, &invalid);
        assert(accepted == (i > 0 && i < LOCALID_CAMERA));
        if (accepted)
            assert(AtlasActorAdmission_Abort(&roster, invalid));
        else
            assert(invalid.slot == ATLAS_ACTOR_ROSTER_CAPACITY
                && invalid.generation == 0);
    }
    /* Emerald reserves 127 for camera and 236..240/255 for link/player.
     * Atlas intentionally excludes the entire 127..255 special-ID range. */
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 127, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 128, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 235, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 236, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 240, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 254, 3, 3, 1, 1, &invalid));
    assert(invalid.slot == ATLAS_ACTOR_ROSTER_CAPACITY && invalid.generation == 0);
    assert(AtlasActorAdmission_Reserve(&roster, &a, 126, 3, 3, 1, 1, &h1));
    assert(AtlasActorAdmission_Abort(&roster, h1));
    assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 1, 1, &h1));
    assert(!AtlasActorAdmission_Reserve(&roster, &a, 2, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &b, 1, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &b, 0, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Reserve(&roster, &b, 255, 3, 3, 1, 1, &invalid));
    assert(!AtlasActorAdmission_Resolve(&roster, h1, &copy, &local));
    assert(AtlasActorAdmission_Reserve(&roster, &b, 2, 3, 3, 1, 1, &h2));
    assert(!AtlasActorAdmission_Reserve(&roster, &c, 3, 3, 3, 1, 1, &invalid));
    assert(invalid.generation == 0);
    assert(AtlasActorAdmission_Abort(&roster, h2));
    assert(!AtlasActorAdmission_Commit(&roster, h2));
    assert(AtlasActorAdmission_Commit(&roster, h1));
    assert(!AtlasActorAdmission_Commit(&roster, h1));
    assert(AtlasActorAdmission_Resolve(&roster, h1, &copy, &local));
    assert(local == 1 && AtlasActor_SpawnKeyEquals(&a, &copy));
    assert(!AtlasActorAdmission_Abort(&roster, h1));
    assert(AtlasActorAdmission_Reserve(&roster, &b, 2, 2, 2, 1, 1, &h2));
    assert(h2.generation != 0);
    assert(AtlasActorAdmission_Commit(&roster, h2));
    assert(AtlasActorAdmission_Reserve(&roster, &c, 3, 2, 2, 1, 1, &h3));
    assert(AtlasActorAdmission_Commit(&roster, h3));
    assert(!AtlasActorAdmission_Reserve(&roster, &c, 4, 10, 10, 0, 0, &invalid));
    assert(AtlasActorAdmission_Retire(&roster, h1));
    assert(!AtlasActorAdmission_Resolve(&roster, h1, &copy, &local));
    assert(!AtlasActorAdmission_Retire(&roster, h1));
    AtlasActorAdmission_Reset(&roster);
    assert(!AtlasActorAdmission_Resolve(&roster, h2, &copy, &local));
    assert(!AtlasActorAdmission_Resolve(&roster, h3, &copy, &local));
    for (i = 0; i < 50000; i++)
    {
        struct AtlasActorHandle h, old;
        struct AtlasActorSpawnKey k = Key(i + 10);
        assert(AtlasActorAdmission_Reserve(&roster, &k, 1, 1, 1, 0, 0, &h));
        old = h;
        if (i & 1)
        {
            assert(AtlasActorAdmission_Commit(&roster, h));
            assert(AtlasActorAdmission_Retire(&roster, h));
        }
        else
            assert(AtlasActorAdmission_Abort(&roster, h));
        assert(!AtlasActorAdmission_Commit(&roster, old));
        assert(!AtlasActorAdmission_Resolve(&roster, old, NULL, NULL));
    }
    /* Stage 2B4: bound live ownership is distinct from an unbound legacy
     * Commit. Failed admission must leave the pending reservation intact. */
    {
        struct AtlasActorNativeBinding first = {3, 4, 5, 7};
        struct AtlasActorNativeBinding second = {5, 6, 5, 7};
        struct AtlasActorNativeBinding read = {0};
        struct AtlasActorNativeBinding bad;
        unsigned n;
        AtlasActorAdmission_Reset(&roster);
        assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 0, 0, &h1));
        assert(!AtlasActorAdmission_CommitBound(&roster, h1, NULL));
        for (n = 0; n < 256; n++)
        {
            bad = first;
            bad.objectEventId = (u8)n;
            if (n >= ATLAS_ACTOR_OBJECT_EVENT_LIMIT)
                assert(!AtlasActorAdmission_CommitBound(&roster, h1, &bad));
        }
        for (n = ATLAS_ACTOR_SPRITE_LIMIT; n < 256; n++)
        {
            bad = first;
            bad.spriteId = (u8)n;
            assert(!AtlasActorAdmission_CommitBound(&roster, h1, &bad));
        }
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &read));
        assert(AtlasActorAdmission_CommitBound(&roster, h1, &first));
        assert(AtlasActorAdmission_ResolveBound(&roster, h1, &read));
        assert(read.objectEventId == 3 && read.spriteId == 4
            && read.mapNum == 5 && read.mapGroup == 7);
        assert(!AtlasActorAdmission_CommitBound(&roster, h1, &first));
        assert(AtlasActorAdmission_Reserve(&roster, &b, 2, 3, 3, 0, 0, &h2));
        bad = second;
        bad.objectEventId = first.objectEventId;
        assert(!AtlasActorAdmission_CommitBound(&roster, h2, &bad));
        bad = second;
        bad.spriteId = first.spriteId;
        assert(!AtlasActorAdmission_CommitBound(&roster, h2, &bad));
        assert(AtlasActorAdmission_CommitBound(&roster, h2, &second));
        assert(AtlasActorAdmission_Reserve(&roster, &c, 3, 3, 3, 0, 0, &h3));
        assert(AtlasActorAdmission_Commit(&roster, h3));
        assert(!AtlasActorAdmission_ResolveBound(&roster, h3, &read));
        assert(AtlasActorAdmission_Retire(&roster, h1));
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &read));
        assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 0, 0, &invalid));
        assert(invalid.generation != h1.generation);
        assert(AtlasActorAdmission_CommitBound(&roster, invalid, &first));
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &read));
        assert(AtlasActorAdmission_ResolveBound(&roster, invalid, &read));
        AtlasActorAdmission_Reset(&roster);
        assert(!AtlasActorAdmission_ResolveBound(&roster, invalid, &read));
        assert(!AtlasActorAdmission_ResolveBound(&roster, h2, &read));
        /* Exhaustive 16 x 64 native slot-pair boundary matrix. */
        for (n = 0; n < ATLAS_ACTOR_OBJECT_EVENT_LIMIT * ATLAS_ACTOR_SPRITE_LIMIT; n++)
        {
            first.objectEventId = n / ATLAS_ACTOR_SPRITE_LIMIT;
            first.spriteId = n % ATLAS_ACTOR_SPRITE_LIMIT;
            assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 1, 1, 0, 0, &h1));
            assert(AtlasActorAdmission_CommitBound(&roster, h1, &first));
            assert(AtlasActorAdmission_ResolveBound(&roster, h1, &read));
            assert(read.objectEventId == first.objectEventId
                && read.spriteId == first.spriteId);
            assert(AtlasActorAdmission_Retire(&roster, h1));
            assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &read));
        }
    }
    /* Stage 2B8B1: failed bound lookups must never leak a previously
     * successful native receipt into a later camera-culling decision. */
    {
        struct AtlasActorNativeBinding stale = {3, 4, 5, 7};
        struct AtlasActorNativeBinding own;
        AtlasActorAdmission_Reset(&roster);
        assert(!AtlasActorAdmission_ResolveBound(NULL, h1, &stale));
        assert(stale.objectEventId == ATLAS_ACTOR_OBJECT_EVENT_LIMIT
            && stale.spriteId == ATLAS_ACTOR_SPRITE_LIMIT
            && stale.mapNum == 0 && stale.mapGroup == 0);
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, NULL));
        assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 0, 0, &h1));
        stale.objectEventId = 3; stale.spriteId = 4;
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &stale));
        assert(stale.objectEventId == ATLAS_ACTOR_OBJECT_EVENT_LIMIT
            && stale.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
        assert(AtlasActorAdmission_Commit(&roster, h1));
        stale.objectEventId = 3; stale.spriteId = 4;
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &stale));
        assert(stale.objectEventId == ATLAS_ACTOR_OBJECT_EVENT_LIMIT
            && stale.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
        assert(AtlasActorAdmission_Retire(&roster, h1));
        assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 0, 0, &h2));
        own.objectEventId = 3; own.spriteId = 4;
        own.mapNum = 5; own.mapGroup = 7;
        assert(AtlasActorAdmission_CommitBound(&roster, h2, &own));
        stale.objectEventId = 3; stale.spriteId = 4;
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1, &stale));
        assert(stale.objectEventId == ATLAS_ACTOR_OBJECT_EVENT_LIMIT
            && stale.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
        /* Reject internal output aliases without damaging live ownership. */
        assert(!AtlasActorAdmission_ResolveBound(&roster, h1,
            &roster.slots[h2.slot].binding));
        assert(roster.slots[h2.slot].binding.objectEventId == 3
            && roster.slots[h2.slot].binding.spriteId == 4);
        assert(AtlasActorAdmission_ResolveBound(&roster, h2, &stale));
        assert(stale.objectEventId == 3 && stale.spriteId == 4);
        AtlasActorAdmission_Reset(&roster);
        assert(!AtlasActorAdmission_ResolveBound(&roster, h2, &stale));
        assert(stale.objectEventId == ATLAS_ACTOR_OBJECT_EVENT_LIMIT
            && stale.spriteId == ATLAS_ACTOR_SPRITE_LIMIT);
    }
    /* Stage 2B8B2A: a native culling notification is safe only with the
     * exact actor generation AND a pre-removal object/sprite/map receipt.
     * This does not exercise Emerald's actual culling callback. */
    {
        struct AtlasActorNativeBinding receipt = {3, 4, 5, 7};
        struct AtlasActorNativeBinding other = {8, 9, 5, 7};
        struct AtlasActorNativeBinding bad;
        struct AtlasActorHandle stale;
        unsigned field;
        AtlasActorAdmission_Reset(&roster);
        assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 0, 0, &h1));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &receipt, 1));
        assert(AtlasActorAdmission_CommitBound(&roster, h1, &receipt));
        assert(AtlasActorAdmission_Reserve(&roster, &b, 2, 3, 3, 0, 0, &h2));
        assert(AtlasActorAdmission_CommitBound(&roster, h2, &other));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, NULL, 1));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1,
            &roster.slots[h1.slot].binding, 1));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1,
            &roster.slots[h2.slot].binding, 1));
        for (field = 0; field < 4; field++)
        {
            bad = receipt;
            if (field == 0) bad.objectEventId++;
            if (field == 1) bad.spriteId++;
            if (field == 2) bad.mapNum++;
            if (field == 3) bad.mapGroup++;
            assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &bad, 1));
        }
        bad = receipt; bad.objectEventId = ATLAS_ACTOR_OBJECT_EVENT_LIMIT;
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &bad, 1));
        bad = receipt; bad.spriteId = ATLAS_ACTOR_SPRITE_LIMIT;
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &bad, 1));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &receipt, 2));
        assert(AtlasActorAdmission_ResolveBound(&roster, h1, &bad));
        stale = h1;
        assert(AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &receipt, 1));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, stale, &receipt, 1));
        assert(AtlasActorAdmission_ResolveBound(&roster, h2, &bad));
        assert(bad.objectEventId == other.objectEventId);
        /* Reuse identical native slots and local/map identity: stale
         * generation MUST NOT retire the newly admitted actor. */
        assert(AtlasActorAdmission_Reserve(&roster, &a, 1, 3, 3, 0, 0, &h1));
        assert(h1.generation != stale.generation);
        assert(AtlasActorAdmission_CommitBound(&roster, h1, &receipt));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, stale, &receipt, 1));
        assert(AtlasActorAdmission_ResolveBound(&roster, h1, &bad));
        assert(AtlasActorAdmission_ConfirmNativeRemoval(&roster, h1, &receipt, 1));
        assert(AtlasActorAdmission_ConfirmNativeRemoval(&roster, h2, &other, 2));
        assert(!AtlasActorAdmission_ConfirmNativeRemoval(&roster, h2, &other, 2));
    }
    puts("PASS: Stage 2B4 native slot-bound generation and collision contract");
    puts("PASS: native actor admission 50,000 rollback/retire cycles and ownership boundaries");
    return 0;
}
