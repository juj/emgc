int gc_is_weak_ptr(void *ptr)
{
  if (!ptr) return 0;
  if (!gc_looks_like_ptr((uintptr_t)ptr)) return 0;
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ptr);
  int is_weak = (i != INVALID_INDEX && HAS_WEAK_BIT(table[i]));
  GC_MALLOC_RELEASE();
  return is_weak;
}

int gc_is_strong_ptr(void *ptr)
{
  if (!ptr) return 0;
  if (!gc_looks_like_ptr((uintptr_t)ptr)) return 0;
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ptr);
  int is_strong = (i != INVALID_INDEX && !HAS_WEAK_BIT(table[i]));
  GC_MALLOC_RELEASE();
  return is_strong;
}

void *gc_get_weak_ptr(void *strong_ptr)
{
  if (!strong_ptr) return 0;
  assert(!gc_is_weak_ptr(strong_ptr));
 
  // Allocate a reference block to hold the masked pointer
  void **ref_block = (void**)gc_malloc(sizeof(void*));
  if (!ref_block) return 0;

  // Store the bit-reversed pointer (masked to hide from GC scanner)
  *ref_block = (void*)~(uintptr_t)strong_ptr;

  // Mark the reference block as a weak pointer and as a leaf (don't scan contents)
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ref_block);
  assert(i != INVALID_INDEX);
  table[i] = (void*)((uintptr_t)table[i] | PTR_WEAK_BIT | PTR_LEAF_BIT);
  GC_MALLOC_RELEASE();

  return ref_block;
}

void *gc_acquire_strong_ptr(void **weak_ptr_ptr __attribute__((nonnull)))
{
  assert(weak_ptr_ptr);
  void *weak_ptr = *weak_ptr_ptr;
  if (!weak_ptr) return 0;
  assert(gc_is_weak_ptr(weak_ptr));

  void **ref_block = (void**)weak_ptr; // We have a weak pointer, so dereference the reference block
  void *strong_ptr = (void*)~(uintptr_t)*ref_block; // Unmask the pointer from weak reference block -> strong allocation
  if (strong_ptr)
  {
    // Check if the strong pointer still exists in the table
    GC_MALLOC_ACQUIRE();
    uint32_t i = table_find(strong_ptr);
    GC_MALLOC_RELEASE();
    if (i != INVALID_INDEX) return strong_ptr;
    // The strong allocation has been freed, so as micro-optimization, clear the weak ptr reference block.
    // This way later gc_acquire_strong_ptr() queries will short-circuit faster, without needing to GC_MALLOC_ACQUIRE/_RELEASE().
    *ref_block = (void*)~(uintptr_t)0;
  }
  // The strong allocation has been freed, so mutate the caller's weak pointer to a null pointer, so that the reference block
  // will have the chance to eventually garbage collect as well.
  *weak_ptr_ptr = 0;
  return 0;
}

int gc_weak_ptr_equals(void *weak_or_strong_ptr1, void *weak_or_strong_ptr2)
{
  if (weak_or_strong_ptr1 == weak_or_strong_ptr2) return 1;
  return gc_acquire_strong_ptr(weak_or_strong_ptr1) == gc_acquire_strong_ptr(weak_or_strong_ptr2);
}
