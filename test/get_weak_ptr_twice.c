// Tests that calling gc_get_weak_ptr() twice on the same strong allocation
// returns the same ref block rather than allocating a new one each time.
// The weak-pointer map must deduplicate calls for the same strong pointer.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  void *strong = gc_malloc(64);
  require(gc_num_ptrs() == 1 && "Expect exactly one allocation.");

  void *weak1 = gc_get_weak_ptr(strong);
  require(weak1 != 0 && "First gc_get_weak_ptr must succeed.");
  require(gc_num_ptrs() == 2 && "Expect strong + one ref block.");

  void *weak2 = gc_get_weak_ptr(strong);
  require(weak2 != 0 && "Second gc_get_weak_ptr must succeed.");
  require(weak2 == weak1 && "Second call must return the existing ref block, not a new one.");
  require(gc_num_ptrs() == 2 && "No new allocation must be created by the second call.");
}

int main()
{
  CALL_INDIRECTLY(func);

  // First collection: strong is unreachable; ref block is unrooted by sweep.
  gc_collect();
  require(gc_num_ptrs() == 1 && "Strong allocation freed; ref block survives until next sweep.");

  // Second collection: ref block is no longer reachable or rooted.
  gc_collect();
  require(gc_num_ptrs() == 0 && "Ref block must be freed on the subsequent sweep.");
}
