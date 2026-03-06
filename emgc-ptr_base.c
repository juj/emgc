// emgc-ptr_base.c deals with converting interior pointers that point somewhere
// inside an allocated buffer, into a pointer that points to the base address
// of the allocation.

void *gc_ptr_base(void *ptr)
{
  if ((uintptr_t)ptr - (uintptr_t)&__heap_base >= (uintptr_t)emscripten_get_heap_size() - (uintptr_t)&__heap_base) return 0;
  if (!num_allocs) return 0;

  // TODO: Rewrite this to utilize an allocation range interval tree.
  GC_MALLOC_ACQUIRE();
  for(uint32_t i = 0, offset; i <= table_mask; i += 64)
    for(uint64_t bits = used_table[i>>6]; bits; bits ^= (1ull<<offset))
    {
      void *managed_ptr = REMOVE_FLAG_BITS(table[i + (offset = __builtin_ctzll(bits))]);
      if (ptr >= managed_ptr && (uintptr_t)ptr - (uintptr_t)managed_ptr < malloc_usable_size(managed_ptr))
      {
        GC_MALLOC_RELEASE();
        return managed_ptr;
      }
    }
  GC_MALLOC_RELEASE();
  return 0;
}
