// Tests gc_collect_when_stack_is_empty(), which schedules a GC cycle to run
// once the JavaScript event-loop call stack is completely clear.
// At that point no managed pointers can be on any C stack frame, so conservative
// scanning is guaranteed to find nothing on the stack.
// run: browser
#include "test.h"
#include <emscripten.h>
#include <emscripten/eventloop.h>

void verify(void *unused)
{
  require(gc_num_ptrs() == 0 && "GC must have collected the unreachable object once the stack was empty.");
  emscripten_force_exit(0);
}

int main()
{
  // Allocate an unrooted object and immediately discard the pointer.
  // The GC cannot see it on the stack once main() has returned to the event loop.
  gc_malloc(1024);
  require(gc_num_ptrs() == 1);

  gc_collect_when_stack_is_empty();

  // Schedule verification to run after the GC callback. Both have a 0 ms delay,
  // so they fire in FIFO order: GC first, verify second.
  emscripten_set_timeout(verify, 0, 0);

  emscripten_exit_with_live_runtime();
}
