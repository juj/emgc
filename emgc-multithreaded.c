#include <emscripten/wasm_worker.h>

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
// In multithreaded builds, use a simple global spinlock strategy to acquire/release access to the memory allocator.
static volatile uint8_t mt_lock = 0;
#define GC_MALLOC_ACQUIRE() while (__sync_lock_test_and_set(&mt_lock, 1)) { while (mt_lock) { /*nop*/ } }
#define GC_MALLOC_RELEASE() __sync_lock_release(&mt_lock)
// Test code to ensure we have tight malloc acquire/release guards in place.
#define ASSERT_GC_MALLOC_IS_ACQUIRED() assert(mt_lock == 1)
#else
// In singlethreaded builds, no need for locking.
#define GC_MALLOC_ACQUIRE() ((void)0)
#define GC_MALLOC_RELEASE() ((void)0)
#define ASSERT_GC_MALLOC_IS_ACQUIRED() ((void)0)
#endif

static void mark_from_queue();
static void mark(void *ptr, size_t bytes);
static uint32_t find_index(void *ptr);

static _Atomic(int) num_threads_accessing_managed_state, mt_marking_running, num_managed_threads_participating_in_marking;
static __thread int this_thread_accessing_managed_state = 0;
static emscripten_lock_t mark_lock = EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER;
static void **mark_array;
static _Atomic(uint32_t) mark_head, mark_tail;
static _Atomic(int) can_start_marking, num_threads_marking;

static void wait_for_all_participants()
{
  // Wait for all threads currently executing in managed context to gather up together for the collection.
  while(num_managed_threads_participating_in_marking < num_threads_accessing_managed_state) /*nop*/;
}

void gc_participate_to_garbage_collection()
{
  if (mt_marking_running && this_thread_accessing_managed_state)
  {
    uintptr_t stack_bottom = emscripten_stack_get_current();
    ++num_managed_threads_participating_in_marking;
    ++num_threads_marking;
    wait_for_all_participants();
    while(!can_start_marking) /*nop*/ ;
    mark((void*)stack_bottom, emscripten_stack_get_base() - stack_bottom);
    mark_from_queue(); 
  }
}

void gc_access_managed_state(gc_mutator_func mutator, void *user1, void *user2)
{
  // If there is a current GC collection going, help out the GC collection before
  // we enter managed state. Need to track two cases: if we have a previous nested
  // call to gc_access_managed_state() from before, do full participation (that
  // scans this stack). Otherwise we simply assist in marking (without scanning
  // this thread's stack).
  if (this_thread_accessing_managed_state) gc_participate_to_garbage_collection();
  else if (mt_marking_running) mark_from_queue();

  if (!this_thread_accessing_managed_state) ++num_threads_accessing_managed_state;
  ++this_thread_accessing_managed_state;
  mutator(user1, user2);
  --this_thread_accessing_managed_state;
  if (!this_thread_accessing_managed_state) --num_threads_accessing_managed_state;
}

static void start_multithreaded_collection()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  if (!mark_array) mark_array = malloc(512*1024);
  mark_head = mark_tail = 0;
  num_managed_threads_participating_in_marking = 0;
  can_start_marking = 0;
  num_threads_marking = 0;
  mt_marking_running = 1;
  wait_for_all_participants();
#endif
}

static void start_multithreaded_marking()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  can_start_marking = 1;
#endif
}

static void wait_for_all_marking_complete()
{
  while(num_threads_marking > 0) /*nop*/;  
}

static void mark_from_queue()
{
  for(;;)
  {
    emscripten_lock_busyspin_wait_acquire(&mark_lock, 1e9);
    if (mark_head == mark_tail)
    {
      emscripten_lock_release(&mark_lock);
      --num_threads_marking;
      wait_for_all_marking_complete();
      return;
    }
    void *ptr = mark_array[mark_tail];
    mark_tail = (mark_tail + 1) & (512*1024-1);
    emscripten_lock_release(&mark_lock);
    mark(ptr, malloc_usable_size(ptr));
  }
}

static void finish_multithreaded_marking()
{
#ifdef __EMSCRIPTEN_SHARED_MEMORY__
  mark_from_queue();
  mt_marking_running = 0;
#endif
}

#ifdef __EMSCRIPTEN_SHARED_MEMORY__
static void mark(void *ptr, size_t bytes)
{
  uint32_t i;
  assert(IS_ALIGNED(ptr, sizeof(void*)));
  for(void **p = (void**)ptr; (uintptr_t)p < (uintptr_t)ptr + bytes; ++p)
    if ((i = find_index(*p)) != (uint32_t)-1 && BITVEC_GET(mark_table, i))
    {
      emscripten_lock_busyspin_wait_acquire(&mark_lock, 1e9);

      BITVEC_CLEAR(mark_table, i);

      if (HAS_FINALIZER_BIT(table[i])) __c11_atomic_fetch_add((_Atomic uint32_t*)&num_finalizers_marked, 1, __ATOMIC_SEQ_CST);
      if (HAS_LEAF_BIT(table[i]))
      {
        mark_array[mark_head] = *p;
        mark_head = (mark_head + 1) & (512*1024-1); // TODO if (mark_head+1 == mark_tail)
      }
      emscripten_lock_release(&mark_lock);
    }
}
#endif
