// Tests that A->B link of objects will hold B alive through garbage collection,
// as long as A is being directly referenced.
// flags: -sBINARYEN_EXTRA_PASSES=--spill-pointers
#include "test.h"

char **global;

void func()
{
  global = (char**)gc_malloc(1024);
  global[0] = (char*)gc_malloc(1024);
  PIN(&global);
  require(gc_num_ptrs() == 2);

  gc_collect();
  require(gc_num_ptrs() == 2);
}

int main()
{
  CALL_INDIRECTLY(func);

  global = 0;
  PIN(&global);

  gc_collect();
  require(gc_num_ptrs() == 0);
}
