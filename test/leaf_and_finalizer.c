// Tests that a leaf allocation with a registered finalizer is correctly finalized
// when it becomes garbage. The PTR_LEAF_BIT prevents the GC from scanning the
// allocation's contents, but must not interfere with the PTR_FINALIZER_BIT that
// causes the finalizer to run before the object is freed.
// flags: -sSPILL_POINTERS
#include "test.h"

int finalizer_ran = 0;
void my_finalizer(void *ptr) { finalizer_ran = 1; }

void func()
{
  void *ptr = gc_malloc_leaf(64);
  gc_register_finalizer(ptr, my_finalizer);
}

int main()
{
  CALL_INDIRECTLY(func);

  // First collection: leaf object is unreachable; finalizer must run despite the leaf bit.
  gc_collect();
  require(finalizer_ran && "Finalizer must run even when the allocation has the leaf bit set.");

  // Second collection: finalized object has no finalizer bit anymore; must be freed.
  gc_collect();
  require(gc_num_ptrs() == 0 && "Finalized leaf object must be freed on the subsequent sweep.");
}
