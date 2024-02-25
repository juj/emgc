#include <emscripten/wasm_worker.h>
#include <emscripten/html5.h>

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
// In multithreaded builds, use a simple global spinlock strategy to acquire/release access to the memory allocator.
static volatile uint8_t mt_lock = 0;
#define GC_MALLOC_ACQUIRE() while (__sync_lock_test_and_set(&mt_lock, 1)) { while (mt_lock) { ; } } // nop
#define GC_MALLOC_RELEASE() __sync_lock_release(&mt_lock)
// Test code to ensure we have tight malloc acquire/release guards in place.
#define ASSERT_GC_MALLOC_IS_ACQUIRED() assert(mt_lock == 1)
#define GC_CHECKPOINT_KEEPALIVE EMSCRIPTEN_KEEPALIVE __attribute__((noinline))
static uint8_t  cas_u8( _Atomic(uint8_t) *addr,  uint8_t prev,  uint8_t new)  { __c11_atomic_compare_exchange_strong(addr, &prev, new, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return prev; }
static uint32_t cas_u32(_Atomic(uint32_t) *addr, uint32_t prev, uint32_t new) { __c11_atomic_compare_exchange_strong(addr, &prev, new, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return prev; }
#else
// In singlethreaded builds, no need for locking.
#define GC_MALLOC_ACQUIRE() ((void)0)
#define GC_MALLOC_RELEASE() ((void)0)
#define ASSERT_GC_MALLOC_IS_ACQUIRED() ((void)0)
#define GC_CHECKPOINT_KEEPALIVE
#endif

#if defined(__EMSCRIPTEN_SHARED_MEMORY__) || defined(EMGC_FENCED)
#define ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED() assert(this_thread_accessing_managed_state && "In fenced build mode, GC state must be only accessed from inside gc_enter_fence() scope.")
#else
#define ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED() ((void)0)
#endif

static void sweep();
static void mark_from_queue();
static void mark_current_thread_stack();
static void mark(void *ptr, size_t bytes);
static uint32_t find_index(void *ptr);

static _Atomic(int) num_threads_accessing_managed_state, mt_marking_running, num_threads_ready_to_start_marking, num_threads_finished_marking, num_threads_resumed_execution;
static __thread int this_thread_accessing_managed_state;
static __thread uintptr_t stack_top;
static void **mark_array;
static _Atomic(uint32_t) producer_head, producer_tail, consumer_head, consumer_tail;

static void gc_uninterrupted_sleep(double nsecs)
{
#if defined(__EMSCRIPTEN_SHARED_MEMORY__)
  if (emscripten_current_thread_is_wasm_worker()) { int32_t dummy = 0; __builtin_wasm_memory_atomic_wait32(&dummy, 0, nsecs); }
  else
#endif
    for(double end = emscripten_performance_now() + nsecs/1000000.0; emscripten_performance_now() < end;) ; // nop
}

// TODO: attribute(noinline) doesn't seem to prevent this function from not being inlined. Adding EMSCRIPTEN_KEEPALIVE seems to help, but is excessive.
void EMSCRIPTEN_KEEPALIVE __attribute__((noinline)) gc_sleep(double nsecs)
{
  for(double end = emscripten_performance_now() + nsecs/1000000.0; emscripten_performance_now() < end;) gc_uninterrupted_sleep(100);
}

static void wait_for_all_participants()
{
  // Wait for all threads currently executing in managed context to gather up together for the collection.
  while(num_threads_ready_to_start_marking < num_threads_accessing_managed_state) gc_uninterrupted_sleep(1);
}

// Mark as keepalive to make sure it exists in the generated Module so that the
// --instrument-cooperative-gc Binaryen pass can find it. (TODO: This function shouldn't be exported out to JS)
void GC_CHECKPOINT_KEEPALIVE gc_participate_to_garbage_collection()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (mt_marking_running && this_thread_accessing_managed_state)
  {
    ++num_threads_ready_to_start_marking;
    wait_for_all_participants();
    mark_current_thread_stack();
    mark_from_queue();
  }
#endif
}

static void gc_enter_fence()
{
  if (!this_thread_accessing_managed_state++)
  {
    // Record where the stack is currently at. Any functions before this cannot
    // contain GC pointers, so this is an easy micro-optimization to shrink
    // the amount of local stack scanning.
    stack_top = emscripten_stack_get_current();
    ++num_threads_accessing_managed_state;
  }

  // If there is a current GC collection going, help out the GC collection as
  // the first thing we do, or otherwise we cannot safely access any GC objects.
  gc_participate_to_garbage_collection();
}

static void gc_exit_fence()
{
  if (!--this_thread_accessing_managed_state) --num_threads_accessing_managed_state;
}

void *js_try_finally(gc_mutator_func func, void *user1, void *user2, void (*finally_func)(void));

void *gc_enter_fence_cb(gc_mutator_func mutator, void *user1, void *user2)
{
  gc_enter_fence();

  // Call the mutator callback function in a fashion that safely clears the
  // fence state in case a JavaScript exception is thrown inside the call stack.
  return js_try_finally(mutator, user1, user2, gc_exit_fence);
}

static void gc_wait_for_all_threads_resumed_execution()
{
  while(num_threads_resumed_execution < num_threads_finished_marking) gc_uninterrupted_sleep(1);
}

static void start_multithreaded_collection()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  gc_wait_for_all_threads_resumed_execution();

  if (!mark_array) mark_array = malloc(512*1024);
  producer_head = producer_tail = consumer_head = consumer_tail = 0;
  gc_enter_fence();
  num_threads_resumed_execution = num_threads_finished_marking = 0;
  num_threads_ready_to_start_marking = 1;
  mt_marking_running = 1;
  wait_for_all_participants();
  GC_MALLOC_ACQUIRE();
#endif
}

