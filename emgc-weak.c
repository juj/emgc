int gc_is_weak_ptr(void *ptr)
{
  return !ptr || !IS_ALIGNED(ptr, 8);
}

int gc_is_strong_ptr(void *ptr)
{
  return IS_ALIGNED(ptr, 8);
}

void *gc_get_weak_ptr(void *strong_ptr)
{
  if (gc_is_weak_ptr(strong_ptr)) return strong_ptr; // Already a weak ptr?
  return (void*)((uintptr_t)strong_ptr - 1);
}

void *gc_acquire_strong_ptr(void *weak_ptr)
{
  if (gc_is_strong_ptr(weak_ptr)) return weak_ptr; // Already a strong ptr?
  void *strong_ptr = (void*)((uintptr_t)weak_ptr + 1);
  return (find_index(strong_ptr) == (uint32_t)-1) ? 0 : strong_ptr;
}

int gc_weak_ptr_equals(void *weak_ptr1, void *weak_ptr2)
{
  if (weak_ptr1 == weak_ptr2) return 1;
  return gc_acquire_strong_ptr(weak_ptr1) == gc_acquire_strong_ptr(weak_ptr2);
}
