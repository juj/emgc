#ifdef __EMSCRIPTEN_SHARED_MEMORY__
static void mark_from_queue()
{
  for(;;)
  {
    uint32_t tail = queue_tail;
again:
    if (tail >= consumer_head) break;
    void *ptr = mark_queue[tail & MARK_QUEUE_MASK];
    uint32_t actual = cas_u32(&queue_tail, tail, tail+1);
    if (actual != tail) { tail = actual; goto again; }

    mark(ptr, malloc_usable_size(ptr));
  }
  wait_for_all_threads_finished_marking();
}

static void mark_maybe_ptr(void *ptr)
{
  uint32_t i = table_find(ptr);
  if (i == INVALID_INDEX) return;
  uint8_t bit = ((uint8_t)1 << (i&7));
  _Atomic(uint8_t) *marks = (_Atomic(uint8_t)*)mark_table + (i>>3);
  uint8_t old = *marks;
again_bit:
  if ((old & bit)) return; // This pointer is already marked? Then can skip it.
  uint8_t actual = cas_u8(marks, old, old | bit);
  if (old != actual) { old = actual; goto again_bit; } // Some other bit in this byte got flipped by another thread, retry marking this.

  if (HAS_FINALIZER_BIT(table[i])) __c11_atomic_fetch_add((_Atomic uint32_t*)&num_finalizers_marked, 1, __ATOMIC_SEQ_CST);
  if (!HAS_LEAF_BIT(table[i]))
  {
    uint32_t head = producer_head;
again_head:
    if (head >= queue_tail + MARK_QUEUE_MASK) mark(ptr, malloc_usable_size(ptr)); // The shared work queue is full, so mark unshared recursively on local stack
    else
    {
      uint32_t actual = cas_u32(&producer_head, head, head+1);
      if (actual != head) { head = actual; goto again_head; }
      mark_queue[head & MARK_QUEUE_MASK] = ptr;
      while(cas_u32(&consumer_head, head, head+1) != head) ; // nop
    }
  }
}
#else
static void mark_maybe_ptr(void *ptr)
{
  for(uint32_t i = hash_ptr(ptr); table[i]; i = (i+1) & table_mask)
    if (REMOVE_FLAG_BITS(table[i]) == ptr)
    {
      if (BITVEC_GET(mark_table, i)) return;
      BITVEC_SET(mark_table, i);
      num_finalizers_marked += HAS_FINALIZER_BIT(table[i]);
      if (!HAS_LEAF_BIT(table[i])) mark(ptr, malloc_usable_size(ptr));
    }
}
#endif

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
static void mark(void *ptr, size_t bytes)
{
  assert(IS_ALIGNED(ptr, sizeof(void*)));

  const v128_t mem_start = wasm_u32x4_splat((uintptr_t)&__heap_base);
  const v128_t mem_end = wasm_u32x4_splat((uintptr_t)emscripten_get_heap_size() - (uintptr_t)&__heap_base);
  const v128_t align_mask = wasm_u32x4_const_splat((uintptr_t)7);
  const v128_t zero = wasm_u32x4_const_splat((uintptr_t)0);

  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; p += 4)
  {
    v128_t ptrs = wasm_i32x4_sub(wasm_v128_load(p), mem_start);
    v128_t cmp = wasm_v128_and(wasm_u32x4_lt(ptrs, mem_end), wasm_i32x4_eq(wasm_v128_and(ptrs, align_mask), zero));
    if (wasm_v128_any_true(cmp))
      for(uint32_t bits = wasm_i32x4_bitmask(cmp), offset; bits; bits ^= 1 << offset)
        mark_maybe_ptr(p[(offset = __builtin_ctz(bits))]);
  }
}
#else
static void mark(void *ptr, size_t bytes)
{
  uint32_t i;
  assert(IS_ALIGNED(ptr, sizeof(void*)));
  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; ++p)
    mark_maybe_ptr(*p);
}
#endif
