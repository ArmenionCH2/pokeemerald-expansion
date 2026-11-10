#ifndef MOCK_GLOBAL
#define MOCK_GLOBAL
#include <stdint.h>
#include <string.h>
typedef int16_t s16;
typedef int64_t s64;
typedef uint64_t u64;
typedef int32_t s32;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint8_t bool8;
#define EWRAM_DATA
#define TRUE 1
#define FALSE 0
#define PACK_METATILE(n) ((n)&0x3FF)
#define PACK_COLLISION(n) (((n)&3)<<10)
#define PACK_ELEVATION(n) (((n)&15)<<12)
struct MapHeader;
#endif
