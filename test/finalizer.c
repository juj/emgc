// Tests invocation of finalizers.
#include "test.h"

int my_finalizer_executed = 0;
uintptr_t allocated_ptr;

void my_finalizer(void *ptr)
{
  my_finalizer_executed = 1;
  require((uintptr_t)ptr == ~allocated_ptr && "Finalizer should have been called with the GC pointer that is about to be finalized.");
}

void func()
{
  void *ptr = (char*)gc_malloc(1024);
  allocated_ptr = ~(uintptr_t)ptr;
  gc_register_finalizer(ptr, my_finalizer);

  gc_collect(); // Should not execute the finalizer.
  PIN(ptr);
  require(!my_finalizer_executed && "Finalizer should not have executed when the GC object is still alive.");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(my_finalizer_executed && "Finalizer should have been executed after the GC object has been reclaimed.");
  gc_collect();
  require(gc_num_ptrs() == 0 && "The finalizable object should have gotten collected.");
}
