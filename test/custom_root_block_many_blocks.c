// Stress test: registers many custom root blocks to exercise hash table growth
// inside the custom root block registry. All GC pointers held across the blocks
// must survive collection, and must all be collected after removal.
// flags: -sSPILL_POINTERS
#include "test.h"
#include <stdlib.h>

// 16384 blocks forces several doublings of the internal custom_roots hash table.
#define NUM_BLOCKS 16384

void **blocks[NUM_BLOCKS];

void func()
{
  for (int i = 0; i < NUM_BLOCKS; ++i)
    blocks[i][0] = gc_malloc(16);
}

int main()
{
  for (int i = 0; i < NUM_BLOCKS; ++i)
  {
    blocks[i] = (void**)malloc(sizeof(void*));
    gc_add_custom_root_block(blocks[i], sizeof(void*));
  }

  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == NUM_BLOCKS && "Every pointer in every block must survive collection.");

  for (int i = 0; i < NUM_BLOCKS; ++i)
  {
    gc_remove_custom_root_block(blocks[i]);
    free(blocks[i]);
  }

  gc_collect();
  require(gc_num_ptrs() == 0 && "All pointers must be collected after all root blocks are removed.");
}
