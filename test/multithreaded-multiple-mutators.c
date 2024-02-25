// Tests multithreaded GC allocation in one thread, and collection on another.
// flags: -sSPILL_POINTERS -sCOOPERATIVE_GC -sWASM_WORKERS -g2
// run: browser
#include "test.h"
#include <emscripten/wasm_worker.h>
#include <emscripten/eventloop.h>
#include <emscripten/html5.h>

#define NT 16
emscripten_wasm_worker_t worker[NT];

_Atomic(int) worker_quit;

void collect_periodically(void *unused)
{
  uint32_t ptrs_before = gc_num_ptrs();

  // Benchmark collection speed.
  double t0 = emscripten_performance_now();
  gc_collect();
  double t1 = emscripten_performance_now();

  // Collect back-to-back a couple of times to stress test the scenario
  // when a new GC is invoked immediately after a previous one finishes, to verify
  // that the worker threads that are resuming execution after previous GC won't
  // go out of sync.
  for(int i = 0; i < 3; ++i) gc_collect();

  uint32_t ptrs_after = gc_num_ptrs();
  gc_log("Freed %d ptrs (down to %d). Collect took %f msecs.", ptrs_before - ptrs_after, ptrs_after, t1-t0);
  emscripten_set_timeout(collect_periodically, 100, 0);
}

void notify_worker_quit()
{
  for(int i = 0; i < NT; ++i)
    emscripten_terminate_wasm_worker(worker[i]);
}

void *work(void *user1, void *user2)
{
  int ***gc_mem = 0, ***gc_mem_prev = 0;
  while(!__c11_atomic_load(&worker_quit, __ATOMIC_SEQ_CST))
  {
    gc_sleep(10 * 1000000);
    if (gc_mem_prev)
    {
      require(gc_is_ptr(gc_mem_prev));
      require(gc_is_ptr(*gc_mem_prev));
      require(gc_is_ptr(**gc_mem_prev));
    }
    gc_mem_prev = gc_mem;
    gc_mem = (int ***)gc_malloc(4);
    *gc_mem = (int **)gc_malloc(4);
    **gc_mem = (int *)gc_malloc(4);
  }
  emscripten_wasm_worker_post_function_v(0, notify_worker_quit);
  return 0;
}

void worker_main()
{
  gc_enter_fence_cb(work, 0, 0);
}

int main()
{
  for(int i = 0; i < NT; ++i)
  {
    worker[i] = emscripten_malloc_wasm_worker(1024);
    emscripten_wasm_worker_post_function_v(worker[i], worker_main);
  }
  collect_periodically(0);
}
