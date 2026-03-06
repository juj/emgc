typedef struct weak_ptr_map
{
  void *strong_ptr;
  void **weak_ptr;
} weak_ptr_map;

static weak_ptr_map *weak_ptrs;
static uint32_t num_weak_ptrs, num_weak_ptrs_entries, weak_ptrs_mask;

static uint32_t hash_to_weak_ptr_map(void *strong_ptr) { return (uint32_t)((uintptr_t)strong_ptr >> 3) & weak_ptrs_mask; }

static uint32_t find_weak_ptr_index(void *strong_ptr)
{
  if (weak_ptrs)
    for(uint32_t i = hash_to_weak_ptr_map(strong_ptr); weak_ptrs[i].strong_ptr; i = (i+1) & weak_ptrs_mask)
      if (weak_ptrs[i].strong_ptr == strong_ptr) return i;
  return INVALID_INDEX;
}

static void insert_weak_ptr(void *strong_ptr, void *weak_ptr)
{
  assert(strong_ptr);
  assert(weak_ptr);

  uint32_t i = hash_to_weak_ptr_map(strong_ptr);
  while((uintptr_t)weak_ptrs[i].strong_ptr > 1 && weak_ptrs[i].strong_ptr != strong_ptr)
    i = (i+1) & weak_ptrs_mask;

  if (!weak_ptrs[i].strong_ptr) ++num_weak_ptrs_entries;
  if (weak_ptrs[i].strong_ptr != strong_ptr)
  {
    ++num_weak_ptrs;
    weak_ptrs[i].strong_ptr = strong_ptr;
  }
  weak_ptrs[i].weak_ptr = weak_ptr;
}

static void remove_weak_ptr(void *strong_ptr)
{
  assert(strong_ptr);
  uint32_t i = find_weak_ptr_index(strong_ptr);
  if (i == INVALID_INDEX) return; // There was no strong->weak link to this allocation.
  assert(weak_ptrs[i].strong_ptr == strong_ptr);
  assert(weak_ptrs[i].weak_ptr != 0);
  gc_unmake_root(weak_ptrs[i].weak_ptr); // Unpin the weak reference block for garbage collection.
  *weak_ptrs[i].weak_ptr = 0;
  weak_ptrs[i].strong_ptr = (void*)1;
  weak_ptrs[i].weak_ptr = 0;
  --num_weak_ptrs;
}

int gc_is_weak_ptr(void *ptr)
{
  if (!ptr) return 0;
  if (!gc_looks_like_ptr((uintptr_t)ptr)) return 0;
  ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED();
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
  ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED();
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
 
  // See if there already exists a weak pointer reference block for this allocation.
  ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED();
  GC_MALLOC_ACQUIRE(); // acquire lock early, so that parallel calls to this function won't race to allocate.
  uint32_t i = find_weak_ptr_index(strong_ptr);
  if (i != INVALID_INDEX)
  {
    void *weak_ptr = weak_ptrs[i].weak_ptr;
    GC_MALLOC_RELEASE();
    return weak_ptr; // Return the already existing block if so.
  }

  // Allocate a reference block to hold the strong ptr -> weak ptr association.
  void **ref_block = (void**)malloc(sizeof(void*));
  if (!ref_block)
  {
    GC_MALLOC_RELEASE();
    return 0;
  }

  record_gc_malloc(ref_block);

  // Store the strong pointer to the allocated reference block.
  *ref_block = strong_ptr;

  uint32_t old_mask = weak_ptrs_mask;
  if (2*num_weak_ptrs_entries >= weak_ptrs_mask)
  {
    weak_ptrs_mask = (weak_ptrs_mask << 1) | 1;

    weak_ptr_map *old_weak_ptrs = weak_ptrs;
    weak_ptrs = (weak_ptr_map*)calloc(weak_ptrs_mask+1, sizeof(weak_ptr_map));
    assert(weak_ptrs);
    num_weak_ptrs_entries = 0;

    if (old_weak_ptrs)
    {
      for(uint32_t i = 0; i <= old_mask; ++i)
        if ((uintptr_t)old_weak_ptrs[i].strong_ptr > 1)
          insert_weak_ptr(old_weak_ptrs[i].strong_ptr, old_weak_ptrs[i].weak_ptr);
      free(old_weak_ptrs);
    }
  }

  // Mark the reference block as a weak pointer and as a leaf (don't scan contents).
  // The leaf mark is what makes this reference block a weak reference to the
  // allocation.
  i = table_find(ref_block);
  assert(i != INVALID_INDEX);
  table[i] = (void*)((uintptr_t)table[i] | PTR_WEAK_BIT | PTR_LEAF_BIT);
  insert_weak_ptr(strong_ptr, ref_block); // Record the strong ptr -> weak ptr mapping.
  gc_make_root(ref_block); // Finally pin the weak pointer as a root allocation.
  GC_MALLOC_RELEASE();

  return ref_block;
}

void *gc_acquire_strong_ptr(void **weak_ptr_ptr __attribute__((nonnull)))
{
  assert(weak_ptr_ptr);
  void *weak_ptr = *weak_ptr_ptr;
  if (!weak_ptr) return 0;
  ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED();
  assert(gc_is_weak_ptr(weak_ptr));

  // We need to read the weak->strong reference block while holding the malloc
  // lock, because the background sweep worker thread may still be working, and
  // might clear the reference block.
  GC_MALLOC_ACQUIRE();
  void **ref_block = (void**)weak_ptr; // We have a weak pointer, so dereference the reference block
  void *strong_ptr = *ref_block; // Fetch the pointer from weak reference block.
  GC_MALLOC_RELEASE();
  if (strong_ptr) return strong_ptr;
  // The strong allocation has been freed, so mutate the caller's weak pointer to a null pointer, so that the reference block
  // will have the chance to eventually garbage collect as well.
  *weak_ptr_ptr = 0;
  return 0;
}
