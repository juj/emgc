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
  if (gc_is_weak_ptr(strong_ptr)) return strong_ptr; // Already a weak ptr?

  // Allocate a reference block to hold the masked pointer
  void **ref_block = (void**)gc_malloc(sizeof(void*));
  if (!ref_block) return 0;

  // Store the bit-reversed pointer (masked to hide from GC scanner)
  *ref_block = (void*)~(uintptr_t)strong_ptr;

  // Mark the reference block as a weak pointer and as a leaf (don't scan contents)
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(ref_block);
  if (i != INVALID_INDEX) table[i] = (void*)((uintptr_t)table[i] | PTR_WEAK_BIT | PTR_LEAF_BIT);
  GC_MALLOC_RELEASE();

  return ref_block;
}

void *gc_acquire_strong_ptr(void *weak_or_strong_ptr)
{
  if (!weak_or_strong_ptr) return 0;
  if (gc_is_strong_ptr(weak_or_strong_ptr)) return weak_or_strong_ptr;

  // It's a weak pointer, dereference the reference block
  void **ref_block = (void**)weak_or_strong_ptr;
  void *strong_ptr = (void*)~(uintptr_t)(*ref_block); // Unmask
  assert(strong_ptr != 0); // We should never have a null strong ptr in the first place.

  // Check if the strong pointer still exists in the table
  GC_MALLOC_ACQUIRE();
  uint32_t i = table_find(strong_ptr);
  GC_MALLOC_RELEASE();
  return (i == INVALID_INDEX) ? 0 : strong_ptr;
}

int gc_weak_ptr_equals(void *weak_or_strong_ptr1, void *weak_or_strong_ptr2)
{
  if (weak_or_strong_ptr1 == weak_or_strong_ptr2) return 1;
  return gc_acquire_strong_ptr(weak_or_strong_ptr1) == gc_acquire_strong_ptr(weak_or_strong_ptr2);
}
