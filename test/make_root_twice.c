// Tests the gc_make_root() function will keep an allocation alive.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  void **ptr = (void**)gc_malloc(1024);
  require(gc_num_ptrs() == 1 && "Root pointer should be tracked as a managed pointer.");
  gc_make_root(ptr); // Make this allocation a root, so it won't be collected.
  require(debug_gc_num_roots_slots_populated() == 1);
  for(int i = 0; i < 1000; ++i)
    gc_make_root(ptr); // Deliberately make the allocation root multiple times. These calls should all be effectively no-op.
  require(debug_gc_num_roots_slots_populated() == 1);
  *ptr = (void*)gc_malloc(1024);
  require(gc_num_ptrs() == 2 && "Should have two allocations.");
  gc_free(ptr);
  require(gc_num_ptrs() == 1 && "Root pointer should have been freed.");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "All managed allocations should be gone now.");
}
