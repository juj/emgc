// Tests that a weak pointer cannot prevent garbage collection.
#include "test.h"

void *global;

void func()
{
  global = (void*)gc_get_weak_ptr(gc_malloc(1024));
  PIN(&global);
  require(global && "Alloc must have succeeded");
  require(gc_num_ptrs() == 1);

  require(gc_is_weak_ptr(global) && "gc_get_weak_ptr() should have returned a weak pointer.");
  void *strong = gc_acquire_strong_ptr(global);
  require(strong != 0 && "gc_acquire_strong_ptr() should have worked before GC.");
  require(gc_is_strong_ptr(strong) && "gc_acquire_strong_ptr() should have returned a strong pointer.");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "A weak pointer should not have prevented GC.");

  void *strong = gc_acquire_strong_ptr(global);
  require(strong == 0 && "Acquiring strong pointer from stale weak pointer should no longer work.");
}
