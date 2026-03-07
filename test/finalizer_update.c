// Tests that calling gc_register_finalizer() a second time on the same allocation
// replaces the existing finalizer rather than adding a second one.
// Only the most recently registered finalizer must run on collection.
// flags: -sSPILL_POINTERS
#include "test.h"

int finalizer_a_ran = 0;
int finalizer_b_ran = 0;
void finalizer_a(void *ptr) { finalizer_a_ran = 1; }
void finalizer_b(void *ptr) { finalizer_b_ran = 1; }

void func()
{
  void *ptr = gc_malloc(64);
  gc_register_finalizer(ptr, finalizer_a);
  gc_register_finalizer(ptr, finalizer_b); // Must replace finalizer_a, not add another.
  require(gc_get_finalizer(ptr) == finalizer_b && "Re-registration must update the stored finalizer.");
}

int main()
{
  CALL_INDIRECTLY(func);

  gc_collect(); // Runs the single registered finalizer (finalizer_b).
  require( finalizer_b_ran && "The most recently registered finalizer must run on collection.");
  require(!finalizer_a_ran && "The overwritten finalizer must not run.");

  gc_collect(); // Frees the finalized object.
  require(gc_num_ptrs() == 0 && "Object must be freed after finalization.");
}
