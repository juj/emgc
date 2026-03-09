// Tests multiple custom root blocks registered simultaneously, and that removing
// one block only releases the pointers it was rooting, leaving the others unaffected.
// flags: -sSPILL_POINTERS -DNDEBUG
#include "test.h"
#include <stdlib.h>

void **block_a;
void **block_b;

void func()
{
  block_a[0] = gc_malloc(64);
  block_b[0] = gc_malloc(64);
}

int main()
{
  block_a = (void**)malloc(sizeof(void*));
  block_b = (void**)malloc(sizeof(void*));
  gc_add_custom_root_block(block_a, sizeof(void*));
  gc_add_custom_root_block(block_b, sizeof(void*));

  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 2 && "Both pointers should survive while both blocks are registered.");

  // Remove only block_a; its pointer must be collected while block_b's survives.
  gc_remove_custom_root_block(block_a);

  gc_collect();
  require(gc_num_ptrs() == 1 && "Only the pointer in the removed block should be collected.");

  gc_remove_custom_root_block(block_b);

  gc_collect();
  require(gc_num_ptrs() == 0 && "The remaining pointer must be collected after removing the second block.");

  free(block_a);
  free(block_b);
}
