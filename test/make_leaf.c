#include "test.h"

char *string;
void func()
{
  string = (char*)gc_malloc(100);
  gc_make_leaf(string);
  *(int**)string = (int*)gc_malloc(100);
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect(); // will not scan contents of 'string', so the second allocation should be collected.
  require(gc_num_ptrs() == 1 && "Pointer inside leaf allocation should not have gotten scanned, but it did.");
}
