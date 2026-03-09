// custom root blocks are GC root memory regions that are located e.g. in
// manually malloc()ed dynamic memory. The difference between GC roots and
// custom root blocks are that GC roots are always managed GC allocations
// themselves, but custom root blocks are malloc()ed allocations outside the
// GC memory allocation domain.
typedef struct span { void *start, *end; } span;

static span *custom_roots;
static uint32_t num_custom_roots_slots_populated, custom_roots_mask;
static emscripten_lock_t custom_roots_lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;

static uint32_t hash_custom_root(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & custom_roots_mask; }

static void insert_custom_root(void *start __attribute__((nonnull)), void *end __attribute__((nonnull)))
{
  assert(start);
  assert(end);
  assert(start <= end);

#if !defined(NDEBUG) && !EMGC_SKIP_AUTOMATIC_STATIC_MARKING
  // When building without -DEMGC_SKIP_AUTOMATIC_STATIC_MARKING, custom added root blocks cannot be contained within global/static data section. (they are already covered automatically)
  assert(((uintptr_t)start >= (uintptr_t)&__heap_base + (uintptr_t)emscripten_get_heap_size() || (uintptr_t)end <= (uintptr_t)&__heap_base) && "When building without -DEMGC_SKIP_AUTOMATIC_STATIC_MARKING, custom root blocks cannot be memory areas that are contained in the program global/static data section, because that section is already explicitly tracked. Build with -DEMGC_SKIP_AUTOMATIC_STATIC_MARKING to skip automatic global/static marking.");
#endif

  uint32_t i = hash_custom_root(start);
  while((uintptr_t)custom_roots[i].start > 1)
  {
    assert(custom_roots[i].start != start && "gc_add_custom_root_block() attempted to register the same custom root block twice!");
    i = (i+1) & custom_roots_mask;
  }
  if ((uintptr_t)custom_roots[i].start == 0) ++num_custom_roots_slots_populated;
  custom_roots[i].start = start;
  custom_roots[i].end = end;
}

void gc_add_custom_root_block(void *ptr __attribute__((nonnull)), size_t bytes)
{
  assert(ptr);
  assert(gc_ptr_base(ptr) == 0); // This root block cannot be contained within managed memory area.
  gc_acquire_lock(&custom_roots_lock);
  uint32_t old_mask = custom_roots_mask;
  if (2*num_custom_roots_slots_populated >= custom_roots_mask)
  {
    custom_roots_mask = (custom_roots_mask << 1) | 1;

    span *old_roots = custom_roots;
    custom_roots = (span*)calloc(custom_roots_mask+1, sizeof(span));
    assert(custom_roots);
    num_custom_roots_slots_populated = 0;
    if (old_roots)
    {
      for(uint32_t i = 0; i <= old_mask; ++i)
        if ((uintptr_t)old_roots[i].start > 1) insert_custom_root(old_roots[i].start, old_roots[i].end);
      free(old_roots);
    }
  }
  insert_custom_root(ptr, ptr + bytes);
  gc_release_lock(&custom_roots_lock);
}

void gc_remove_custom_root_block(void *ptr __attribute__((nonnull)))
{
  assert(custom_roots != 0);
  assert(ptr);
  gc_acquire_lock(&custom_roots_lock);
  bool found_custom_root_block = false;
  for(uint32_t i = hash_custom_root(ptr); custom_roots[i].start; i = (i+1) & custom_roots_mask)
    if (custom_roots[i].start == ptr)
    {
      custom_roots[i].start = custom_roots[i].end = (void*)1;
      found_custom_root_block = true;
      break;
    }
  assert(found_custom_root_block == true && "gc_remove_custom_root_block() did not find the specified custom root block!");
  gc_release_lock(&custom_roots_lock);
}
