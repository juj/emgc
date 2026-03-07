// Tests gc_malloc_root()
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Test gc_malloc_root - allocates and marks as root
  void *root_ptr = gc_malloc_root(64);
  require(root_ptr != 0 && "gc_malloc_root should succeed");
  require(gc_is_root(root_ptr) && "gc_malloc_root allocation should be a root");
}

int main()
{
  CALL_INDIRECTLY(func);

  // All allocated pointers should have been collected
  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed after collection");
}
