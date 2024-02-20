// Tests gc_num_ptrs()
#include "test.h"

int main()
{
  require(gc_num_ptrs() == 0 && "There should be no managed ptrs at program startup.");
  gc_malloc(1024);
  require(gc_num_ptrs() == 1 && "gc_malloc() should increase the number of managed pointers by one.");
  gc_malloc(1024);
  require(gc_num_ptrs() == 2 && "gc_malloc() should increase the number of managed pointers by one.");
}
