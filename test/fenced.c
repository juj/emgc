// This test verifies that the fenced mode works in singlethreaded builds.
// flags: -DEMGC_FENCED=1 -sSPILL_POINTERS
#include "test.h"

char *global;

void *work(void *user1, void *user2)
{
  global = gc_malloc(4); // Inside fenced scope we can access GC allocations.
  return 0;
}

int main()
{
  gc_enter_fence_cb(work, 0, 0);
  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer in fenced access should not have gotten freed.");
}
