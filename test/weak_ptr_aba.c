// Tests the ABA problem with weak pointers: if an allocation is freed and
// a new allocation reuses the same address, a stale weak pointer should not
// incorrectly resolve to the new allocation.
// flags: -sSPILL_POINTERS -sMALLOC=emmalloc
#include "test.h"

#define NUM_ATTEMPTS 100

void *weak_ptrs[NUM_ATTEMPTS];
uintptr_t original_addrs[NUM_ATTEMPTS];
int *marker_values[NUM_ATTEMPTS];

void func_allocate()
{
  // Allocate many pointers of the same size and store their weak references
  // Also write a unique marker value into each allocation
  for (int i = 0; i < NUM_ATTEMPTS; i++)
  {
    int *ptr = (int*)gc_malloc(128);
    *ptr = i + 1000; // Unique marker value
    marker_values[i] = (int*)ptr;
    // Store bit-reversed address to hide it from GC scanning
    original_addrs[i] = ~(uintptr_t)ptr;
    weak_ptrs[i] = gc_get_weak_ptr(ptr);
  }

  PIN(weak_ptrs);
  PIN(original_addrs);
  PIN(marker_values);
}

void func_reallocate()
{
  // Allocate new pointers of the same size - some will likely reuse the freed addresses
  for (int i = 0; i < NUM_ATTEMPTS; i++)
  {
    int *new_ptr = (int*)gc_malloc(128);
    *new_ptr = i + 2000; // Different marker value for new allocations

    // Check if this new allocation reused any of the old addresses
    for (int j = 0; j < NUM_ATTEMPTS; j++)
    {
      // Reverse the stored bit-reversed address to get original address
      if ((uintptr_t)new_ptr == ~original_addrs[j])
      {
        // ABA scenario detected! The old weak pointer should return NULL,
        // NOT the new allocation at the same address
        void *acquired = gc_acquire_strong_ptr(weak_ptrs[j]);

        if (acquired != 0)
        {
          // If acquired is non-null, verify it's truly the wrong object by checking the marker
          int marker = *(int*)acquired;
          if (marker != (j + 1000))
          {
            // The marker doesn't match the original allocation!
            // This is the ABA bug: we got back a different object
            require(0 && "ABA problem detected: stale weak pointer resolved to new allocation at same address");
          }
        }

        require(acquired == 0 && "Stale weak pointer should not resolve to new allocation at same address (ABA problem)");
      }
    }

    gc_make_root(new_ptr); // Keep these alive
  }
}

int main()
{
  // Phase 1: Allocate original objects
  CALL_INDIRECTLY(func_allocate);

  require(gc_num_ptrs() == NUM_ATTEMPTS);

  // Verify weak pointers work before GC
  for (int i = 0; i < NUM_ATTEMPTS; i++)
  {
    void *acquired = gc_acquire_strong_ptr(weak_ptrs[i]);
    require(acquired != 0 && "Weak pointer should resolve before GC");
    require(*(int*)acquired == (i + 1000) && "Should get correct object before GC");
  }

  // Phase 2: Collect everything
  gc_collect();
  require(gc_num_ptrs() == 0 && "All allocations should be collected");

  // Verify weak pointers are stale after GC
  for (int i = 0; i < NUM_ATTEMPTS; i++)
  {
    void *acquired = gc_acquire_strong_ptr(weak_ptrs[i]);
    require(acquired == 0 && "Weak pointer should be stale after GC");
  }

  // Phase 3: Reallocate - try to trigger address reuse
  CALL_INDIRECTLY(func_reallocate);

  require(gc_num_ptrs() == NUM_ATTEMPTS && "Should have new allocations");
}
