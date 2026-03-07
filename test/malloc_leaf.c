// Tests gc_malloc_leaf()
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Test gc_malloc_leaf - allocates and marks as leaf (contents not scanned)
  void *leaf_ptr = gc_malloc_leaf(32);
  require(leaf_ptr != 0 && "gc_malloc_leaf should succeed");
}

int main()
{
  CALL_INDIRECTLY(func);

  // All allocated pointers should have been collected
  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed after collection");
}
