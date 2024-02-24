// Tests the gc_make_root() function will keep an allocation alive.
// flags: -sBINARYEN_EXTRA_PASSES=--spill-pointers
#include "test.h"

void func()
{
  char *leak = (char*)gc_malloc(1024);
  gc_make_root(leak); // Make this allocation a root, so it won't be collected.
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 1 && "Pointer declared as root should not have gotten collected.");
}
