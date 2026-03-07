// Tests gc_wait32() and gc_wait64() return values.
// In single-threaded builds both functions return immediately:
//   1 = not-equal  (value at addr differs from expected)
//   2 = timed-out  (value equals expected; no thread to wake us)
// The gc_wait64 case also guards against a past bug where an
// emscripten_atomic_load_u32 was used instead of _u64, causing the
// not-equal check to miss differences in the upper 32 bits.
// flags: -sSPILL_POINTERS
#include "test.h"
#include <stdint.h>

int main()
{
  // --- gc_wait32 ---
  uint32_t val32 = 42;

  // Value differs from expected -> not-equal (1).
  require(gc_wait32(&val32, 99, 0) == 1 && "gc_wait32: must return 1 when *addr != expected.");

  // Value matches expected -> timed-out (2) in single-threaded mode.
  require(gc_wait32(&val32, 42, 0) == 2 && "gc_wait32: must return 2 when *addr == expected in ST mode.");

  // --- gc_wait64 ---
  uint64_t val64 = 0x100000001ULL; // upper and lower 32-bit halves are both non-zero

  // Completely different value -> not-equal (1).
  require(gc_wait64(&val64, 0, 0) == 1 && "gc_wait64: must return 1 when *addr != expected.");

  // Expected matches only the lower 32 bits (= 1), but the full 64-bit value
  // is 0x100000001 != 1.  Must return 1.  An erroneous u32 load would have
  // read only 1 and incorrectly returned 2.
  require(gc_wait64(&val64, 1, 0) == 1 && "gc_wait64: must use a 64-bit load; upper bits must not be ignored.");

  // Expected matches exactly -> timed-out (2) in single-threaded mode.
  require(gc_wait64(&val64, 0x100000001ULL, 0) == 2 && "gc_wait64: must return 2 when *addr == expected in ST mode.");
}
