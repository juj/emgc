// Tests that gc_acquire_strong_ptr() can be called multiple times on the same weak pointer.
// Each call should succeed and return the same pointer (if not yet collected).
// flags: -sSPILL_POINTERS
#include "test.h"

void *weak_global;

void func()
{
  void *strong = gc_malloc(1024);
  weak_global = gc_get_weak_ptr(strong);

  PIN(&weak_global);

  // First acquire should succeed.
  void *first = gc_acquire_strong_ptr(&weak_global);
  require(first != 0 && "First acquire should succeed");
  require(gc_is_strong_ptr(first) && "Should return a strong pointer");

  // Second acquire on the same weak pointer should also succeed and return the same pointer.
  void *second = gc_acquire_strong_ptr(&weak_global);
  require(second != 0 && "Second acquire should also succeed");
  require(gc_is_strong_ptr(second) && "Should return a strong pointer");
  require(first == second && "Multiple acquires should return the same pointer");

  // Third acquire to verify consistency.
  void *third = gc_acquire_strong_ptr(&weak_global);
  require(third != 0 && "Third acquire should also succeed");
  require(third == first && "All acquires should return identical pointers");
}

int main()
{
  CALL_INDIRECTLY(func);

  // The object should still be alive due to the strong references from the acquires.
  require(gc_num_ptrs() == 2 && "Object should still be alive after multiple strong pointer acquisitions");

  gc_collect();

  // After collection, the weak pointer is now stale since we released our strong reference.
  void *stale = gc_acquire_strong_ptr(&weak_global);
  require(stale == 0 && "Acquiring from a collected weak pointer should return NULL");
}
