#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <emscripten/stack.h>
#include <emscripten/heap.h>
#include <emscripten/eventloop.h>
#include "emgc.h"
#ifdef __wasm_simd128__
#include <wasm_simd128.h>
#endif

// Pass this define to not scan the global memory during GC. If you register all global managed
// variables yourself, skipping automatic marking can improve performance.
// #define EMGC_SKIP_AUTOMATIC_STATIC_MARKING

#define IS_ALIGNED(ptr, size) (((uintptr_t)(ptr) & ((size)-1)) == 0)
#define BITVEC_GET(arr, i)  (((arr)[(i)>>3] &    1<<((i)&7)) != 0)
#define BITVEC_SET(arr, i)   ((arr)[(i)>>3] |=   1<<((i)&7))
#define BITVEC_CLEAR(arr, i) ((arr)[(i)>>3] &= ~(1<<((i)&7)))
#define REMOVE_FLAG_BITS(ptr) ((void*)((uintptr_t)(ptr) & ~(uintptr_t)7))
#define INVALID_INDEX ((uint32_t)-1)
#define SENTINEL_PTR ((void*)31)
#define PTR_FINALIZER_BIT ((uintptr_t)1)
#define PTR_LEAF_BIT ((uintptr_t)2)
#define HAS_FINALIZER_BIT(ptr) (((uintptr_t)(ptr) & PTR_FINALIZER_BIT))
#define HAS_LEAF_BIT(ptr) (((uintptr_t)(ptr) & PTR_LEAF_BIT))

size_t malloc_usable_size(void*);
void * __attribute__((weak, __visibility__("default"))) emmalloc_realloc_zeroed(void *ptr, size_t size) { free(ptr); return calloc(size, 1); }

extern char __global_base, __data_end, __heap_base;

static void **table;
static uint8_t *mark_table, *used_table;
static uint32_t num_allocs, num_table_entries, table_mask;

static uint32_t hash_ptr(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & table_mask; }

static int gc_looks_like_ptr(uintptr_t val)
{
  return (IS_ALIGNED(val, 8) && val - (uintptr_t)&__heap_base < (uintptr_t)emscripten_get_heap_size() - (uintptr_t)&__heap_base);
}

static uint32_t table_find(void *ptr)
{
  for(uint32_t i = hash_ptr(ptr); table[i]; i = (i+1) & table_mask)
    if (REMOVE_FLAG_BITS(table[i]) == ptr) return i;
  return INVALID_INDEX;
}

static void table_insert(void *ptr)
{
  uint32_t i = hash_ptr(ptr);
  uint32_t i64 = i>>6;
  uint64_t *used64 = (uint64_t*)used_table;
  uint64_t u = used64[i64] | ((1ull<<(i&63))-1); // Mask off all indices that come before the initial hash index
  while(u == (uint64_t)-1)
    u = used64[(i64 = (i64+1) & (table_mask >> 6))];
  i = (i64<<6) + __builtin_ctzll(~u);
  if (!table[i]) ++num_table_entries;
  table[i] = ptr;
  BITVEC_SET(used_table, i);
  ++num_allocs;
}

static void table_free(uint32_t i)
{
  assert(table[i] > SENTINEL_PTR);
  free(REMOVE_FLAG_BITS(table[i]));
  BITVEC_CLEAR(used_table, i);
  --num_allocs;
  if (table[(i+1)&table_mask]) table[i] = SENTINEL_PTR;
  else // Opportunistically clear sentinels if they aren't needed in these hash slots.
    for(;;)
    {
      --num_table_entries;
      table[i] = 0;
      if (table[i = (i+table_mask) & table_mask] != SENTINEL_PTR) break;
    }
}

static void realloc_table()
{
  uint32_t old_mask = table_mask;
  if (2*num_allocs >= table_mask) table_mask <<= 1;
  else if (((8*num_allocs)|127) < table_mask) table_mask = (1 << (32-__builtin_clz(2*num_allocs))) - 1;
  table_mask |= 127;

  if (old_mask != table_mask) mark_table = (uint8_t*)emmalloc_realloc_zeroed(mark_table, (table_mask+1)>>3);

  uint64_t *old_used_table = (uint64_t *)used_table;
  void **old_table = table;

  used_table = (uint8_t*)calloc(((table_mask+1)>>3) + (table_mask+1)*sizeof(void*), 1);
  table = (void**)(used_table + ((table_mask+1)>>3));
  num_table_entries = num_allocs = 0;
  assert(mark_table && used_table);

  if (old_table)
    for(uint32_t i = 0, offset; i <= old_mask; i += 64)
      for(uint64_t bits = old_used_table[i>>6]; bits; bits ^= (1ull<<offset))
        table_insert(old_table[i + (offset = __builtin_ctzll(bits))]);

  free(old_used_table);
}

