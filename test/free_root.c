// Tests that one can gc_free() root allocations.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  char *root = (char*)gc_malloc_root(1024);
  require(gc_num_ptrs() == 1 && "Root pointer should be tracked as a managed pointer.");
  gc_free(root);
  require(gc_num_ptrs() == 0 && "Root pointer should have been freed.");
}

int main()
{
  CALL_INDIRECTLY(func);
  gc_collect(); // Finally, check that GC runs fine after having freed the root.
}
