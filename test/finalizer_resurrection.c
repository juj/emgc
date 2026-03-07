// Tests that a finalizer can resurrect an object by calling gc_make_root() on it.
// After resurrection the object must survive subsequent collections until explicitly un-rooted.
// flags: -sSPILL_POINTERS
#include "test.h"

int finalizer_ran = 0;
void *resurrected_ptr;

void resurrect(void *ptr)
{
  finalizer_ran = 1;
  gc_make_root(ptr);   // Pin the allocation to prevent it from being freed.
  resurrected_ptr = ptr;
}

void func()
{
  void *ptr = gc_malloc(64);
  gc_register_finalizer(ptr, resurrect);
}

void unroot()
{
  // Un-root and collect: the finalizer bit was already cleared, so the object is freed normally.
  gc_unmake_root(resurrected_ptr);
  resurrected_ptr = 0;
}

int main()
{
  CALL_INDIRECTLY(func);

  // First collection: object is unreachable so the finalizer fires.
  // The finalizer calls gc_make_root(), keeping the object alive.
  gc_collect();
  require(finalizer_ran && "Finalizer must have run on the first collection.");
  require(gc_num_ptrs() == 1 && "Resurrected object must still be alive after finalization.");

  // Second collection: object is now a root and must survive.
  gc_collect();
  require(gc_num_ptrs() == 1 && "Resurrected root must survive a second collection.");

  CALL_INDIRECTLY(unroot);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Un-rooted resurrected object must be collected.");
}
