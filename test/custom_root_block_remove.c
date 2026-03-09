// Tests that gc_remove_custom_root_block() correctly un-roots the block, allowing
// pointers that were only held inside it to be collected on the next gc_collect().
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
  require(gc_num_ptrs() == 1 && "Pointer should survive while the custom root block is registered.");

  gc_remove_custom_root_block(block);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Pointer must be collected after its custom root block is removed.");

  free(block);
}
