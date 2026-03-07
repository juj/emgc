// Tests gc_is_strong_ptr() and gc_is_weak_ptr() with various pointer types including
// null pointers, stack addresses, and weak vs. strong GC allocations.
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  void *strong = gc_malloc(128);
  void *weak   = gc_get_weak_ptr(strong);
  PIN(strong);
  PIN(weak);

  // Strong pointer: is_strong returns true, is_weak returns false.
  require( gc_is_strong_ptr(strong) && "A strong GC pointer must be identified as strong.");
  require(!gc_is_weak_ptr(strong)   && "A strong GC pointer must not be identified as weak.");

  // Weak pointer: is_weak returns true, is_strong returns false.
  require( gc_is_weak_ptr(weak)     && "A weak GC pointer must be identified as weak.");
  require(!gc_is_strong_ptr(weak)   && "A weak GC pointer must not be identified as strong.");

  gc_make_root(strong);
}

int main()
{
  // Null: both functions must return 0.
  require(!gc_is_strong_ptr(0) && "gc_is_strong_ptr(null) must return 0.");
  require(!gc_is_weak_ptr(0)   && "gc_is_weak_ptr(null) must return 0.");

  // Stack address: neither a strong nor a weak GC pointer.
  int stack_var = 42;
  require(!gc_is_strong_ptr(&stack_var) && "A stack address must not be identified as a strong GC pointer.");
  require(!gc_is_weak_ptr(&stack_var)   && "A stack address must not be identified as a weak GC pointer.");

  CALL_INDIRECTLY(func);
  // strong is a root, so it and the weak ref block remain.
  require(gc_num_ptrs() == 2);
}
