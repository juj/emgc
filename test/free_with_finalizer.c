// Tests that gc_free() does NOT invoke the finalizer on an allocation that has one.
// The API contract states: "If the allocation had a finalizer, it is *not* called."
// flags: -sSPILL_POINTERS
#include "test.h"

int finalizer_ran = 0;
void my_finalizer(void *ptr) { finalizer_ran = 1; }

void func()
{
  void *ptr = gc_malloc(64);
  gc_register_finalizer(ptr, my_finalizer);
  gc_free(ptr);
  require(!finalizer_ran && "gc_free() must not invoke the finalizer.");
  require(gc_num_ptrs() == 0 && "gc_free() must remove the allocation immediately.");
}

int main()
{
  CALL_INDIRECTLY(func);
  gc_collect();
  require(!finalizer_ran && "Finalizer must not have run even after a subsequent gc_collect().");
}
