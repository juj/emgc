// Tests the ABA problem with weak pointers: if an allocation is freed and
// a new allocation reuses the same address, a stale weak pointer should not
// incorrectly resolve to the new allocation.
// flags: -sSPILL_POINTERS -sMALLOC=emmalloc
#include "test.h"

#define NUM_POINTERS 1000
void *weak_ptrs[NUM_POINTERS];

void func_allocate()
{
  // Allocate many pointers of the same size and store their weak references
  for(int i = 0; i < NUM_POINTERS; i++)
  {
    int *ptr = (int*)gc_malloc(sizeof(int));
    *ptr = 10000 + i; // Unique marker value
    weak_ptrs[i] = gc_get_weak_ptr(ptr);
  }
}

void verify_weak_ptrs_all_alive()
{
  for(int i = 0; i < NUM_POINTERS; i++)
  {
    void *acquired = gc_acquire_strong_ptr(&weak_ptrs[i]);
    require(acquired != 0 && "Weak pointer should resolve before GC");
    require(*(int*)acquired == (10000 + i) && "Should get correct object before GC");
  }
}

int main()
{
  // Phase 1: Allocate original objects
  CALL_INDIRECTLY(func_allocate);

  require(gc_num_ptrs() == NUM_POINTERS*2); // For every allocation, must have alloc + weak ref. block alive

  // Verify weak pointers work before GC
  CALL_INDIRECTLY(verify_weak_ptrs_all_alive);

  // Phase 2: Collect everything
  gc_collect();

  require(gc_num_ptrs() == NUM_POINTERS); // After collecting, we only have weak ref. blocks alive

  // Allocate new pointers of the same size - some will likely reuse the freed addresses
  for(int i = 0; i < NUM_POINTERS; i++)
  {
    int *new_ptr = (int*)gc_malloc(sizeof(int));
    PIN(new_ptr);
  }

  // Verify that all weak pointers are stale after GC, and none will point to the newly made allocations
  for(int i = 0; i < NUM_POINTERS; i++)
  {
    void *acquired = gc_acquire_strong_ptr(&weak_ptrs[i]);
    require(acquired == 0 && "Weak pointer should be stale after GC");
  }
}
