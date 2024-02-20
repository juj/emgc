#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *gc_malloc(size_t bytes);
void gc_free(void *ptr);
void gc_collect(void);
void gc_dump(void);
uint32_t gc_num_ptrs(void);

#ifdef __cplusplus
}
#endif
