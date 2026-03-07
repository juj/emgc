// Tests that gc_ptr_base() returns 0 for an allocation that has been freed
// via gc_free(), both for the base address and for interior offsets.
// flags: -sMALLOC=emmalloc
#include "test.h"

int main()
{
  char *ptr = (char*)gc_malloc(256);
  require(gc_ptr_base(ptr)      == ptr && "gc_ptr_base must return the base of a live allocation.");
  require(gc_ptr_base(ptr + 64) == ptr && "gc_ptr_base must resolve an interior pointer of a live allocation.");

  gc_free(ptr);

  require(gc_ptr_base(ptr)      == 0 && "gc_ptr_base must return 0 for the freed base address.");
  require(gc_ptr_base(ptr + 64) == 0 && "gc_ptr_base must return 0 for an interior pointer after gc_free().");
}
