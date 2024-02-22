#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *gc_malloc(size_t bytes);
void gc_free(void *ptr);

void *gc_malloc_root(size_t bytes);
void gc_make_root(void *ptr);
void gc_unmake_root(void *ptr);

void *gc_malloc_leaf(size_t bytes);
void gc_make_leaf(void *ptr);
void gc_unmake_leaf(void *ptr);

void gc_collect(void);
void gc_collect_when_stack_is_empty(void);

void *gc_get_weak_ptr(void *weak_or_strong_ptr);
void *gc_acquire_strong_ptr(void *weak_or_strong_ptr);

int gc_weak_ptr_equals(void *weak_or_strong_ptr1, void *weak_or_strong_ptr2);

int gc_is_weak_ptr(void *weak_or_strong_ptr);
int gc_is_strong_ptr(void *weak_or_strong_ptr);

uint32_t gc_num_ptrs(void);
void gc_dump(void);

#define IS_ALIGNED(ptr, size) (((uintptr_t)(ptr) & ((size)-1)) == 0)

#ifdef __cplusplus
}
#endif
