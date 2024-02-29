// Stress tests performing many temporary allocations that all immediately turn into garbage.
// flags: -sALLOW_MEMORY_GROWTH -sMAXIMUM_MEMORY=4GB -sSPILL_POINTERS -DNDEBUG
#include "test.h"
#include <emscripten/html5.h>
#include <string.h>

size_t malloc_usable_size(void*);

void __attribute__((noinline)) alloc(uint32_t num)
{
  while(num--)
  {
    void *ptr = gc_malloc(4);
    memset(ptr, 0, malloc_usable_size(ptr)); // Clear memory to avoid false GC scans
  }
}

int main()
{
  for(uint32_t num = 1; num <= 67108864; num *= 2)
  {
    double t0 = emscripten_performance_now();
    alloc(num);
    double t1 = emscripten_performance_now();
    gc_collect();
    double t2 = emscripten_performance_now();
    uint32_t size = num*4;
    printf("%u objects: gc_malloc(): %.3f msecs. gc_collect(): %.3f msecs. i.e. marked %.3f MB/second. %u objects left.\n", num, t1-t0, t2-t1, size * 1000.0 / ((t1-t0)*1024*1024), gc_num_ptrs());
    gc_log("-----------");
  }
}
