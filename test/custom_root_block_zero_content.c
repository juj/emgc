// Tests that clearing a GC pointer from within a custom root block (without
// removing the block itself) causes the pointer to be collected on the next
// gc_collect(). The GC scans the block's current contents at collection time,
// so a zeroed slot no longer roots the allocation it previously held.
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

  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer must survive while it is present in the block.");

  // Clear the slot inside the block without unregistering the block.
  block[0] = 0;

  gc_collect();
  require(gc_num_ptrs() == 0 && "Pointer must be collected when its slot inside the block is zeroed out.");

  gc_remove_custom_root_block(block);
  free(block);
}
