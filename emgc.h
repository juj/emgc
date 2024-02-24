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

typedef void (*gc_finalizer)(void *ptr);
void gc_register_finalizer(void *ptr, gc_finalizer finalizer);

void *gc_get_weak_ptr(void *weak_or_strong_ptr);
void *gc_acquire_strong_ptr(void *weak_or_strong_ptr);

int gc_weak_ptr_equals(void *weak_or_strong_ptr1, void *weak_or_strong_ptr2);

int gc_is_ptr(void *weak_or_strong_ptr);
int gc_is_weak_ptr(void *weak_or_strong_ptr);
int gc_is_strong_ptr(void *weak_or_strong_ptr);

typedef void (*gc_finalizer)(void *ptr);
void gc_register_finalizer(void *ptr, gc_finalizer finalizer);

typedef void *(*gc_mutator_func)(void *user1, void *user2);
void *gc_enter_fence_cb(gc_mutator_func mutator_callback, void *user1, void *user2);

uint32_t gc_num_ptrs(void);
void gc_dump(void);
void gc_sleep(double msecs);

void gc_participate_to_garbage_collection(void);

#ifdef __cplusplus
}
#endif
