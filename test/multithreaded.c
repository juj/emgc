// Tests multithreaded GC allocation in one thread, and collection on another.
// flags: -sBINARYEN_EXTRA_PASSES=--instrument-cooperative-gc,--spill-pointers -sWASM_WORKERS -g2
// run: browser
#include "test.h"
#include <emscripten/wasm_worker.h>

emscripten_wasm_worker_t worker;

_Atomic(int) worker_quit;

void notify_worker_started()
{
  printf("Main thread: notify_worker_started\n");
  for(int i = 0; i < 100; ++i)
  {
    EM_ASM({console.log(`Main thread: collecting`)});
    gc_collect();
    EM_ASM({console.log(`Main thread: collecting done`)});
  }
  worker_quit = 1;
}

void notify_worker_quit()
{
  printf("Main thread: notify_worker_quit\n");
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
    if (gc_mem_prev)
    {
      require(gc_is_ptr(gc_mem_prev));
      require(gc_is_ptr(*gc_mem_prev));
      require(gc_is_ptr(**gc_mem_prev));
    }
    gc_mem_prev = gc_mem;
    EM_ASM({console.log(`Worker thread: allocating memory`)});
    gc_mem = (int ***)gc_malloc(4);
    *gc_mem = (int **)gc_malloc(4);
    **gc_mem = (int *)gc_malloc(4);
    EM_ASM({console.log(`Worker thread: allocated memory`)});
    emscripten_wasm_worker_sleep(10000);
    gc_participate_to_garbage_collection();
  }

  while(!__c11_atomic_load(&worker_quit, __ATOMIC_SEQ_CST))
  {
    emscripten_wasm_worker_sleep(10000);
    gc_participate_to_garbage_collection();
  }
  require(gc_is_ptr(data));

  EM_ASM({console.log(`Worker thread: finished`)});
  emscripten_wasm_worker_post_function_v(0, notify_worker_quit);
  return 0;
}

void worker_main()
{
  gc_enter_fenced_access(work, 0, 0);
}

int main()
{
  worker = emscripten_malloc_wasm_worker(1024);
  emscripten_wasm_worker_post_function_v(worker, worker_main);
}
