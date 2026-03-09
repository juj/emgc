// Tests that a custom root block can be removed and immediately re-added (without
// an intervening gc_collect()) and continue to root pointers correctly.
// flags: -sSPILL_POINTERS -DNDEBUG
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
  // block[0] is now the sole reference to the GC allocation.

  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer must survive with block registered.");

  // Remove then immediately re-add — no collect in between. The pointer in
  // block[0] is never swept because we re-add before the next collection.
  gc_remove_custom_root_block(block);
  gc_add_custom_root_block(block, sizeof(void*));

  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer must survive after the block is removed and re-added.");

  // Final cleanup: remove the block and clear its content, then verify collection.
  gc_remove_custom_root_block(block);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Pointer must be collected after the block is finally removed.");

  free(block);
}
