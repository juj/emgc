// Tests multithreaded GC allocation in one thread, and collection on another.
// flags: -sSPILL_POINTERS -sCOOPERATIVE_GC -sWASM_WORKERS -g2
// run: browser
#include "test.h"
#include <emscripten/wasm_worker.h>
#include <emscripten/eventloop.h>

emscripten_wasm_worker_t worker;

_Atomic(int) worker_quit;

void collect_periodically(void *unused)
{
  uint32_t ptrs_before = gc_num_ptrs();
  gc_collect();
  uint32_t ptrs_after = gc_num_ptrs();
  EM_ASM({console.log(`Main thread: Freed ${$0} ptrs (down to ${$1})`)}, ptrs_before - ptrs_after, ptrs_after);
  emscripten_set_timeout(collect_periodically, 100, 0);
}

void notify_worker_started()
{
  collect_periodically(0);
}

void notify_worker_quit()
{
  emscripten_terminate_wasm_worker(worker);
}

void *work(void *user1, void *user2)
{
  EM_ASM({console.log(`Worker thread: work`)});

  char *data = gc_malloc(4);
  PIN(&data);

  emscripten_wasm_worker_post_function_v(0, notify_worker_started);

  int ***gc_mem = 0, ***gc_mem_prev = 0;
  while(!__c11_atomic_load(&worker_quit, __ATOMIC_SEQ_CST))
  {
    gc_sleep(10000000);
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

  EM_ASM({console.log(`Worker thread: finished`)});
  emscripten_wasm_worker_post_function_v(0, notify_worker_quit);
  return 0;
}

void worker_main()
{
  gc_enter_fence_cb(work, 0, 0);
}

int main()
{
  worker = emscripten_malloc_wasm_worker(1024);
  emscripten_wasm_worker_post_function_v(worker, worker_main);
}
