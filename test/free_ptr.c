// Tests gc_free() which manually frees a managed allocation.
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Allocate and verify it exists
  void *ptr = gc_malloc(64);
  require(ptr != 0 && "gc_malloc should succeed");
  require(gc_num_ptrs() == 1 && "Should have one allocation");

  // Manually free the pointer
  require(gc_is_ptr(ptr) && "ptr should be valid before freeing");
  gc_free(ptr);
  require(!gc_is_ptr(ptr) && "ptr should no longer be valid after freeing");

  // The allocation count should drop to zero (since it's no longer tracked)
  require(gc_num_ptrs() == 0 && "After gc_free, allocation should be removed");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed after collection");
}
