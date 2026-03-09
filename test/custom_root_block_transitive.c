// Tests that GC pointers transitively reachable through a custom root block are kept alive.
// If the block contains pointer A, and A's contents include pointer B, then B must
// also survive collection, just as it would via any other root.
// flags: -sSPILL_POINTERS -DNDEBUG
#include "test.h"
#include <stdlib.h>

void **block;

void func()
{
  void **a = (void**)gc_malloc(sizeof(void*));
  void  *b =         gc_malloc(64);
  a[0] = b;      // A -> B
  block[0] = a;  // block -> A
}

int main()
{
  block = (void**)malloc(sizeof(void*));
  gc_add_custom_root_block(block, sizeof(void*));

  CALL_INDIRECTLY(func);
  // Stack cleared. block[0] = A, A[0] = B.

  gc_collect();
  require(gc_num_ptrs() == 2 && "Both A (direct) and B (transitive through A) must survive.");

  // Removing the block severs the only path to A, which in turn loses the only path to B.
  gc_remove_custom_root_block(block);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Both A and B must be collected once the root block is removed.");

  free(block);
}
