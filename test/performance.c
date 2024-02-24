// Tests how fast the marking process can run on a large allocation array.
// flags: -sALLOW_MEMORY_GROWTH -sMAXIMUM_MEMORY=4GB -sSPILL_POINTERS
#include "test.h"
#include <emscripten/html5.h>

int main()
{
  srand(emscripten_random()*(2048u*1024*1024));
  uint32_t size = 3800ull*1024*1024;
  uint32_t *large = gc_malloc(size);
  require(large != 0);
  for(uint32_t i = 0; i < 900ull*1024*1024; ++i)
    if ((rand() % 10) < 3)
      large[i] = rand();
  double t0 = emscripten_performance_now();
  gc_collect(); // will not scan contents of 'string', so the second allocation should be collected.
  double t1 = emscripten_performance_now();
  printf("gc_collect() took %f msecs. i.e. marked %f MB/second.\n", t1-t0, size * 1000.0 / ((t1-t0)*1024*1024));
}
