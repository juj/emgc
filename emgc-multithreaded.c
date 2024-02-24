#include <emscripten/wasm_worker.h>
#include <emscripten/html5.h>

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
// In multithreaded builds, use a simple global spinlock strategy to acquire/release access to the memory allocator.
static volatile uint8_t mt_lock = 0;
#define GC_MALLOC_ACQUIRE() while (__sync_lock_test_and_set(&mt_lock, 1)) { while (mt_lock) { /*nop*/ } }
#define GC_MALLOC_RELEASE() __sync_lock_release(&mt_lock)
// Test code to ensure we have tight malloc acquire/release guards in place.
#define ASSERT_GC_MALLOC_IS_ACQUIRED() assert(mt_lock == 1)
#define GC_CHECKPOINT_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
// In singlethreaded builds, no need for locking.
#define GC_MALLOC_ACQUIRE() ((void)0)
#define GC_MALLOC_RELEASE() ((void)0)
#define ASSERT_GC_MALLOC_IS_ACQUIRED() ((void)0)
#define GC_CHECKPOINT_KEEPALIVE
#endif

#if defined(__EMSCRIPTEN_SHARED_MEMORY__) || defined(EMGC_FENCED)
#define ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED() assert(this_thread_accessing_managed_state && "In fenced build mode, GC state must be only accessed from inside gc_enter_fenced_access() scope.")
#else
#define ASSERT_GC_FENCED_ACCESS_IS_ACQUIRED() ((void)0)
#endif

static void mark_from_queue();
static void mark_current_thread_stack();
static void mark(void *ptr, size_t bytes);
static uint32_t find_index(void *ptr);

static _Atomic(int) num_threads_accessing_managed_state, mt_marking_running, num_threads_ready_to_start_marking, num_threads_finished_marking, num_threads_resumed_execution;
static __thread int this_thread_accessing_managed_state;
static __thread uintptr_t stack_top;
static emscripten_lock_t mark_lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;
static void **mark_array;
static _Atomic(uint32_t) mark_head, mark_tail;

void gc_sleep(double nsecs)
{
  if (emscripten_current_thread_is_wasm_worker()) emscripten_wasm_worker_sleep(nsecs);
  else
  {
    double t = emscripten_performance_now() + nsecs/1000000.0;
    while(emscripten_performance_now() < t) /*nop*/;
  }
}
static void wait_for_all_participants()
{
  // Wait for all threads currently executing in managed context to gather up together for the collection.
  while(num_threads_ready_to_start_marking < num_threads_accessing_managed_state) gc_sleep(1);
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

static void enter_gc_fence()
{
  // If there is a current GC collection going, help out the GC collection before
  // we enter managed state. Need to track two cases: if we have a previous nested
  // call to gc_enter_fenced_access() from before, do full participation (that
  // scans this stack). Otherwise we simply assist in marking (without scanning
  // this thread's stack).
  if (this_thread_accessing_managed_state) gc_participate_to_garbage_collection();
  else
  {
    // Record where the stack is currently at. Any functions before this cannot
    // contain GC pointers, so this is an easy micro-optimization to local stack
    // scanning.
    stack_top = emscripten_stack_get_current();
    ++num_threads_accessing_managed_state;
  }
  ++this_thread_accessing_managed_state;}

static void exit_gc_fence()
{
  --this_thread_accessing_managed_state;
  if (!this_thread_accessing_managed_state) --num_threads_accessing_managed_state;
}

void *js_try_finally(gc_mutator_func func, void *user1, void *user2, void (*finally_func)(void));

void *gc_enter_fenced_access(gc_mutator_func mutator, void *user1, void *user2)
{
  enter_gc_fence();

  if (mt_marking_running) mark_from_queue();

  // Call the mutator callback function in a fashion that safely clears the
  // fence state in case a JavaScript exception is thrown inside the call stack.
  return js_try_finally(mutator, user1, user2, exit_gc_fence);
}

static void gc_wait_for_all_threads_resumed_execution()
{
  while(num_threads_resumed_execution < num_threads_finished_marking) gc_sleep(1);
}

static void start_multithreaded_collection()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  gc_wait_for_all_threads_resumed_execution();

  if (!mark_array) mark_array = malloc(512*1024);
  mark_head = mark_tail = 0;
  enter_gc_fence();
  num_threads_resumed_execution = num_threads_finished_marking = 0;
  num_threads_ready_to_start_marking = 1;
  mt_marking_running = 1;
  wait_for_all_participants();
#endif
}

static void wait_for_all_threads_finished_marking()
{
  while(mt_marking_running && num_threads_finished_marking < num_threads_ready_to_start_marking) gc_sleep(1);
  ++num_threads_resumed_execution;
}

static void gc_acquire_lock(emscripten_lock_t *lock)
{
  emscripten_lock_t val;
  do {
    val = emscripten_atomic_cas_u32((void*)lock, 0, 1);
  } while(val);
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
    gc_acquire_lock(&mark_lock);
    if (mark_head == mark_tail)
    {
      gc_release_lock(&mark_lock);
      ++num_threads_finished_marking;
      wait_for_all_threads_finished_marking();
      return;
    }
    void *ptr = mark_array[mark_tail];
    mark_tail = (mark_tail + 1) & (512*1024-1);
    gc_release_lock(&mark_lock);
    mark(ptr, malloc_usable_size(ptr));
  }
#endif
}

static void finish_multithreaded_marking()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  mark_from_queue();
  mt_marking_running = 0;
  exit_gc_fence();
#endif
}

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
static void mark(void *ptr, size_t bytes)
{
  uint32_t i;
  assert(IS_ALIGNED(ptr, sizeof(void*)));
  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; ++p)
    if ((i = find_index(*p)) != INVALID_INDEX && !BITVEC_GET(mark_table, i))
    {
      gc_acquire_lock(&mark_lock);

      BITVEC_SET(mark_table, i);

      if (HAS_FINALIZER_BIT(table[i])) ++num_finalizers_marked;
      if (!HAS_LEAF_BIT(table[i]))
      {
        mark_array[mark_head] = *p;
        mark_head = (mark_head + 1) & (512*1024-1); // TODO if (mark_head+1 == mark_tail)
      }
      gc_release_lock(&mark_lock);
    }
}
#endif
