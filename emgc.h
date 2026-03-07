#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *gc_malloc(size_t bytes); // Allocates memory with unspecified (dirty) initial contents.
void *gc_calloc(size_t bytes); // Allocates zero-initialized memory.

// Manually frees an allocation. If the allocation had a finalizer, it is *not* called.
void gc_free(void *ptr);

void *gc_malloc_root(size_t bytes);
void *gc_calloc_root(size_t bytes);
void gc_make_root(void *ptr __attribute__((nonnull)));
void gc_unmake_root(void *ptr __attribute__((nonnull)));

void *gc_malloc_leaf(size_t bytes);
void *gc_calloc_leaf(size_t bytes);
void gc_make_leaf(void *ptr __attribute__((nonnull)));
void gc_unmake_leaf(void *ptr __attribute__((nonnull)));

void gc_collect(void);
void gc_collect_when_stack_is_empty(void);

typedef void (*gc_finalizer)(void *ptr);
void gc_register_finalizer(void *ptr __attribute__((nonnull)), gc_finalizer finalizer);
gc_finalizer gc_get_finalizer(void *ptr __attribute__((nonnull)));
void gc_remove_finalizer(void *ptr __attribute__((nonnull)));

void *gc_get_weak_ptr(void *strong_ptr);
// Given a weak pointer, acquire the referenced strong pointer.
// Slightly unintuitively, this function takes a pointer to a weak pointer.
// This is to allow nulling out the original weak pointer, once the strong pointer
// has been garbage collected.
// Usage:
//    void *strong_ptr = gc_malloc(1024);
//    void *weak_ptr = gc_get_weak_ptr(strong_ptr);
//    void *strong_ptr_again = gc_acquire_strong_ptr(&weak_ptr);
//    -> if strong_ptr_again is zero (GC freed the allocation), then weak_ptr will reset to zero as well.
void *gc_acquire_strong_ptr(void **weak_ptr_ptr __attribute__((nonnull)));

int gc_is_ptr(void *weak_or_strong_ptr);
int gc_is_weak_ptr(void *weak_or_strong_ptr);
int gc_is_strong_ptr(void *weak_or_strong_ptr);
int gc_is_root(void *strong_ptr); // Must be called with a strong pointer.

void *gc_ptr_base(void *interior_ptr); // Given a pointer to the interior of an object, returns the base address of the allocation.

typedef void *(*gc_mutator_func)(void *user1, void *user2);
void *gc_enter_fence_cb(gc_mutator_func mutator_callback, void *user1, void *user2);

void gc_temporarily_leave_fence(void);
void gc_return_to_fence(void);

void gc_sleep(double nsecs);
// Wait functions return: 0: ok, 1: not-equal, 2: timed-out
int gc_wait32(void *addr __attribute__((nonnull)), uint32_t expected, int64_t nsecs);
int gc_wait64(void *addr __attribute__((nonnull)), uint64_t expected, int64_t nsecs);

void gc_participate_to_garbage_collection(void);

uint32_t gc_num_ptrs(void);
void gc_dump(void);

void gc_loge(const char *format, ...);
void gc_log(const char *format, ...);

// Internal debug functions. Only tests are allowed to access these:
int debug_gc_num_roots_slots_populated(void);

#ifdef __cplusplus
}
#endif
