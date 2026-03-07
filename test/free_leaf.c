// Tests that gc_free() works correctly on a leaf allocation.
// A leaf has PTR_LEAF_BIT set in the table entry; gc_free() must strip
// all flag bits and remove the allocation from the table regardless.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  void *ptr = gc_malloc_leaf(128);
  require(ptr != 0 && "gc_malloc_leaf must succeed.");
  require(gc_is_ptr(ptr)  && "Leaf allocation must be a managed pointer.");
  require(gc_num_ptrs() == 1);

  gc_free(ptr);

  require(!gc_is_ptr(ptr) && "Freed leaf must no longer be a managed pointer.");
  require(gc_num_ptrs() == 0 && "gc_free on a leaf must remove it from the table.");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect(); // GC must run cleanly after a manually-freed leaf allocation.
  require(gc_num_ptrs() == 0);
}