#include "emgc-finalizer.c"
#include "emgc-multithreaded.c"
#include "emgc-sleep.c"

void *gc_malloc(size_t bytes)
{
  ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED();
  void *ptr = malloc(bytes);
  if (!ptr) return 0;
  GC_MALLOC_ACQUIRE();
  if (2*num_table_entries >= table_mask) realloc_table();
  table_insert(ptr);
  GC_MALLOC_RELEASE();
  return ptr;
}

void gc_free(void *ptr)
{
  if (!ptr) return;
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ptr);
  if (i != INVALID_INDEX)
  {
    table_free(i);
    gc_unmake_root(ptr);
  }
  GC_MALLOC_RELEASE();
}

#include "emgc-weak.c"
#include "emgc-roots.c"
#include "emgc-mark.c"

static void sweep()
{
  ASSERT_GC_MALLOC_IS_ACQUIRED();

  // If we didn't mark all finalizers, we know we will have GC object with
  // finalizer to sweep. If so, find a finalizer to run.
  if (num_finalizers_marked < num_finalizers) find_and_run_a_finalizer();
  else // No finalizers to invoke, so perform a real sweep that frees up GC objects.
  {
#ifdef __wasm_simd128__
    for(uint32_t i = 0, offset; i <= table_mask; i += 128)
    {
      v128_t g = wasm_v128_and(wasm_v128_load(used_table + (i>>3)), wasm_v128_not(wasm_v128_load(mark_table + (i>>3))));
      if (wasm_v128_any_true(g))
      {
        for(uint64_t lo = wasm_u64x2_extract_lane(g, 0); lo; lo ^= (1ull<<offset)) table_free(i + (offset = __builtin_ctzll(lo)));
        for(uint64_t hi = wasm_u64x2_extract_lane(g, 1); hi; hi ^= (1ull<<offset)) table_free(i + 64 + (offset = __builtin_ctzll(hi)));
      }
    }
#else
    for(uint32_t i = 0, offset; i <= table_mask; i += 64)
      for(uint64_t b = ((uint64_t*)used_table)[i>>6] & ~((uint64_t*)mark_table)[i>>6]; b; b ^= (1ull<<offset))
        table_free(i + (offset = __builtin_ctzll(b)));
#endif
  }

  // Compactify managed allocation array if it is now overly large to fit all allocations.
  // Or if the size doesn't change, then since we still hold the gc_malloc lock, this
  // is a good moment to clear the mark table back to zero for the next allocation
  // (which helps avoid a tricky double synchronization at start_multithreaded_collection())
  if (((8*num_allocs)|127) < table_mask) realloc_table();
  else memset(mark_table, 0, (table_mask+1)>>3);

  GC_MALLOC_RELEASE();
}

static void mark_current_thread_stack()
{
  uintptr_t stack_bottom = emscripten_stack_get_current();

#if defined(__EMSCRIPTEN_SHARED_MEMORY__) || defined(EMGC_FENCED)
  if (this_thread_accessing_managed_state)
    mark((void*)stack_bottom, stack_top - stack_bottom);
#else
  mark((void*)stack_bottom, emscripten_stack_get_base() - stack_bottom);
#endif
}

void gc_collect()
{
  if (!num_allocs) return;

  GC_MALLOC_ACQUIRE(); // Acquire GC lock so that we know that the sweep worker has finished.
  GC_MALLOC_RELEASE(); // But release it immediately, since other threads may still sneak in a gc malloc before realizing they need to participate to collection.

  num_finalizers_marked = 0;

  start_multithreaded_collection();

#ifndef EMGC_SKIP_AUTOMATIC_STATIC_MARKING
  mark(&__global_base, (uintptr_t)&__data_end - (uintptr_t)&__global_base);
#endif

  mark_current_thread_stack();
  mark_orphaned_stacks();

  if (roots) mark((void*)roots, (roots_mask+1)*sizeof(void*));

#if defined(__EMSCRIPTEN_SHARED_MEMORY__)
  finish_multithreaded_marking(); // In mt builds, delegate sweeping (and the active gc lock) to a sweep worker.
#else
  sweep(); // In st builds, complete sweeping here.
#endif
}

static void collect_when_stack_is_empty(void *unused) { gc_collect(); } // We know 100% we won't have any managed pointers on the stack frame now.
void gc_collect_when_stack_is_empty() { emscripten_set_timeout(collect_when_stack_is_empty, 0, 0); }

int gc_is_ptr(void *ptr)
{
  if (!gc_looks_like_ptr((uintptr_t)ptr)) return 0;
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ptr);
  GC_MALLOC_RELEASE();
  return i != INVALID_INDEX;
}

#include "emgc-ptr_base.c"
#include "emgc-debug.c"
