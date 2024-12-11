// emgc-ptr_base.c deals with converting interior pointers that point somewhere
// inside an allocated buffer, into a pointer that points to the base address
// of the allocation.

void *gc_ptr_base(void *ptr)
{
  if ((uintptr_t)ptr - (uintptr_t)&__heap_base >= (uintptr_t)emscripten_get_heap_size() - (uintptr_t)&__heap_base) return 0;
  if (!num_allocs) return 0;

  uint32_t i = hash_ptr(ptr);
  if (!table[i]) while(!table[i]) i = (i + table_mask) & table_mask;
  else           while(table[(i+1) & table_mask]) i = (i+1) & table_mask;

  void *max = 0;
  for(; table[i]; --i)
    if (table[i] != SENTINEL_PTR)
    {
      void *t = REMOVE_FLAG_BITS(table[i]);
      if (t <= ptr && t > max) max = t;
    }
  return ((uintptr_t)ptr <= (uintptr_t)max + malloc_usable_size(max)) ? max : 0;
}
