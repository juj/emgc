// Tests that a weak pointer cannot prevent garbage collection.
// flags: -sSPILL_POINTERS
#include "test.h"

void *global;

void func()
{
  void *ptr = gc_malloc(1024);
  global = (void*)gc_get_weak_ptr(ptr);
  PIN(&global);
  require(global && "Alloc must have succeeded");
  require(gc_num_ptrs() == 2); // We should have two allocs: one for the strong ptr, and another for the control block of the weak ptr.

  require(gc_is_weak_ptr(global) && "gc_get_weak_ptr() should have returned a weak pointer.");
  void *strong = gc_acquire_strong_ptr(&global);
  require(strong != 0 && "gc_acquire_strong_ptr() should not return null.");
  require(strong == ptr && "gc_acquire_strong_ptr() should have required the original allocated pointer.");

  require(gc_is_strong_ptr(strong) && "gc_acquire_strong_ptr() should detect correctly as a gc_is_strong_ptr().");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 1 && "A weak pointer should not have prevented GC.");

  void *strong = gc_acquire_strong_ptr(&global);
  require(strong == 0 && "Acquiring strong pointer from stale weak pointer should no longer work.");
}
