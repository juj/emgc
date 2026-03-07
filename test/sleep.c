// Smoke test for gc_sleep().
// Verifies that sleeping does not corrupt GC state, and that a collection
// run after gc_sleep() correctly reclaims unreachable allocations.
// In multithreaded builds gc_sleep() orphans the caller's stack so that
// another thread can scan it; this test also exercises that path (1 ms sleep).
// flags: -sSPILL_POINTERS
#include "test.h"

void func()
{
  gc_malloc(64); // unreachable once func() returns and stack is cleared
}

int main()
{
  CALL_INDIRECTLY(func);

  // A non-zero sleep exercises gc_temporarily_leave_fence / gc_return_to_fence
  // in multithreaded builds, and the busy-wait path in single-threaded builds.
  gc_sleep(1e6); // 1 ms in nanoseconds

  gc_collect();
  require(gc_num_ptrs() == 0 && "Unreachable allocation must be freed after gc_sleep + gc_collect.");
}
