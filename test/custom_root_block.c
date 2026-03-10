// Tests that a GC pointer stored inside a custom root block survives gc_collect().
// A malloc()-managed buffer (not GC-managed) is registered as a custom root region.
// The GC pointer inside the buffer must be kept alive for as long as the block is registered,
// even after the stack frame that created the allocation has been cleared.
// flags: -sSPILL_POINTERS
#include "test.h"
#include <stdlib.h>

void **block;

void func()
{
  block[0] = gc_malloc(64);
}

int main()
{
  block = (void**)malloc(sizeof(void*));
  gc_add_custom_root_block(block, sizeof(void*));

  CALL_INDIRECTLY(func);
  // After CALL_INDIRECTLY, the stack frame from func() is gone. block[0] is
  // the only remaining reference to the GC allocation.

  gc_collect();
  require(gc_num_ptrs() == 1 && "GC pointer in a registered custom root block must survive collection.");

  gc_remove_custom_root_block(block);
  free(block);
}
