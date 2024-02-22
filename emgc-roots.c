#include "emgc.h"
#include <stdlib.h>

void **gc_roots;
uint32_t gc_num_roots, gc_roots_mask;

static uint32_t hash_root(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & gc_roots_mask; }

static void insert_root(void *ptr)
{
  uint32_t i = hash_root(ptr);
  while((uintptr_t)gc_roots[i] > 1) i = (i+1) & gc_roots_mask;
  if ((uintptr_t)gc_roots[i] != 1) ++gc_num_roots;
  gc_roots[i] = ptr;
}

void gc_make_root(void *ptr)
{
  uint32_t old_mask = gc_roots_mask;
  if (2*gc_num_roots >= gc_roots_mask)
  {
    gc_roots_mask = (gc_roots_mask << 1) | 1;

    void **old_roots = gc_roots;
    gc_roots = (void**)calloc(gc_roots_mask+1, sizeof(void*));

    gc_num_roots = 0;
    if (old_mask)
      for(uint32_t i = 0; i <= old_mask; ++i)
        if ((uintptr_t)old_roots[i] > 1) insert_root(old_roots[i]);
    free(old_roots);
  }
  insert_root(ptr);
}

void gc_unmake_root(void *ptr)
{
  if (!gc_roots) return;
  for(uint32_t i = hash_root(ptr); gc_roots[i]; i = (i+1) & gc_roots_mask)
    if (gc_roots[i] == ptr)
    {
      gc_roots[i] = (void*)1;
      return;
    }
}

void *gc_malloc_root(size_t bytes)
{
  void *ptr = gc_malloc(bytes);
  gc_make_root(ptr);
  return ptr;
}
