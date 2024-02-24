// Tests the gc_unmake_root() function can be used to unregister an allocation from being a root.
// flags: -sSPILL_POINTERS
#include "test.h"

uintptr_t global;
void func()
{
  global = (uintptr_t)gc_malloc(1024);
  gc_make_root((void*)global); // Make this allocation a root, so it won't be collected.
  ++global; // Increment the pointer value to hide it from the GC.
}

void func2()
{
  gc_unmake_root((void*)(global-1));
}

int main()
{
  CALL_INDIRECTLY(func);
  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer declared as root should not have gotten collected.");

  CALL_INDIRECTLY(func2);
  gc_collect();
  require(gc_num_ptrs() == 0 && "Pointer undeclared from being a root should now have gotten collected.");
}
