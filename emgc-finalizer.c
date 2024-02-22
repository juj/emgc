typedef struct finalizer_map
{
  void *ptr;
  gc_finalizer finalizer;
} finalizer_map;

static finalizer_map *finalizers;
uint32_t num_finalizers, num_finalizer_entries, finalizers_mask;

static uint32_t hash_finalizer(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & finalizers_mask; }

static uint32_t find_finalizer_index(void *ptr)
{
  for(uint32_t i = hash_finalizer(ptr); finalizers[i].ptr; i = (i+1) & finalizers_mask)
    if (finalizers[i].ptr == ptr) return i;
  return (uint32_t)-1;
}

static void find_and_run_a_finalizer()
{
  uint64_t *marks = (uint64_t*)mark_table;

  for(uint32_t i = 0, offset; i <= table_mask; i += 64)
    for(uint64_t bits = marks[i>>6]; bits; bits ^= (1ull<<offset))
    {
      uint32_t j = i + (offset = __builtin_ctzll(bits));
      if (((uintptr_t)table[j] & PTR_FINALIZER_BIT) != 0)
      {
        table[j] = (void*)((uintptr_t)table[j] ^ PTR_FINALIZER_BIT);
        uint32_t f = find_finalizer_index(table[j]);
        void *ptr = finalizers[f].ptr;
        finalizers[f].ptr = (void*)1;
        finalizers[f].finalizer(ptr);
        --num_finalizers;
        return; // In this sweep, we are not going to do anything else.
      }
    }
}

static void insert_finalizer(void *ptr, gc_finalizer finalizer)
{
  uint32_t i = hash_finalizer(ptr);
  while((uintptr_t)finalizers[i].ptr > 1 && finalizers[i].ptr != ptr)
    i = (i+1) & finalizers_mask;

  if (!ptr)
  {
    if (!finalizers[i].ptr) return;
    ptr = (void*)1;
  }

  if (!finalizers[i].ptr) ++num_finalizer_entries;
  if (finalizers[i].ptr != ptr)
  {
    ++num_finalizers;
    finalizers[i].ptr = ptr;
  }
  finalizers[i].finalizer = finalizer;
}

void gc_register_finalizer(void *ptr, gc_finalizer finalizer)
{
  uint32_t old_mask = finalizers_mask;
  if (2*num_finalizer_entries >= finalizers_mask)
  {
    finalizers_mask = (finalizers_mask << 1) | 1;

    finalizer_map *old_finalizers = finalizers;
    finalizers = (finalizer_map*)calloc(finalizers_mask+1, sizeof(finalizer_map));
    num_finalizer_entries = 0;

    if (old_finalizers)
    {
      for(uint32_t i = 0; i <= old_mask; ++i)
        if ((uintptr_t)old_finalizers[i].ptr > 1)
          insert_finalizer(old_finalizers[i].ptr, old_finalizers[i].finalizer);
      free(old_finalizers);
    }
  }
  insert_finalizer(ptr, finalizer);

  uint32_t i = gc_find_index(ptr);
  table[i] = (void*)((uintptr_t)table[i] | PTR_FINALIZER_BIT);
}
