// Tests that a single custom root block can hold many GC pointers simultaneously,
// and that all of them are kept alive across gc_collect(). Upon removal, all are
// collected.
// flags: -sSPILL_POINTERS
#include "test.h"
#include <stdlib.h>

#define N 8

void **block;

void func()
{
  for (int i = 0; i < N; ++i)
    block[i] = gc_malloc(16);
}

int main()
{
  block = (void**)malloc(N * sizeof(void*));
  gc_add_custom_root_block(block, N * sizeof(void*));

  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == N && "All GC pointers inside the block must survive collection.");

  gc_remove_custom_root_block(block);

  gc_collect();
  require(gc_num_ptrs() == 0 && "All pointers must be collected after root block removal.");

  free(block);
}
