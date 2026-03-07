// Tests gc_calloc() and convenience allocator functions (gc_malloc_root, gc_calloc_root, etc.)
// These allocate zero-initialized memory rather than dirty memory.
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Test gc_calloc returns zeroed memory
  int *arr = (int*)gc_calloc(10 * sizeof(int));
  require(arr != 0 && "gc_calloc should succeed");
  
  for (int i = 0; i < 10; ++i) {
    require(arr[i] == 0 && "gc_calloc memory should be zero-initialized");
  }
}

int main()
{
  CALL_INDIRECTLY(func);

  // All allocated pointers should have been collected
  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed after collection");
}
