#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *gc_malloc(size_t bytes);
void gc_free(void *ptr);
void gc_collect(void);
void gc_collect_when_stack_is_empty(void);
uint32_t gc_num_ptrs(void);

void gc_make_root(void *ptr);
void gc_unmake_root(void *ptr);

void gc_dump(void);

#ifdef __cplusplus
}
#endif
