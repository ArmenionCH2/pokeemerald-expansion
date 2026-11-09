#ifndef ATLAS_TEST_MOCK_CONSTANTS_EVENT_OBJECTS_H
#define ATLAS_TEST_MOCK_CONSTANTS_EVENT_OBJECTS_H

/* Exactly the sprite ids in the pinned native Emerald event_objects.h.
 * The production ROM compiles against the real header; this prevents the
 * standalone host unit test from requiring generated map_event_ids.h. */
/* Native Emerald special local IDs, source-checked against include/constants/event_objects.h. */
#define OBJ_EVENT_MON (1u << 14)
#define LOCALID_CAMERA 127
#define LOCALID_BERRY_BLENDER_PLAYER_END 240
#define LOCALID_PLAYER 255
#define OBJ_EVENT_GFX_SKITTY 203
#define OBJ_EVENT_GFX_KECLEON 204
#define OBJ_EVENT_GFX_ZIGZAGOON_2 208
#define OBJ_EVENT_GFX_PIKACHU 209
#define OBJ_EVENT_GFX_AZUMARILL 210
#define OBJ_EVENT_GFX_WINGULL 211
#define OBJ_EVENT_GFX_AZURILL 214
#define OBJ_EVENT_GFX_POOCHYENA 220

#endif
