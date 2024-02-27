// emgc-sleep.c implements support for futex waiting and sleeping via a "stack orphaning" technique.
// Under this scheme, when a thread that is executing inside the GC fence (so has possibly managed ptrs on its stack)
// needs to perform a futex wait or a sleep, it can temporarily leave the GC fence
// by calling gc_temporarily_leave_fence(), even when it has GC pointers on its stack.
// Then, when the thread has concluded the futex/sleep operation, it will re-enter
// the fence by calling gc_return_to_fence().
// Leaving and re-entering the fence will "orphan" the caller's stack that may contain managed GC pointers.
// When the stack is orphaned, it is placed in a global "orphaned stacks" data structure.
// If a GC operation arrives while the stack is orphaned, **some other thread** will mark the orphaned stack
// on behalf of the thread that stepped out of the fence.

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
static emscripten_lock_t orphan_stack_lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;
typedef struct range { void *start, *end; } range;
static range *orphan_stacks;
static uint16_t orphan_stack_size, orphan_stack_cap;
static __thread uint16_t my_orphan_stack_pos;
#endif

void gc_temporarily_leave_fence()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (!this_thread_accessing_managed_state) return;

  gc_acquire_lock(&orphan_stack_lock);
  if (orphan_stack_size >= orphan_stack_cap) orphan_stacks = (range*)realloc(orphan_stacks, (orphan_stack_cap = orphan_stack_cap*2 + 1)*sizeof(range));
  orphan_stacks[orphan_stack_size].start = (void*)emscripten_stack_get_current();
  orphan_stacks[orphan_stack_size].end = (void*)stack_top;
  my_orphan_stack_pos = orphan_stack_size++;
  gc_release_lock(&orphan_stack_lock);

  gc_participate_to_garbage_collection();
  // --num_threads_accessing_managed_state; // xxx todo: this is currently not working
#endif
}

void gc_return_to_fence()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (!this_thread_accessing_managed_state) return;

  // ++num_threads_accessing_managed_state; // xxx todo: this is currently not working
  gc_participate_to_garbage_collection();

  gc_acquire_lock(&orphan_stack_lock);
  if (my_orphan_stack_pos < --orphan_stack_size) orphan_stacks[my_orphan_stack_pos] = orphan_stacks[orphan_stack_size];
  gc_release_lock(&orphan_stack_lock);
#endif
}

static void mark_orphaned_stacks()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  gc_acquire_lock(&orphan_stack_lock);
  for(range *r = orphan_stacks; r < orphan_stacks + orphan_stack_size; ++r)
    mark(r->start, (uintptr_t)r->end - (uintptr_t)r->start);
  gc_release_lock(&orphan_stack_lock);
#endif
}

static void gc_uninterrupted_sleep(double nsecs)
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (emscripten_current_thread_is_wasm_worker()) { int32_t dummy = 0; __builtin_wasm_memory_atomic_wait32(&dummy, 0, nsecs); }
  else
#endif
    for(double end = emscripten_performance_now() + nsecs/1000000.0; emscripten_performance_now() < end;) ; // nop
}

// TODO: attribute(noinline) doesn't seem to prevent this function from not being inlined. Adding EMSCRIPTEN_KEEPALIVE seems to help, but is excessive.
void EMSCRIPTEN_KEEPALIVE __attribute__((noinline)) gc_sleep(double nsecs)
{
  // Sliced sleep: suffers from performance problems. :(
//  for(double end = emscripten_performance_now() + nsecs/1000000.0; emscripten_performance_now() < end;) gc_uninterrupted_sleep(100);

  // Orphaned stack sleep: let another thread scan our stack while we are sleeping.
  // if we're sleeping more than 0.1 msecs, then give the stack of the current thread for someone else to scan, and only then sleep.
  if (nsecs > 100*1000) gc_temporarily_leave_fence();
  gc_uninterrupted_sleep(nsecs);
  if (nsecs > 100*1000) gc_return_to_fence();
}

int gc_wait32(void *addr __attribute__((nonnull)), uint32_t expected, int64_t nsecs)
{
  if (*(int32_t*)addr != expected) return 1; // not-equal

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (nsecs > 100*1000) gc_temporarily_leave_fence();
  int ret = __builtin_wasm_memory_atomic_wait32((int32_t*)addr, expected, nsecs);
  if (nsecs > 100*1000) gc_return_to_fence();
  return ret;
#else
  return 2; // timed-out
#endif
}

int gc_wait64(void *addr __attribute__((nonnull)), uint64_t expected, int64_t nsecs)
{
  if (*(int64_t*)addr != expected) return 1; // not-equal

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (nsecs > 100*1000) gc_temporarily_leave_fence();
  int ret = __builtin_wasm_memory_atomic_wait64((int64_t*)addr, expected, nsecs);
  if (nsecs > 100*1000) gc_return_to_fence();
  return ret;
#else
  return 2; // timed-out
#endif
}
