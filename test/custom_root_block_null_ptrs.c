// Tests that a custom root block containing a mix of null entries and valid GC
// pointers is scanned correctly. Null (and non-pointer) words must not cause
// crashes, and only the actual GC pointers must be kept alive.
// flags: -sSPILL_POINTERS -DNDEBUG
#include "test.h"
#include <stdlib.h>

#define N 8

void **block;

void func()
{
  // Place a valid GC pointer in even slots; leave odd slots as null.
  for (int i = 0; i < N; ++i)
    block[i] = (i % 2 == 0) ? gc_malloc(16) : 0;
}

int main()
{
  block = (void**)malloc(N * sizeof(void*));
  gc_add_custom_root_block(block, N * sizeof(void*));

  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == N/2 && "Only the non-null GC pointers should survive; null slots must be ignored.");

  gc_remove_custom_root_block(block);
  free(block);
}
