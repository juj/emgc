// Tests garbage collection from a stack variable, but the stack variable will be returned to the calling function.
// flags: -sBINARYEN_EXTRA_PASSES=--spill-pointers
#include "test.h"

void *func2()
{
  char *stack = (char*)gc_malloc(1024);
  gc_collect();
  require(gc_num_ptrs() == 1 && "Managed pointer on stack should not have gotten garbage collected, but it did.");
  return stack; // Return the pointer from function so that it is alive throughout this function call.
}

void func()
{
  void *ptr = CALL_INDIRECTLY_P(func2);
  gc_collect();
  require(gc_num_ptrs() == 1 && "Managed pointer obtained as return value from a function should not have gotten garbage collected, but it did.");
  PIN(ptr); // Use the returned pointer so that it is alive across the garbage collect call above.
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Pointer on stack in an old called function should have gotten freed, but didn't.");
}
