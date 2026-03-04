// Tests that contents of leaf allocations are not scanned.
// If A contains a pointer to B, and A is marked as a leaf, then if there
// are no references to B elsewhere, the fact that A points to B should
// not keep B alive. (since A's contents won't be scanned)
// flags: -sSPILL_POINTERS
#include "test.h"

char **leaf;

void func()
{
  leaf = (char**)gc_malloc_leaf(1024); // leaf contents should not be scanned
  leaf[0] = (char*)gc_malloc(1024); // allocate a pointer to leaf - should not keep it alive
  PIN(&leaf);
  require(gc_num_ptrs() == 2);
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  // Since 'leaf' is a leaf, its contents are not scanned. The pointer inside it to
  // leaf[0] is never discovered, so leaf[0] becomes unreachable and gets collected.
  // Only the leaf object itself remains (it's pinned via 'leaf').
  require(gc_num_ptrs() == 1 && "Leaf contents should not be scanned; only the leaf itself should remain.");
}
