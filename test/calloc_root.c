// Tests gc_calloc_root()
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Test gc_calloc_root - allocates zeroed memory and marks as root
  int *root_arr = (int*)gc_calloc_root(5 * sizeof(int));
  require(root_arr != 0 && "gc_calloc_root should succeed");
  for (int i = 0; i < 5; ++i) {
    require(root_arr[i] == 0 && "gc_calloc_root memory should be zero-initialized");
  }
  require(gc_is_root(root_arr) && "gc_calloc_root allocation should be a root");
}

int main()
{
  CALL_INDIRECTLY(func);

  // All allocated pointers should have been collected
  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed after collection");
}
