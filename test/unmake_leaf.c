// Tests that gc_unmake_leaf() converts a leaf allocation back to a regular allocation
// that gets its contents scanned during GC.
// flags: -sSPILL_POINTERS
#include "test.h"

char *container;

void func()
{
  container = (char*)gc_malloc(100);
  gc_make_leaf(container);

  // Store a pointer inside the leaf - it should NOT be scanned initially
  *(void**)container = gc_malloc(100);

  require(gc_num_ptrs() == 2 && "Should have two allocations");

  PIN(container);
}

void func2()
{
  // Now convert the leaf back to a regular allocation
  gc_unmake_leaf(container);
  PIN(container);
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer inside leaf should have been collected (leaf not scanned)");

  // Allocate a new pointer and store it in the container
  CALL_INDIRECTLY(func2);
  *(void**)container = gc_malloc(100);

  require(gc_num_ptrs() == 2 && "Should have container + new allocation");

  gc_collect();
  require(gc_num_ptrs() == 2 && "After gc_unmake_leaf(), pointer inside container should be scanned and kept alive");
}
