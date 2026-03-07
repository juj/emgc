// Tests that a weak pointer to a root allocation resolves correctly while the root is alive,
// and becomes stale once the root is un-rooted and collected.
// flags: -sSPILL_POINTERS
#include "test.h"

void *weak_global;
uintptr_t hidden_strong; // Stored bit-flipped so the GC does not mistake it for a live pointer.

void func_setup()
{
  void *strong = gc_malloc_root(1024);
  hidden_strong = (uintptr_t)strong ^ (uintptr_t)-1;
  weak_global = gc_get_weak_ptr(strong);
}

void func_verify_resolves()
{
  void *acquired = gc_acquire_strong_ptr(&weak_global);
  require(acquired != 0 && "Weak pointer must resolve while the root is alive.");
  require(gc_is_strong_ptr(acquired) && "Acquired pointer must be a strong GC pointer.");
}

void func_unroot()
{
  gc_unmake_root((void*)(hidden_strong ^ (uintptr_t)-1));
  hidden_strong = 0;
}

int main()
{
  CALL_INDIRECTLY(func_setup);

  // Root + weak ref block must survive GC.
  gc_collect();
  require(gc_num_ptrs() == 2 && "Root and its weak ref block must survive GC.");

  CALL_INDIRECTLY(func_verify_resolves);

  // Un-root the strong allocation so GC can collect it.
  CALL_INDIRECTLY(func_unroot);
  gc_collect();
  require(gc_num_ptrs() == 1 && "Strong object collected; only the weak ref block should remain.");

  // Weak pointer must now be stale.
  void *stale = gc_acquire_strong_ptr(&weak_global);
  require(stale == 0 && "Weak pointer must be stale after its target was collected.");
  require(weak_global == 0 && "gc_acquire_strong_ptr() must null out the caller's stale pointer.");

  gc_collect();
  require(gc_num_ptrs() == 0 && "Weak ref block must be collected once the weak pointer is nulled.");
}
