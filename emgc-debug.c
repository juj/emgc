uint32_t gc_num_ptrs()
{
  return num_allocs;
}

void gc_dump()
{
  if (table)
    for(uint32_t i = 0; i <= table_mask; ++i)
      if (table[i] > SENTINEL_PTR) EM_ASM({console.log(`Table index ${$0}: 0x${$1.toString(16)}`);}, i, table[i]);
  EM_ASM({console.log(`${$0} allocations total, ${$1} used table entries. Table size: ${$2}`);}, num_allocs, num_table_entries, table_mask+1);
}
