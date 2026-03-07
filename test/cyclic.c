// Tests that mutually-referencing (cyclic) garbage is collected.
// A -> B -> A where neither A nor B is reachable from outside forms a reference
// cycle that a mark-and-sweep collector must still reclaim.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  void **a = (void**)gc_malloc(sizeof(void*));
  void **b = (void**)gc_malloc(sizeof(void*));
  *a = b; // A -> B
  *b = a; // B -> A (cycle)
  require(gc_num_ptrs() == 2);
}

int main()
{
  CALL_INDIRECTLY(func);
  gc_collect();
  require(gc_num_ptrs() == 0 && "Cyclic garbage must be reclaimed by mark-and-sweep.");
}
