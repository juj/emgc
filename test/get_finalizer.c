// Tests gc_get_finalizer() returns the correct registered finalizer.
// flags: -sSPILL_POINTERS

#include "test.h"
#include <stdio.h>

void finalizer_a(void *ptr) { printf("Finalizer A\n"); }
void finalizer_b(void *ptr) { printf("Finalizer B\n"); }

void func()
{
  void *ptr1 = gc_malloc(64);
  void *ptr2 = gc_malloc(64);
  
  // Initially no finalizer
  require(gc_get_finalizer(ptr1) == 0 && "New allocation should have no finalizer");
  require(gc_get_finalizer(ptr2) == 0 && "New allocation should have no finalizer");
  
  // Register different finalizers on each pointer
  gc_register_finalizer(ptr1, finalizer_a);
  gc_register_finalizer(ptr2, finalizer_b);
  
  // Verify correct finalizer is returned for each
  require(gc_get_finalizer(ptr1) == finalizer_a && "Should return first finalizer");
  require(gc_get_finalizer(ptr2) == finalizer_b && "Should return second finalizer");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should be freed");
}
