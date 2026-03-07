// Tests that gc_enter_fence_cb() passes user1 and user2 to the callback
// unchanged, and that the callback's return value is forwarded to the caller.
// The existing fenced.c test calls with (work, 0, 0) and ignores the return;
// this test exercises the non-null argument and return-value paths.
// flags: -DEMGC_FENCED=1 -sSPILL_POINTERS
#include "test.h"

void *callback(void *user1, void *user2)
{
  require(user1 == (void*)42 && "user1 must be forwarded unchanged.");
  require(user2 == (void*)99 && "user2 must be forwarded unchanged.");
  return (void*)123;
}

int main()
{
  void *ret = gc_enter_fence_cb(callback, (void*)42, (void*)99);
  require(ret == (void*)123 && "gc_enter_fence_cb must return the callback's return value.");
}
