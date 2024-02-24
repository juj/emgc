// Tests memory pressure with a lot of temporary managed allocations.
// flags: -sBINARYEN_EXTRA_PASSES=--spill-pointers
#include "test.h"
#include <string.h>

char *linear_or_quadratic_memory_use()
{
  char *str = (char*)gc_malloc(1);
  str[0] = '\0';
  int len = 1;
  for(int i = 0; i < 1000; ++i)
  {
    len += 3;
    char *str2 = (char*)gc_malloc(len);
    strcpy(str2, str);
    strcat(str2, "foo");
    str = str2;
    gc_collect();
  }
  return str;
}

int main()
{
  char *foo = (char*)CALL_INDIRECTLY_P((void* (*)(void))linear_or_quadratic_memory_use);
  gc_collect();
  require(gc_num_ptrs() < 10 && "Temporary strings should have been freed.");
  printf("String length: %d\n", (int)strlen(foo));
}
