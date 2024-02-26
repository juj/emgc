# Emgc

A tiny low-level research garbage collector to be used solely on the Emscripten WebAssembly platform.

This is a toy project used to introspect Emscripten compiler behavior. Not for production use.

 - [ℹ Introduction](#introduction)
 - [📄 Usage](#usage)
 - [🔎 Details](#details)
   - [❓ Pointer Identification](#-pointer-identification)
   - [🌏 Global Memory Scanning](#-global-memory-scanning)
   - [🌳 Roots and Leaves](#-roots-and-leaves)
     - [🌱 Roots](#-roots)
     - [🍃 Leaves](#-leaves)
   - [📌 Weak Pointers](#-weak-pointers)
   - [📚 Stack Scanning](#-stack-scanning)
     - [𝕏² Quadratic Memory Usage](#𝕏-quadratic-memory-usage)
   - [🪦 Finalizer Support](#-finalizer-support)
   - [🔢 WebAssembly SIMD](#-webassembly-simd)
   - [🧶 Multithreaded Garbage Collection](#-multithreaded-garbage-collection)
 - [🧪 Running Tests](#-running-tests)
 - [☠️ Challenges with using a GC in WebAssembly](#-challenges-with-using-a-gc-in-webassembly)
   - [📚 The Hidden Stack Problem](#-the-hidden-stack-problem)
   - [𝕏² Quadratic Memory Usage](#-quadratic-memory-usage)
   - [📣 Cooperative Signaling Problem](#-cooperative-signaling-problem)
   - [🦌 Gathering-The-Herd Problem](#-gathering-the-herd-problem)
   - [💤 Sleep Slicing Problem](#-sleep-slicing-problem)

# ℹ Introduction

Emgc provides the user the ability to do low-level `malloc()` style memory allocations that do not need to be manually `free()`d, but are garbage collected instead. Example C/C++ code:

```c
#include "emgc.h"

void work()
{
  char *data = gc_malloc(32);
  char *data2 = gc_malloc(10000);
  // Skip freeing pointers at the end of function.
}

int main()
{
  work();
  gc_collect(); // Runs garbage collection to free up data and data2.
}
```

The memory returned by `gc_malloc()` are referred to as "GC allocations" or "managed pointers".

To identify which GC allocations are no longer reachable by the program and thus can be freed, emgc implements a typical **Mark-and-Sweep** collection process.

In the Mark phase of this process, the
  1) program stack,
  2) the global data section, and
  3) any explicitly declared root pointers

are scanned to find managed pointers that the program code can still reference (i.e. are *"reachable"* or *"alive"*). Then the memory regions of each reachable pointer are further scanned to identify more managed pointers that are still reachable, and so on, finally resulting in a full set of managed allocations still referenceable by the program code.

Then a Sweep phase of the garbage collection process frees up all GC allocations that were not found (not marked) during the search, and thus no longer reachable.

Additionally, Emgc does support manual freeing of GC memory, even though that is not necessary. I.e. just like with `malloc()` and `free()`, it is possible to manually call `gc_free()` on a GC pointer:

```c
#include "emgc.h"

int main()
{
  char *data = gc_malloc(1024*1024*1024);
  gc_free(data); // Explicitly frees up the allocated memory. May be useful to immediately
}                // force huge allocs to go away.
```

# 📄 Usage

To use emgc in your program, compile the file `emgc.c` along with your program code, and `#include "emgc.h"`.

Additionally, you must **choose one** of the **three** possible operation modes in order to use Emgc. See the section [Stack Scanning](#stack-scanning) below.

# 🔎 Details

See the following sections for more detailed information on Emgc.

### ❓ Pointer Identification

During marking, Emgc scans raw memory regions to identify any values that could look like managed pointers. This process requires that all pointer values are stored at aligned addresses, i.e. at addresses that are multiples of 4 in a 32-bit build, and at multiples of 8 in a 64-bit build. Pointers that would be stored at unaligned addresses would go undetected by the marking process (with catastrophic consequences).

This kind of scanning of GC pointers from unstructured linear memory is **conservative** and can cause **false positives** (values in raw memory may coincide with the bit patterns of GC pointers and be mistakenly thought to be referenced, but in fact are not). This kind of collision occurrence is assumed to be rare, and at worst case will result in a larger memory consumption for the application. Though note that there are ways to address this to make the collection precise. (TODO: document)

All scanned pointers need to point to the starting address of the allocation, as returned by `gc_malloc()`. Emgc does not detect pointers that point to the interior address of a managed allocation.

### 🌏 Global Memory Scanning

By default, Emgc scans (i.e. marks) all static data (the memory area holding global variables) during garbage collection to find managed pointers.

This is convenient for getting started, although in a larger application, the static data section can grow large and typically only few global variables might hold managed pointers, so this automatic scanning of global data can become inefficient.

To disable automatic static data marking, pass the define `-DEMGC_SKIP_AUTOMATIC_STATIC_MARKING=1` when compiling `emgc.c`.

### 🌳 Roots and Leaves

Managed allocations can be specialized in two different ways: as roots or leaves.

#### 🌱 Roots
A managed allocation may be declared as a **root allocation** with the `gc_make_root(ptr)` function. A root allocation is always assumed to be reachable by the collector, and will never be freed by `gc_collect()`.

A manual call to `gc_free(ptr)` is required to free a root allocation. For example:

```c
#include "emgc.h"

int **global;

int main()
{
    global = (int**)gc_malloc(128);
    gc_make_root(global);

    global[0] = (int*)gc_malloc(42);
    gc_collect(); // will not free memory because global was marked a root.

    gc_free(global); // Finally free the root allocation.
}
```

During garbage collection, all root pointers are always scanned.

Even if the program is compiled with `-DEMGC_SKIP_AUTOMATIC_STATIC_MARKING=1`, the call to `gc_collect()` in above code will not free up any memory, since `global` is declared to be a root, and it references the second allocation, so both allocations are kept alive.

The function `gc_unmake_root(ptr)` can be used to restore a pointer from being a root back into being a regular managed allocation. (it is not necessary to manually do this before `gc_free()`ing the root pointer though)

The function `gc_malloc_root(bytes)` is provided for conveniently allocating root memory in one call.

#### 🍃 Leaves

A **leaf allocation** is one that is guaranteed by the user to not contain any pointers to other managed allocations. If you are allocating large blocks of GC memory, say, for strings or images, that will never contain managed pointers, it is a good idea to mark those allocations as leaves. The `gc_collect()` function will skip scanning any leaf objects, improving runtime performance.

Use the function `gc_make_leaf(ptr)` to declare a GC allocation a leaf. For example:

```c
#include "emgc.h"

int main()
{
    char *string = (char*)gc_malloc(1048576);
    gc_make_leaf(string);

    gc_collect(); // will not scan contents of 'string'.
}
```

Symmetrically, there exists a function `gc_unmake_leaf(ptr)` to undo an allocation from being a leaf allocation.

The function `gc_malloc_leaf(bytes)` is provided for conveniently allocating leaf memory in one call.

While it is technically possible to make an allocation simultaneously be both a root and a leaf, it is more optimal to just use regular `malloc()` + `free()` API for such allocations.

Note that while declaring GC allocations as leaves is a performance aid, declaring roots is required for correct GC behavior in your program.

### 📌 Weak Pointers

Emgc provides the ability to maintain weak pointers to managed allocations. Unlike regular ("strong") GC pointers, weak pointers do not keep the GC pointers they point to alive.

A weak pointer is tracked as a `void *` pointer, and cannot be directly dereferenced (so do not type cast it to any other type, e.g. `char*` or similar).

To convert a strong GC pointer to a weak pointer, call the function `gc_get_weak_ptr(ptr)`. Example:

```c
#include "emgc.h"

int main()
{
    char *data = (char*)gc_malloc(100);
    void *weak = gc_get_weak_ptr(data);
    data = 0;

    gc_collect(); // may free 'data', since only a weak reference to it remains.
    if (gc_weak_ptr_equals(weak, 0))
      printf("Weak pointer got GCd!");
}
```

Weak pointers have slightly different semantics to strong pointers:
 - Do not compare weak pointers against other pointers, **not even to null**. Instead use the function `gc_weak_ptr_equals(ptr1, ptr2);` where `ptr1` and `ptr2` may be weak or strong pointers, or null.
 - Do not type cast weak pointers to dereference them.
 - To dereference a weak pointer, it must be turned back to a strong pointer first by calling `gc_acquire_strong_ptr(ptr)`, where `ptr` may be a weak or a strong pointer. If the object pointed to by a weak pointer has been freed, the function returns null.
 - To test if a pointer represents a weak pointer, call the function `gc_is_weak_ptr(ptr)`.
 - To test if a pointer represents a strong pointer, call the function `gc_is_strong_ptr(ptr)`.
 - The null pointer is considered **both** a weak and a strong pointer.

Note that unlike root and leaf properties that are properties of the allocation itself, weak vs strong pointers are a property of the pointer. A single GC allocation can have a combination of several strong and weak pointers pointing to it.

Internally weak pointers are implemented via *pointer disguising*, which makes the marking process skip over them.

### 📚 Stack Scanning

To identify managed pointers on the program stack, Emgc automatically scans the LLVM data stack.

However, there is a challenge: because WebAssembly places most of its function local variables into Wasm `local`s, and Wasm does not provide means to introspect/enumerate these locals, then by default the LLVM data stack will likely not (by default) contain all of the managed pointers that Emgc would need to observe, which would break the soundness of the garbage collector.

To remedy this, opt in to one of three choices:

1. **collect-on-empty-stack mode**: Use this mode if you are building in single-threaded mode (or are building multi-threaded, but with access to managed state restricted only to a single thread), and can ensure that you will never call `gc_collect()` when there could potentially exist managed pointers on the program stack. A good strategy to deploy this mode is to call the JS `setTimeout()` function to only ever asynchronously invoke a garbage collection in a separate event handler after the stack is empty. The `gc_collect_when_stack_is_empty()` function is provided to conveniently do this. For example:

```c
#include "emgc.h"

void some_function()
{
  char *data = gc_malloc(42);
  // gc_collect();  // Unsafe if building without --spill-pointers pass. Data would incorrectly be freed.
  gc_collect_when_stack_is_empty(); // Safe, calls setTimeout() to schedule collection after this event.
  data[0] = 42;
}
```

2. **--spill-pointers mode**: At the final Wasm link stage, specify the linker flag `-sBINARYEN_EXTRA_PASSES=--spill-pointers`. This causes the Binaryen optimizer to perform a special pointer spilling codegen pass, that will cause anything that looks like a pointer to be explicitly spilled on to the LLVM data stack, in all functions of the program. This way all the managed pointers will be guaranteed to be observable by Emgc when it is scanning the LLVM data stack, making it safe to call `gc_collect()` at any point in the program.

3. **Fenced --spill-pointers mode** (also called just the **fenced mode**): This mode is like the **--spill-pointers mode**, but additionally in this mode, access to managed program objects is restricted to only be available after entering a **fenced scope**. This kind of scope declares to Emgc that the current thread is going to act as a mutator that modifies managed object state. Keeping track of these mutator threads enables Emgc to provided multithreading support for garbage collection.
\
Additionally, fenced scopes provide an opportunity for a small micro-optimization: only a sub-portion of the thread's call stack will need to be marked to find managed pointers, as the fence delimits where on the stack potential GC pointers might exist.

All these modes come with drawbacks:

- The --spill-pointers modes reduce performance and increases code size, since every function local variable that might be a pointer needs to be shadow copied to the LLVM data stack. This overhead can be prohibitive to some applications.

- Only the fenced --spill-pointers mode can be used in multithreaded applications. The unfenced --spill-pointers mode and the collect-on-empty-stack mode cannot be used when building with multithreading enabled.

- The fenced --spill-pointers mode carries a potentially heavy overhead to the generated code size, as cooperative marking points will need to be emitted across all functions in the program.

- In the collect-on-empty-stack mode, the application will be unable to resolve any OOM situations by collecting on the spot inside a `gc_malloc()` call. If the application developer knows they will not perform too many temp allocations, this might not sound too bad; but there is a grave gotcha that can cause certain algorithms that use Θ(n) of memory to consume Θ(n²) of memory instead. See the section [𝕏² Quadratic Memory Usage](#𝕏-quadratic-memory-usage) at the end for more details.

### 🪦 Finalizer Support

It is possible to register a finalizer callback to be run before a lost GC object is freed. Use the function `gc_register_finalizer(ptr, callback)` for this purpose. Example:

```c
#include "emgc.h"
#include <stdio.h>

void my_finalizer(void *ptr)
{
  printf("This GC object is getting freed.\n");
}

void work()
{
  void *ptr = (char*)gc_malloc(1024);
  gc_register_finalizer(ptr, my_finalizer);
}

int main()
{
  work();
  gc_collect(); // Calls my_finalizer callback before gc_free()ing the ptr.
}
```

Finalizers are called on GC objects in an unspecified order. A finalizer can resurrect the GC pointer it is called on, or other GC pointers that would be about to be lost.

It is assumed that the presence of objects with finalizers is rare. Reclaiming objects with finalizers will have a delaying effect on garbage collection, and multiple calls to `gc_collect()` may be needed to observe the invocations of all finalizable objects.

If an object resurrects itself during finalization, its finalizer will be reset and will not be called again when the object actually is freed.

### 🔢 WebAssembly SIMD

Emgc optionally utilizes the WebAssembly SIMD instruction set to speed up marking.

In a synthetic, possibly best-case performance test ([test/performance.c](test/performance.c)), Emgc achieves a 1128.32 MB/sec marking speed in scalar mode, and a 3719.91 MB/sec marking speed with SIMD. (3.30x faster)

To enable SIMD optimizations, build with the `-msimd128` flag at both compile and link time.

### 🧶 Multithreaded Garbage Collection

It is possible to utilize Emgc `gc_malloc()` allocations and `gc_collect()` garbage collections from multiple threads.

But in order to do so, Emgc must be operated in the **fenced --spill-pointers** mode. The other two build modes are not viable with multithreaded programs.

In the fenced mode, threads are not allowed to mutate (modify) the contents of any managed objects or managed root regions without entering a fenced scope. It is up to the programmer to ensure that this invariant is enforced.

This fenced scope is entered by calling the function `gc_enter_fence_cb(callback, user1, user2)`. The first parameter to this function is a callback function, which will be immediately (synchronously) called back from inside `gc_enter_fence_cb()`. The two other parameters are custom user data pointers. The passed callback may return a `void*` parameter back to the caller.

Inside this callback function (and anywhere that executes nested inside this call stack scope), the program code is free to perform modifications to GC objects and allocate new GC objects. For example:

```c
#include "emgc.h"

void work(void *user1, void *user2)
{
  char *str = gc_malloc(4); // Inside fenced scope we can access GC objects and do GC allocations.
}

int main()
{
  gc_enter_fence_cb(work, 0, 0);
  gc_collect(); // We can collect outside fenced scope.
}
```

The functions `gc_malloc()`, `gc_malloc_root()`, `gc_malloc_leaf()` and `gc_acquire_strong_ptr()` may only be called from inside a fenced scope.

The functions `gc_collect()` and `gc_collect_when_stack_is_empty()` may freely be called from anywhere **outside** a fenced scope (and will implicity place the caller inside a fenced scope for the duration of the call).

When any thread initiates a garbage collection with `gc_collect()`, all threads that are currently executing code inside a fence will immediately join to simultaneously work on the *mark phase* of the garbage collection process in parallel.

When the mark phase is complete, each fenced thread will resume code execution from where they left off inside their fenced scope, and the *sweep phase* will be completed on the background in a single dedicated sweep worker thread.

Fenced mode is always enabled when building with `-sWASM_WORKERS` or `-pthread`. You can also manually activate fenced mode by building with `-DEMGC_FENCED`.

N.b. if you are building C++ code with C++ exceptions enabled, you should manually ensure that no exception will unwind the `gc_enter_fence_cb()` function from the callstack.

# 🧪 Running Tests

Execute `python test.py` to run the test suite.

# ☠️ Challenges with using a GC in WebAssembly

Implementing a garbage collector in WebAssembly is currently not seamless, but comes with some limitations.

This section details some design problems that have been observed while implementing a GC for use inside Wasm.

## 📚 The Hidden Stack Problem

In WebAssembly, the program callstack is not introspectable by user code. This prevents a garbage collector from finding pointers on the stack. For more details, check the previous [Stack Scanning](#stack-scanning) section above.

There is a proposed solution to this issue in [WebAssembly/design#1459](https://github.com/WebAssembly/design/issues/1459)

## 𝕏² Quadratic Memory Usage

Any code that performs a linear number of linearly growing temporary calls to `gc_malloc()`, will turn into a quadratic memory usage under the collect-on-empty-stack scanning mode. For example, see the following code:

```c
char *linear_or_quadratic_memory_use()
{
  char *str = (char*)gc_malloc(1);
  str[0] = '\0';
  int len = 1;
  for(int i = 0; i < 10000; ++i)
  {
    len += 3;
    char *str2 = (char*)gc_malloc(len);
    strcpy(str2, str);
    strcat(str2, "foo");
    str = str2;
  }
  return str;
}
```

The above code generates a long string by concatenating `"foo"` 10000 times. If the heap is about to run out of memory, `gc_malloc()` can collect garbage on demand if running in the `--spill-pointers` mode. This means that the above code will first consume some amount of temporary memory (as the available heap size permits), which will be promptly collected, and then finally the code persists `10000 * strlen("foo")+1` == `30001 bytes` of memory for the string at the end of the function.

If Emgc is operating in only-collect-when-stack-is-empty mode, the above code will temporarily require `1 + 4 + 7 + 10 + ... + 30001` = `150,025,000 bytes` of free memory on the Wasm heap!

The recommendation here is hence to be extremely cautious of containers and strings when building without `--spill-pointers`. It is advisable to perform std::vector style **geometric capacity growths** of memory for containers and strings when compiling under this mode to mitigate the quadratic memory growth issue.

## 📣 Cooperative Signaling Problem

In the browser WebAssembly relies on Web Workers to provide support for shared state multithreading.

Traditionally in a stop-the-world garbage collector, all the threads that are accessing GC state are paused by sending them a [pre-emptive signal](https://man7.org/linux/man-pages/man2/signal.2.html) or by [instructing the OS to suspend the thread](https://www.ibm.com/docs/en/aix/7.2?topic=p-pthread-suspend-np-pthread-unsuspend-np-pthread-continue-np-subroutine) and not schedule it until it has been resumed. This method can be used to cause the threads to pause whatever they are doing, and/or jump to executing a custom signal handler function. This mechanism works well in native code for forcing all GC threads to synchronize for the marking process to start.

On the web however, such mechanisms are not available. To work around this limitation, Emgc employs **cooperative signaling**. In cooperative signaling, the compiler emits check points to all generated code to make sure that each thread will continuously check in whether it is time for the thread to pause for garbage collection.

In the context of Emgc, this cooperative GC checkpoint emitting is done in the [--instrument-cooperative-gc pass in Binaryen](https://github.com/juj/binaryen/blob/spill_pointers_fixes/src/passes/InstrumentCooperativeGC.cpp). This pass must be enabled whenever using Emgc in a mode where there may exist multiple mutator threads.

There are two immediate downsides to this strategy:

1. First one is that this kind of instrumentation will bloat up the disk size of the generated .wasm file. Each loop in the whole program will acquire a call to this GC checkpoint, and there are a lot of loops in any typical program.

2. The second downside is that this kind of constant checking will reduce runtime performance. Small loops such as `memcpy()`, `memset()` and `strcpy()` may need custom blacklisting to exclude them from resulting in pathologically poor performance.

There are two further issues in this strategy, illustrated in more detail in the following two sections.

## 🦌 Gathering-The-Herd Problem

A major issue caused by cooperative GC signaling under a stop-the-world scheme is that garbage collection cannot start until all threads have been successfully paused. That is, each managed thread must stop mutating the object graph before GC marking can begin.

Under a pre-emptive scheme this type of operation is generally not too bad, since thread pre-emption is specifically managed by the operating system. If the thread was a background thread that was already sleeping to wait for a mutex, then marking it as suspended is very cheap.

Under cooperative signaling however, it is unclear how long it will take until each of the worker threads continue their execution to reach next cooperative GC check point. If such GC check points have been excessively emitted at e.g. all function prologues and loop back-edges (like is typical), this process might not be too bad. Although there is pressure to omit such checkpoints e.g. at certain function prologues, or in very short non-atomic loops (`memcpy()`, `memset()`, ...), since their presence have adverse effects on disk size and runtime speed.

Further, the more managed threads there are, the longer the other threads will take to wait for all of those threads to "gather up" together to the GC mark sync point. Many large applications have more threads than there are logical cores (oversubscription), so many of those threads may need to be woken up and be scheduled to the CPU first, just to immediately reach the common GC sync point where they will put to sleep again.

So in effect, this ***"gathering the herd" procedure may become slow, and slower as the number of managed threads grow***.

## 💤 Sleep Slicing Problem

In the previous sections, it was explained how all the managed threads need to be synchronized together in a common GC point in order to start the GC marking process.

But, when a GC marking is about to start, what if one of the managed threads was waiting for a futex or sleeping? Then, since there is no pre-emptive signaling support on the web, the GC marking process cannot start until this futex/sleep operation finishes.

So in order to keep the system responsive, all futexes/sleeps in managed threads should be sliced up into very small wait quantums at a time, so that those threads may perform the GC sync check in between.

For example, instead of performing a futex wait for 10 seconds, the code should be changed to, for example, wait in 10 millisecond, 1 millisecond or 0.1 millisecond slices, and check for the need to GC in between.

This might initially read like a simple solution to a problem, but in fact there is a can of worms in wait here.

The first problem is that coming up with a proper wait slice is far from obvious. For a text editor application, a long 10 millisecond wait quantum might not be a problem at all, if GC pauses occur relatively infrequently. However for a real-time interactive game, 10 milliseconds might mean 60% of the game's computation budget to compute a single frame, so the sleep time such be smaller. In game development, developers may chase over 1 millisecond optimization wins, so even a 1 millisecond latency to initiate a GC may be enough to cause observable GC stuttering.

Then, an application might be attracted choose a short wait slice like 0.1 milliseconds and consider that a problem solved?

Well, here then comes the unfortunate other side of the problem. In a large application there may exist a few dozen of background threads, all typically waiting dormant to perform some small dedicated task.

Under a sliced wait scheme that allows these threads to poll if GC participation would be needed, these threads would need to be continuously scheduled by the CPU to execute. This would continuously consume energy, and take throughput performance away from the actually executing threads in the program.

Currently Emgc does not tune its sleep quantum in any way, but at the time of writing has it set to [a ridiculously low value of 100 nsecs](https://github.com/juj/emgc/blob/df39cb6c4a60be87334073fd68e999177a118fd8/emgc-multithreaded.c#L53). This may change after more experience from real-world application benchmarking is gained.
