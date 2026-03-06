typedef struct finalizer_map
{
  void *ptr;
  gc_finalizer finalizer;
} finalizer_map;

static finalizer_map *finalizers;
static uint32_t num_finalizers, num_finalizer_slots_populated, finalizers_mask, num_finalizers_marked;

static uint32_t hash_finalizer(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & finalizers_mask; }

static uint32_t find_finalizer_index(void *ptr)
{
  for(uint32_t i = hash_finalizer(ptr); finalizers[i].ptr; i = (i+1) & finalizers_mask)
    if (finalizers[i].ptr == ptr) return i;
  return INVALID_INDEX;
}

static void find_and_run_a_finalizer()
{
  ASSERT_GC_MALLOC_IS_ACQUIRED();

  for(uint32_t i = 0, offset; i <= table_mask; i += 64)
    for(uint64_t b = ((uint64_t*)used_table)[i>>6] & ~((uint64_t*)mark_table)[i>>6]; b; b ^= (1ull<<offset))
    {
      uint32_t j = i + (offset = __builtin_ctzll(b));
      if (HAS_FINALIZER_BIT(table[j]))
      {
        table[j] = (void*)((uintptr_t)table[j] ^ PTR_FINALIZER_BIT);
        uint32_t f = find_finalizer_index(REMOVE_FLAG_BITS(table[j]));
        assert(f != INVALID_INDEX);
        void *ptr = finalizers[f].ptr;
        finalizers[f].ptr = (void*)1;
        gc_finalizer finalizer_to_run = finalizers[f].finalizer;
        --num_finalizers;
        // Call the finalizer without GC lock present, so that the finalizer
        // function can perform GC allocations if necessary.
        GC_MALLOC_RELEASE();
        finalizer_to_run(ptr);
        GC_MALLOC_ACQUIRE();
        return; // In this sweep, we are not going to do anything else.
      }
    }
}

static void insert_finalizer(void *ptr, gc_finalizer finalizer)
{
  assert(ptr);
  uint32_t i = hash_finalizer(ptr);
  while((uintptr_t)finalizers[i].ptr > 1 && finalizers[i].ptr != ptr)
    i = (i+1) & finalizers_mask;

  if (!finalizers[i].ptr) ++num_finalizer_slots_populated;
  if (finalizers[i].ptr != ptr)
  {
    ++num_finalizers;
    finalizers[i].ptr = ptr;
  }
  finalizers[i].finalizer = finalizer;
}

void gc_register_finalizer(void *ptr, gc_finalizer finalizer)
{
  assert(ptr);
  assert(gc_is_strong_ptr(ptr));
  GC_MALLOC_ACQUIRE();
  uint32_t old_mask = finalizers_mask;
  if (2*num_finalizer_slots_populated >= finalizers_mask)
  {
    finalizers_mask = (finalizers_mask << 1) | 1;

    finalizer_map *old_finalizers = finalizers;
    finalizers = (finalizer_map*)calloc(finalizers_mask+1, sizeof(finalizer_map));
    assert(finalizers);
    num_finalizer_slots_populated = 0;

    if (old_finalizers)
    {
      for(uint32_t i = 0; i <= old_mask; ++i)
        if ((uintptr_t)old_finalizers[i].ptr > 1)
          insert_finalizer(old_finalizers[i].ptr, old_finalizers[i].finalizer);
      free(old_finalizers);
    }
  }
  insert_finalizer(ptr, finalizer);

  uint32_t i = table_find(ptr);
  assert(i != INVALID_INDEX);
  table[i] = (void*)((uintptr_t)table[i] | PTR_FINALIZER_BIT);
  GC_MALLOC_RELEASE();
}