static void wait_for_all_threads_finished_marking()
{
  while(mt_marking_running && num_threads_finished_marking < num_threads_ready_to_start_marking) gc_uninterrupted_sleep(1);
  ++num_threads_resumed_execution;
}

static void gc_acquire_lock(emscripten_lock_t *lock)
{
  emscripten_lock_t val;
  do val = emscripten_atomic_cas_u32((void*)lock, 0, 1);
  while(val);
}

static void gc_release_lock(emscripten_lock_t *lock)
{
  emscripten_atomic_store_u32((void*)lock, 0);
}

static void mark_from_queue()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  for(;;)
  {
    uint32_t tail = consumer_tail;
tail_again:
    if (tail < consumer_head)
    {
      uint32_t actual = cas_u32(&consumer_tail, tail, tail+1);
      if (actual != tail) { tail = actual; goto tail_again; }
    }
    else
    {
      ++num_threads_finished_marking;
      wait_for_all_threads_finished_marking();
      return;
    }
    void *ptr = mark_array[tail];
    __c11_atomic_fetch_add(&producer_tail, 1, __ATOMIC_SEQ_CST);
    mark(ptr, malloc_usable_size(ptr));
  }
#endif
}

static emscripten_semaphore_t sweep_command = EMSCRIPTEN_SEMAPHORE_T_STATIC_INITIALIZER(0);
static _Atomic(int) sweep_worker_running, sweep_worker_should_quit;

static void finish_multithreaded_marking()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  mark_from_queue();
  mt_marking_running = 0;
  gc_exit_fence();

  // Instruct the sweep worker (if it exists) to start sweeping. If it doesn't,
  // we perform the sweeping here locally. This logic is needed even if we
  // always use the sweep worker, because in stress test harness the sweep
  // worker may take time to start up, and at start of gc_collect() we must
  // synchronously spinlock to ensure that the previous sweep job has finished.
  if (sweep_worker_running) emscripten_semaphore_release(&sweep_command, 1);
  else sweep();
#endif
}

#ifdef __EMSCRIPTEN_SHARED_MEMORY__

static void mark(void *ptr, size_t bytes)
{
  uint32_t i;
  assert(IS_ALIGNED(ptr, sizeof(void*)));
  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; ++p)
  {
    if ((i = find_index(*p)) == INVALID_INDEX) continue;
    uint8_t bit = ((uint8_t)1 << (i&7));
    _Atomic(uint8_t) *marks = (_Atomic(uint8_t)*)mark_table + (i>>3);
    uint8_t old = *marks;
again:
    if ((old & bit)) continue; // This pointer is already marked? Then can skip it.
    uint8_t actual = cas_u8(marks, old, old | bit);
    if (old != actual) { old = actual; goto again; } // Some other bit in this byte got flipped by another thread, retry marking this.

    if (HAS_FINALIZER_BIT(table[i])) ++num_finalizers_marked;
    if (!HAS_LEAF_BIT(table[i]))
    {
      uint32_t head = __c11_atomic_fetch_add(&producer_head, 1, __ATOMIC_SEQ_CST);
      mark_array[head] = *p;
      while(cas_u32(&consumer_head, head, head+1) != head) ; // nop
    }
  }
}

static char sweep_worker_stack[256];
static emscripten_wasm_worker_t worker;

static void sweep_worker_main()
{
  sweep_worker_running = 1;
  for(;;)
  {
    emscripten_semaphore_waitinf_acquire(&sweep_command, 1);
    if (sweep_worker_should_quit) break;
    sweep();
  }
  sweep_worker_running = 0;
}

__attribute__((constructor(40))) static void initialize_sweep_worker()
{
  worker = emscripten_create_wasm_worker(sweep_worker_stack, sizeof(sweep_worker_stack));
  emscripten_wasm_worker_post_function_v(worker, sweep_worker_main);
}

#endif
