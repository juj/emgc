// Tests garbage collection from a stack variable.
#include "test.h"

void func()
{
  char *stack = (char*)gc_malloc(1024);
  gc_collect();
  require(gc_num_ptrs() == 1 && "Managed pointer on stack should not have gotten garbage collected, but it did.");
  PIN(stack); // Use pointer on stack so that it is alive across the garbage collect call above.
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Pointer on stack in an old called function should have gotten freed, but didn't.");
}
