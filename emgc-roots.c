// Functions for supporting GC roots and leaves.
#include <stdlib.h>

static void **roots;
static uint32_t num_roots, roots_mask;
static emscripten_lock_t roots_lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;

static uint32_t hash_root(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & roots_mask; }

static void insert_root(void *ptr __attribute__((nonnull)))
{
  assert(ptr);
  uint32_t i = hash_root(ptr);
  while((uintptr_t)roots[i] > 1) i = (i+1) & roots_mask;
  if ((uintptr_t)roots[i] != 1) ++num_roots;
  roots[i] = ptr;
}

int gc_is_root(void *ptr)
{
  if (!ptr) return 0;
  assert(!gc_is_weak_ptr(ptr));
  gc_acquire_lock(&roots_lock);
  for(uint32_t i = hash_root(ptr); roots[i]; i = (i+1) & roots_mask)
    if (roots[i] == ptr) return 1;
  gc_release_lock(&roots_lock);
  return 0;
}

void gc_make_root(void *ptr __attribute__((nonnull)))
{
  assert(ptr);
  gc_acquire_lock(&roots_lock);
  uint32_t old_mask = roots_mask;
  if (2*num_roots >= roots_mask)
  {
    roots_mask = (roots_mask << 1) | 1;

    void **old_roots = roots;
    roots = (void**)calloc(roots_mask+1, sizeof(void*));

    num_roots = 0;
    if (old_roots)
    {
      for(uint32_t i = 0; i <= old_mask; ++i)
        if ((uintptr_t)old_roots[i] > 1) insert_root(old_roots[i]);
      free(old_roots);
    }
  }
  insert_root(ptr);
  gc_release_lock(&roots_lock);
}

void gc_unmake_root(void *ptr __attribute__((nonnull)))
{
  if (!roots) return;
  assert(ptr);
  gc_acquire_lock(&roots_lock);
  for(uint32_t i = hash_root(ptr); roots[i]; i = (i+1) & roots_mask)
    if (roots[i] == ptr)
    {
      roots[i] = (void*)1;
      break;
    }
  gc_release_lock(&roots_lock);
}

void *gc_malloc_root(size_t bytes)
{
  void *ptr = gc_malloc(bytes);
  if (ptr) gc_make_root(ptr);
  return ptr;
}

void gc_make_leaf(void *ptr __attribute__((nonnull)))
{
  assert(ptr);
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ptr);
  assert(i != INVALID_INDEX);
  table[i] = (void*)((uintptr_t)table[i] | PTR_LEAF_BIT);
  GC_MALLOC_RELEASE();
}

void gc_unmake_leaf(void *ptr __attribute__((nonnull)))
{
  assert(ptr);
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ptr);
  assert(i != INVALID_INDEX);
  table[i] = (void*)((uintptr_t)table[i] & ~PTR_LEAF_BIT);
  GC_MALLOC_RELEASE();
}

void *gc_malloc_leaf(size_t bytes)
{
  void *ptr = gc_malloc(bytes);
  if (ptr) gc_make_leaf(ptr);
  return ptr;
}
