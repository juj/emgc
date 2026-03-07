// Tests that gc_is_ptr() returns false for interior (non-base) pointers.
// gc_is_ptr() performs an exact hash-table lookup and only recognises the
// allocation's base address, not any offset into it.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  char *ptr = (char*)gc_malloc(64);
  require(gc_is_ptr(ptr)      && "gc_is_ptr must return true for the base address.");
  require(!gc_is_ptr(ptr + 1) && "gc_is_ptr must return false for offset +1.");
  require(!gc_is_ptr(ptr + 32) && "gc_is_ptr must return false for a mid-allocation offset.");
  require(!gc_is_ptr(ptr + 63) && "gc_is_ptr must return false for the last byte of the allocation.");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect();
  require(gc_num_ptrs() == 0 && "Unreachable allocation must be freed.");
}
