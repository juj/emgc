// Tests the gc_ptr_base(ptr) functionality.
// flags: -sMALLOC=emmalloc

#include "test.h"

int main()
{
  char *ptr = (char*)gc_malloc(1024);
  for(int i = 0; i <= 1024; ++i) require(gc_ptr_base(ptr + i) == ptr && "gc_ptr_base() on an interior pointer should pass.");
  for(int i = 0; i <= 1024; ++i) require(gc_ptr_base(ptr + 1025 + i) == 0 && "gc_ptr_base() after the end of a ptr range should return 0.");
  for(int i = 0; i <= 1024; ++i) require(gc_ptr_base(ptr - 1 - i) == 0 && "gc_ptr_base() before the start of a ptr range should return 0.");
}
