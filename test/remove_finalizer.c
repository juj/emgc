// Tests gc_remove_finalizer() which removes a previously registered finalizer.
// flags: -sSPILL_POINTERS

#include "test.h"

int my_finalizer_executed = 0;

void my_finalizer(void *ptr)
{
  my_finalizer_executed = 1;
}

void func()
{
  void *ptr = gc_malloc(64);
  gc_register_finalizer(ptr, my_finalizer);
  require(gc_get_finalizer(ptr) != 0 && "Finalizer should be registered");
  
  gc_remove_finalizer(ptr);
  require(gc_get_finalizer(ptr) == 0 && "After removal, finalizer should be null");
}

int main()
{
  CALL_INDIRECTLY(func);
  
  // Now trigger collection when the object is no longer referenced
  gc_collect();
  
  // The finalizer should NOT have executed - it was removed earlier
  require(!my_finalizer_executed && "Removed finalizer should not execute on collection");
  require(gc_num_ptrs() == 0 && "Object should be collected after references are released");
}
