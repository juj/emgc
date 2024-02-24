// Tests garbage collection from a global variable.
// flags: -sBINARYEN_EXTRA_PASSES=--spill-pointers
#include "test.h"

char *global;

void func()
{
  global = (char*)gc_malloc(1024);
  PIN(&global);
  require(gc_num_ptrs() == 1);

  gc_collect();
  require(gc_num_ptrs() == 1);
}

int main()
{
  CALL_INDIRECTLY(func);

  global = 0;
  PIN(&global);

  gc_collect();
  require(gc_num_ptrs() == 0);
}
