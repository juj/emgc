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
    if (!wasm_v128_any_true(cmp)) continue;

    for(uint32_t bits = wasm_i32x4_bitmask(cmp), offset; bits; bits ^= 1 << offset)
    {
      void *pp = p[(offset = __builtin_ctz(bits))];
      for(uint32_t i = hash_ptr(pp); table[i]; i = (i+1) & table_mask)
        if (REMOVE_FLAG_BITS(table[i]) == pp)
        {
          if (!BITVEC_GET(mark_table, i))
          {
            BITVEC_SET(mark_table, i);
            num_finalizers_marked += HAS_FINALIZER_BIT(table[i]);
            if (!HAS_LEAF_BIT(table[i])) mark(pp, malloc_usable_size(pp));
          }
          break;
        }
    }
  }
}
