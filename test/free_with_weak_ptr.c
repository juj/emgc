// Tests that gc_free() on a strong pointer immediately invalidates any associated weak pointer.
// flags: -sSPILL_POINTERS
#include "test.h"

void *weak_global;

void func()
{
  void *strong = gc_malloc(1024);
  weak_global = gc_get_weak_ptr(strong);
  require(gc_num_ptrs() == 2 && "Should have one strong + one weak ref block allocation.");

  gc_free(strong);
  require(gc_num_ptrs() == 1 && "After gc_free(), only the weak ref block should remain.");

  // The weak pointer should be immediately stale since the strong object was freed.
  void *acquired = gc_acquire_strong_ptr(&weak_global);
  require(acquired == 0 && "Weak pointer must be stale immediately after gc_free() of its target.");
  require(weak_global == 0 && "gc_acquire_strong_ptr() must null out the caller's stale pointer.");
}

int main()
{
  CALL_INDIRECTLY(func);
  // weak_global is now 0, so the ref block is unreachable and should be collected.
  gc_collect();
  require(gc_num_ptrs() == 0 && "Weak ref block must be collected once the weak pointer is nulled.");
}
