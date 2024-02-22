#include <wasm_simd128.h>

static void mark(void *ptr, size_t bytes)
{
  EM_ASM({console.log(`Marking ptr range ${$0.toString(16)} - ${$1.toString(16)} (${$2} bytes)...`)}, ptr, (char*)ptr + bytes, bytes);
  assert(IS_ALIGNED(ptr, sizeof(void*)));
  assert(IS_ALIGNED((uintptr_t)ptr + bytes, sizeof(void*)));

  const v128_t heap_start = wasm_u32x4_splat((uintptr_t)&__heap_base);
  const v128_t heap_end = wasm_u32x4_splat((uintptr_t)&__heap_base + emscripten_get_heap_size());
  const v128_t bitmask = wasm_u32x4_const_splat((uintptr_t)7);
  const v128_t zero = wasm_u32x4_const_splat((uintptr_t)0);

  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; p += 4)
  {
    v128_t ptrs = wasm_v128_load(p);
    v128_t cmp = wasm_u32x4_gt(ptrs, heap_start);
    cmp = wasm_v128_and(cmp, wasm_u32x4_lt(ptrs, heap_end));
    cmp = wasm_v128_and(cmp, wasm_i32x4_eq(wasm_v128_and(ptrs, bitmask), zero));
    if (!wasm_v128_any_true(cmp)) continue;

    uint32_t bits = wasm_i32x4_bitmask(cmp);
    while(bits)
    {
      int offset = __builtin_ctz(bits);
      bits = (bits >> offset) ^ 1;
      void *ptr = p[offset];
      for(uint32_t i = hash_ptr(ptr); table[i]; i = (i+1) & table_mask)
        if (REMOVE_FLAG_BITS(table[i]) == ptr)
        {
          if (i != (uint32_t)-1 && BITVEC_GET(mark_table, i))
          {
            EM_ASM({console.log(`Marked ptr ${$0.toString(16)} at index ${$1} from memory address ${$2.toString(16)}.`)}, *p, i, p);
            BITVEC_CLEAR(mark_table, i);
            if (((uintptr_t)table[i] & PTR_LEAF_BIT) == 0)
              mark(*p, malloc_usable_size(*p));
          }
          break;
        }
    }
  }
}