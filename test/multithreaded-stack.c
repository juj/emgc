// This test verifies that a GC pointer that is allocated on the stack of a
// Wasm Worker will survive gc_collect() from the main thread.
#include "test.h"
#include <emscripten/wasm_worker.h>

emscripten_wasm_worker_t worker;

_Atomic(int) worker_quit;

void worker_has_started()
{
  gc_collect();
  worker_quit = 1;
}

void *work(void *user1, void *user2)
{
  char *data = gc_malloc(4); // This allocation should stay alive
  EM_ASM({console.log(`Ptr 0x${$0.toString(16)} allocated on worker thread, should stay alive in collection.`)}, data);

  emscripten_wasm_worker_post_function_v(0, worker_has_started);

  while(!worker_quit)
  {
    emscripten_wasm_worker_sleep(10000);
    gc_participate_to_garbage_collection(); // TODO: Remove this after Binaryen codegen magic
  }
  require(gc_is_ptr(data) && "data pointer on the local stack of a Wasm Worker should not have gotten garbage collected.");
  require(gc_num_ptrs() == 1 && "There should only remain one allocation from the Wasm Worker thread alive.");
  exit(0);
  return 0;
}

void worker_main()
{
  gc_enter_fenced_access(work, 0, 0);
}

void func()
{
  char *ptr = (char*)gc_malloc(1024); // this ptr should be getting collected.
  EM_ASM({console.log(`Ptr 0x${$0.toString(16)} allocated on main thread, should be freed in collection.`)}, ptr);
}

int main()
{
  CALL_INDIRECTLY(func);
  worker = emscripten_malloc_wasm_worker(1024);
  emscripten_wasm_worker_post_function_v(worker, worker_main);
}
