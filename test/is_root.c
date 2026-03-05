// Tests the gc_is_root() function returns correct values.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  char *ptr = (char*)gc_malloc(1024);
  PIN(&ptr);
  
  require(!gc_is_root(ptr) && "Freshly allocated pointer should not be a root.");

  gc_make_root(ptr);
  require(gc_is_root(ptr) && "After gc_make_root(), should be identified as root.");
  
  gc_unmake_root(ptr);
  require(!gc_is_root(ptr) && "After gc_unmake_root(), should not be a root.");

  // Finally make it a root again for the remainder of the test.
  gc_make_root(ptr);
}

int main()
{
  CALL_INDIRECTLY(func);
  
  // GC should not collect the original ptr it since it's a root. (effectively will leak for the remainder of the test)
  gc_collect();
  require(gc_num_ptrs() == 1 && "Root pointer should survive garbage collection.");

  // Non-GC pointers should return false.
  int stack_var = 42;
  require(!gc_is_root(&stack_var) && "Stack address should not be identified as root.");
  require(!gc_is_root(0) && "Null pointer should not be a root.");
}
