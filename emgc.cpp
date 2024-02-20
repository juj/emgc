#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <stdio.h> // xxxxxxxxxxxxxx remove
#include <emscripten/stack.h>
#include <emscripten/heap.h>

#define BITVEC_GET(arr, i)  (((arr)[(i)>>3] &    1<<((i)&7)) != 0)
#define BITVEC_SET(arr, i)   ((arr)[(i)>>3] |=   1<<((i)&7))
#define BITVEC_CLEAR(arr, i) ((arr)[(i)>>3] &= ~(1<<((i)&7)))
#define VALID_TABLE_ENTRY(entry) ((uintptr_t)((entry).ptr) > 1)
#define IS_ALIGNED(ptr, size) (((uintptr_t)(ptr) & ((size)-1)) == 0)

extern "C" size_t malloc_usable_size(void*);
extern "C" char __global_base, __data_end, __heap_base;

static struct gc_alloc
{
  void *ptr;
} *table;

static uint8_t *mark_table, *used_table;
static uint32_t num_allocs, num_table_entries, table_size, table_mask;

static uint32_t hash_ptr(void *ptr) { return (uint32_t)((uintptr_t)ptr >> 3) & table_mask; }

static uint32_t find_insert_index(void *ptr)
{
  uint32_t i = hash_ptr(ptr);
  uint32_t i64 = i>>6;
  uint64_t *used64 = (uint64_t*)used_table;
  uint64_t u = used64[i64] | ((1ull<<(i&63))-1); // Mask off all indices that come before the initial hash index
  while(u == (uint64_t)-1)
    u = used64[(i64 = (i64+1) & (table_mask >> 6))];
  return (i64<<6) + __builtin_ctzll(~u);
}

static uint32_t find_index(void *ptr)
{
  if (ptr < &__heap_base || !IS_ALIGNED(ptr, 8) || (uintptr_t)ptr >= (uintptr_t)&__heap_base + emscripten_get_heap_size()) return (uint32_t)-1;
  for(uint32_t i = hash_ptr(ptr); VALID_TABLE_ENTRY(table[i]); i = (i+1) & table_mask)
    if (table[i].ptr == ptr) return i;
  return (uint32_t)-1;
}

static void realloc_table()
{
  uint32_t old_size = table_size;
  if (2*num_allocs >= table_size)
  {
    table_mask = table_mask ? ((table_mask << 1) | 1) : 63;
    table_size = table_mask+1;

    free(mark_table);
    mark_table = (uint8_t*)malloc((table_size>>3)*sizeof(uint8_t));
  }
  else if (table_size > 8*num_allocs)
  {
    while(table_mask+1 > 4*num_allocs && table_mask >= 127) table_mask >>= 1; // TODO: Replace while loop with a __builtin_clz() call
    table_size = table_mask+1;

    free(mark_table);
    mark_table = (uint8_t*)malloc((table_size>>3)*sizeof(uint8_t));
  }

  gc_alloc *old_table = table;
  table = (gc_alloc*)calloc(table_size, sizeof(gc_alloc));

  uint64_t *old_used_table = (uint64_t *)used_table;
  used_table = (uint8_t*)calloc(table_size>>3, sizeof(uint8_t));

  for(uint32_t i64 = 0; i64 < (old_size>>6); ++i64)
  {
    uint64_t bits = old_used_table[i64];
    uint32_t i = (i64<<6);
    while(bits)
    {
      int offset = __builtin_ctzll(bits);
      i += offset;
      bits = (bits >> offset) ^ 1;

      uint32_t new_index = find_insert_index(old_table[i].ptr);
      table[new_index] = old_table[i];
      BITVEC_SET(used_table, new_index);
    }
  }

  free(old_table);
  free(old_used_table);
  num_table_entries = num_allocs; // The hash table is tight again now with no dirty entries.
}

extern "C" void *gc_malloc(size_t bytes)
{
  void *ptr = malloc(bytes);
  if (!ptr) return 0;
  ++num_allocs;
  ++num_table_entries;
  if (2*num_table_entries >= table_size) realloc_table();
  uint32_t i = find_insert_index(ptr);
  table[i].ptr = ptr;
  BITVEC_SET(used_table, i);
  printf("gc_malloc: Allocated ptr %p\n", ptr);
  return ptr;
}

static void free_at_index(uint32_t i)
{
//  printf("Freeing managed ptr %p at index %u\n", table[i].ptr, i);
  assert(VALID_TABLE_ENTRY(table[i]));
  free(table[i].ptr);
  table[i].ptr = (void*)1;
  BITVEC_CLEAR(used_table, i);
  --num_allocs;  
}

extern "C" void gc_free(void *ptr)
{
  if (!ptr) return;
  uint32_t i = find_index(ptr);
  if (i == (uint32_t)-1) return;
  free_at_index(i);
}

static void mark(void *ptr, size_t bytes)
{
  printf("Marking ptr range %p - %p (%zu bytes)...\n", ptr, (char*)ptr + bytes, bytes);
  assert(IS_ALIGNED(ptr, sizeof(void*)));
  assert(IS_ALIGNED((uintptr_t)ptr + bytes, sizeof(void*)));
  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; ++p)
  {
    uint32_t i = find_index(*p);
    if (i != (uint32_t)-1 && BITVEC_GET(mark_table, i))
    {
      printf("Marked ptr %p at index %u from memory address %p.\n", *p, i, p);
      BITVEC_CLEAR(mark_table, i);
      mark(*p, malloc_usable_size(*p));
    }
  }
}

static void sweep()
{
  uint64_t *marks = (uint64_t*)mark_table;
  for(uint32_t i64 = 0; i64 < (table_size>>6); ++i64)
  {
    uint64_t bits = marks[i64];
    uint32_t i = (i64<<6);
    while(bits)
    {
      int offset = __builtin_ctzll(bits);
      i += offset;
      bits = (bits >> offset) ^ 1;
      free_at_index(i);
    }
  }
}

extern "C" void gc_collect()
{
  memcpy(mark_table, used_table, table_size>>3);

  printf("Marking static data.\n");
  mark(&__global_base, (uintptr_t)&__data_end - (uintptr_t)&__global_base);

  printf("Marking stack.\n");
  uintptr_t stack_bottom = emscripten_stack_get_current();
  mark((void*)stack_bottom, emscripten_stack_get_base() - stack_bottom);

  printf("Sweeping.\n");
  sweep();

  // Compactify managed allocation array if it is now overly large to fit all allocations.
  if (table_size > 8*num_allocs && table_size >= 128) realloc_table();
}

extern "C" void gc_dump()
{
  for(uint32_t i = 0; i < table_size; ++i)
    if (VALID_TABLE_ENTRY(table[i]))
      printf("Table %u: %p\n", i, table[i].ptr);
  printf("%u allocations total, %u used table entries. Table size: %u\n", num_allocs, num_table_entries, table_size);
}

extern "C" uint32_t gc_num_ptrs()
{
  return num_allocs;
}
