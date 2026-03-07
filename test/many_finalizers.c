// Tests that multiple objects with finalizers are all eventually finalized and freed.
// The GC processes one finalizer per collection cycle, so N finalizeable objects require
// N collections to run all finalizers, then one final collection to free the objects.
// flags: -sSPILL_POINTERS
#include "test.h"

#define N 5

int finalized_count = 0;
void count_finalizer(void *ptr) { ++finalized_count; }

void func()
{
  for(int i = 0; i < N; ++i)
  {
    void *ptr = gc_malloc(64);
    gc_register_finalizer(ptr, count_finalizer);
  }
}

int main()
{
  CALL_INDIRECTLY(func);

  // N collection cycles to run all N finalizers (one per cycle).
  for(int i = 0; i < N; ++i) gc_collect();
  require(finalized_count == N && "All N finalizers must have run.");

  // One final collection to free the now-finalized (no longer has PTR_FINALIZER_BIT) objects.
  gc_collect();
  require(gc_num_ptrs() == 0 && "All finalized objects must be freed after the sweep cycle.");
}
