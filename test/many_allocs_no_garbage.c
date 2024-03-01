// Stress tests performing many allocations that all are retained.
// flags: -sALLOW_MEMORY_GROWTH -sMAXIMUM_MEMORY=4GB -sSPILL_POINTERS -DNDEBUG
#include "test.h"
#include <emscripten/html5.h>
#include <string.h>

size_t malloc_usable_size(void*);

void __attribute__((noinline)) test(uint32_t num)
{
  double t0 = emscripten_performance_now();
  void **allocs = gc_malloc(num*sizeof(void*));
  for(uint32_t i = 0; i < num; ++i)
  {
    allocs[i] = gc_malloc(4);
    if (!allocs[i]) { printf("Allocation failed!\n"); break; }
    memset(allocs[i], 0, malloc_usable_size(allocs[i])); // Clear memory to avoid false GC scans
  }
  double t1 = emscripten_performance_now();
  gc_collect();
  double t2 = emscripten_performance_now();
  uint32_t size = num*4;
  PIN(allocs);
  printf("%u objects: gc_malloc(): %.3f msecs. gc_collect(): %.3f msecs. i.e. marked %.3f MB/second.\n", num, t1-t0, t2-t1, size * 1000.0 / ((t2-t1)*1024*1024));
}

int main()
{
  for(uint32_t num = 1; num <= 67108864; num *= 2)
  {
    test(num);
    gc_collect(); // Free up memory for the next test.
    gc_log("-----------");
  }
}
