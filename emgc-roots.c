// Functions for supporting GC roots and leaves.
#include <stdlib.h>

static void **roots;
static uint32_t num_roots, roots_mask;

static uint32_t hash_root(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & roots_mask; }

static void insert_root(void *ptr)
{
  uint32_t i = hash_root(ptr);
  while((uintptr_t)roots[i] > 1) i = (i+1) & roots_mask;
  if ((uintptr_t)roots[i] != 1) ++num_roots;
  roots[i] = ptr;
}

void gc_make_root(void *ptr)
{
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
}

void gc_unmake_root(void *ptr)
{
  if (!roots) return;
  for(uint32_t i = hash_root(ptr); roots[i]; i = (i+1) & roots_mask)
    if (roots[i] == ptr)
    {
      roots[i] = (void*)1;
      return;
    }
}

void *gc_malloc_root(size_t bytes)
{
  void *ptr = gc_malloc(bytes);
  gc_make_root(ptr);
  return ptr;
}

void gc_make_leaf(void *ptr)
{
  uint32_t i = find_index(ptr);
  if (i == INVALID_INDEX) return;
  table[i] = (void*)((uintptr_t)table[i] | PTR_LEAF_BIT);
}

void gc_unmake_leaf(void *ptr)
{
  uint32_t i = find_index(ptr);
  if (i == INVALID_INDEX) return;
  table[i] = (void*)((uintptr_t)table[i] & ~PTR_LEAF_BIT);
}

void *gc_malloc_leaf(size_t bytes)
{
  void *ptr = gc_malloc(bytes);
  gc_make_leaf(ptr);
  return ptr;
}
