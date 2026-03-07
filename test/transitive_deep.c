// Tests deep (3-level) transitive reachability: only A is directly accessible,
// but B and C must stay alive because A -> B -> C.
// flags: -sSPILL_POINTERS
#include "test.h"

void **global_a;

void func()
{
  void **a = (void**)gc_malloc(sizeof(void*));
  void **b = (void**)gc_malloc(sizeof(void*));
  void **c = (void**)gc_malloc(sizeof(void*));
  *a = b;
  *b = c;
  *c = 0;
  global_a = a;
  PIN(&global_a);
  require(gc_num_ptrs() == 3);
  gc_collect();
  require(gc_num_ptrs() == 3 && "A, B, and C must all survive while A is reachable.");
}

void func_clear()
{
  global_a = 0;
}

int main()
{
  CALL_INDIRECTLY(func);

  // Null the global anchor so A, B, and C all become unreachable.
  CALL_INDIRECTLY(func_clear);
  gc_collect();
  require(gc_num_ptrs() == 0 && "All three objects must be collected once A is unreachable.");
}
