// Tests gc_calloc_leaf()
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Test gc_calloc_leaf - zero-initialized leaf
  int *leaf_arr = (int*)gc_calloc_leaf(3 * sizeof(int));
  require(leaf_arr != 0 && "gc_calloc_leaf should succeed");
  for (int i = 0; i < 3; ++i) {
    require(leaf_arr[i] == 0 && "gc_calloc_leaf memory should be zero-initialized");
  }
}

int main()
{
  CALL_INDIRECTLY(func);

  // All allocated pointers should have been collected
  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed after collection");
}
