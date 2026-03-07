// Tests the gc_is_ptr() function which checks if a pointer is a managed allocation.
// flags: -sSPILL_POINTERS

#include "test.h"

void func()
{
  // Test with null
  require(!gc_is_ptr(0) && "gc_is_ptr(null) should return 0");

  // Allocate memory and test it is recognized as a pointer
  void *ptr = gc_malloc(128);
  require(ptr != 0 && "gc_malloc should succeed");
  require(gc_is_ptr(ptr) && "gc_is_ptr on valid allocation should return true");
}

int main()
{
  CALL_INDIRECTLY(func);

  // Test with memory that was never allocated
  void *fake_ptr = (void*)0x12345678;
  require(!gc_is_ptr(fake_ptr) && "gc_is_ptr on random address should return 0");

  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should have been freed");
}
