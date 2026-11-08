#ifndef ATLAS_MOCK_MALLOC_H
#define ATLAS_MOCK_MALLOC_H
#include <stdlib.h>
static inline void *AllocZeroed(unsigned size) { return calloc(1, size); }
static inline void Free(void *p) { free(p); }
#endif
