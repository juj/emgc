// Smoke test for gc_dump().
// Verifies that gc_dump() does not crash and does not corrupt GC state,
// both when called on an empty table and when a mix of allocation types
// (plain, root, leaf) is present.
// flags: -sSPILL_POINTERS
#include "test.h"

int main()
{
  gc_dump(); // Empty GC state: must not crash.

  void *plain = gc_malloc(64);
  void *root  = gc_malloc_root(64);
  void *leaf  = gc_malloc_leaf(64);

  gc_dump(); // Mixed allocation types: must not crash.

  require(gc_num_ptrs() == 3 && "GC state must be intact after gc_dump().");

  gc_free(plain);
  gc_free(root);
  gc_free(leaf);

  gc_dump(); // Empty again after manual frees: must not crash.

  require(gc_num_ptrs() == 0);
}
