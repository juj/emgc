# Emgc

A tiny low-level research garbage collector to be used solely on the Emscripten WebAssembly platform.

This is a toy project used to introspect Emscripten compiler behavior. Not for production use.

# Introduction

Emgc provides the user the ability to do low-level `malloc()` style memory allocations do not need to be manually `free()`d, but are garbage collected instead. Example C/C++ code:

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

To identify which GC allocations are no longer reachable by the program and thus can be freed, emgc implements a typical Mark-and-Sweep collection process.

In the Mark phase of this process, the program stack, the global data section, and other declared root regions are scanned to find managed pointers that the program code can still reference (i.e. are *"reachable"* or *"alive"*). Then the memory regions of each reachable pointer are further scanned to identify more managed pointers that are still reachable, and so on, finally resulting in a full set of managed allocations still referenceable by the program code.

Then a Sweep phase of the garbage collection process frees up all GC allocations that were not found (not marked) during the search, and thus no longer reachable.

Additionally, Emgc does support manual freeing of GC memory, even though that is not necessary. I.e. just like with `malloc()` and `free()`, it is possible to manually call `gc_free()` on a pointer:

```c
#include "emgc.h"

int main()
{
  char *data = gc_malloc(1024*1024*1024);
  gc_free(data); // Explicitly frees up the allocated memory. May be useful to immediately
}                // force huge allocs to go away.
```

# Usage

To use emgc in your program, compile the file `emgc.c` along with your program code, and `#include "emgc.h"`.

Additionally, you must choose one of the two possible stack scanning modes in order to use Emgc. See the section [Stack Scanning](#stack-scanning) below.

# Details

See the following sections for more detailed information on Emgc:
 - [Pointer Identification](#-pointer-identification)
 - [Global Memory Scanning](#-global-memory-scanning)
 - [Roots and Leaves](#-roots-and--leaves)
   - [Roots](#-roots)
   - [Leaves](#-leaves)
 - [Weak Pointers](#-weak-pointers)
 - [Stack Scanning](#-stack-scanning)
   - [Quadratic Memory Usage](#-quadratic-memory-usage)
 - [WebAssembly SIMD](#-webassembly-simd)

### ❓ Pointer Identification

During marking, Emgc scans raw memory regions to identify any values that could look like managed pointers. This process requires that all pointer values are stored at aligned addresses, i.e. at addresses that are multiples of 4 in a 32-bit build, and at multiples of 8 in a 64-bit build. Pointers that would be stored at unaligned addresses would go undetected by the marking process (with catastrophic consequences).

This kind of scanning of GC pointers from is **conservative** and can cause **false positives**, though note that there are ways to address this, to make collection be precise. (TODO: document)

All pointers need to point to the starting address of the memory buffer. Emgc does not detect pointers that point to the interior address of a managed allocation.

### 🌏 Global Memory Scanning

By default, Emgc scans (i.e. marks) all static data (the memory area holding global variables) during garbage collection to find managed pointers.

This is convenient for getting started, although in a larger application, the static data section can grow large and typically only few global variables might hold managed pointers, so this automatic scanning of global data can become inefficient.

To disable automatic static data marking, pass the define `-DEMGC_SKIP_AUTOMATIC_STATIC_MARKING=1` when compiling `emgc.c`.

### 🌱 Roots and 🍃 Leaves

Managed allocations can be seen in three flavors: regular, roots, and leaves.

#### 🌱 Roots
A managed allocation may be declared as a **root allocation** with the `gc_make_root(ptr)` function. A root allocation is always assumed to be reachable by the collector, and will never be freed by `gc_collect()`. A manual call to `gc_free(ptr)` is required to free a root allocation. For example:

```c
#include "emgc.h"

int **global;

int main()
{
    global = (int**)gc_malloc(128);
    gc_make_root(global);

    global[0] = (int*)gc_malloc(42);
    gc_collect(); // will not free memory because global was marked a root.
}
```

During garbage collection, all root pointers are always scanned.

Even if the program is compiled with `-DEMGC_SKIP_AUTOMATIC_STATIC_MARKING=1`, the above code will not free up any memory, since `global` is declared to be a root, and it references the second allocation, so both are kept alive.

The function `gc_unmake_root(ptr)` can be used to restore a pointer from being a root back into being a regular managed allocation. (it is not necessary to do this before `gc_free()`ing the pointer)

#### 🍃 Leaves

A **leaf allocation** is one that is guaranteed by the user to not contain any pointers to other managed allocations. If you are allocating large blocks of GC memory, say, for strings or images, that will never contain managed pointers, it is a good idea to mark those allocations as leaves. The `gc_collect()` function will skip scanning any leaf objects, improving runtime performance.

Use the functions `gc_make_leaf(ptr)` and `gc_unmake_leaf(ptr)`. For example:

```c
#include "emgc.h"

int main()
{
    char *string = (char*)gc_malloc(1048576);
    gc_make_leaf(string);

    gc_collect(); // will not scan contents of 'string'.
}
```

The functions `gc_malloc_root(bytes)` and `gc_malloc_leaf(bytes)` are provided for conveniently allocating root or leaf memory in one call.

While it is technically possible to make an allocation simultaneously be both a root and a leaf, it is more optimal to just use regular `malloc()` + `free()` API for such allocations.

Note that while declaring GC allocations as leaves is a performance aid, declaring roots is required for correct GC behavior in your program.

### 📎 Weak Pointers

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

### 📚 Stack Scanning

To identify managed pointers on the program stack, Emgc automatically scans the LLVM data stack.

However, there is a challenge: because WebAssembly places most of its function local variables into Wasm `local`s, and Wasm does not provide means to introspect/enumerate these locals, then by default the LLVM data stack will likely not (by default) contain all of the managed pointers that Emgc would need to observe, which would break the soundness of the garbage collector.

To remedy this, opt in to one of two choices:

1. At final Wasm link stage, specify the linker flag `-sBINARYEN_EXTRA_PASSES=--spill-pointers`. This causes the Binaryen optimizer to perform a special pointer spilling codegen pass, that will cause anything that looks like a pointer to be explicitly spilled on to the LLVM data stack, in all functions of the program. This way all the managed pointers will be guaranteed to be observable by Emgc when it is scanning the LLVM data stack, making it safe to call `gc_collect()` at any stage of the program.

2. Alternatively, ensure that you will never call `gc_collect()` when there could potentially exist managed pointers on the stack. A good strategy to deploy this mode is to call the JS `setTimeout()` function to only ever asynchronously invoke a garbage collection in a separate event handler after the stack is empty. The `gc_collect_when_stack_is_empty()` function is provided to conveniently do this. For example:

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

Both modes come with drawbacks:

- The --spill-pointers mode reduces performance and increases code size, since every function local variable that might be a pointer needs to be shadow copied to the LLVM data stack. This overhead can be prohibitive.

- In the collect-only-when-stack-is-empty mode, the application will be unable to resolve any OOM situations by collecting on the spot inside a `gc_malloc()` call. If the application developer knows they will not perform too many temp allocations, this might not sound too bad; but there is a grave gotcha, see the next section on memory usage.

#### 𝕏² Quadratic Memory Usage

Any code that performs a linear number of linearly growing temporary calls to `gc_malloc()`, will turn into a quadratic memory usage under the collect-only-when-stack-is-empty stack scanning scheme. For example, the following code:

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

The above code generates a long string by concatenating `"foo"` 10000 times. If the heap is about to run out of memory, `gc_malloc()` can collect garbage on demand if running with the `--spill-pointers` flag mode. This means that the above code will first consume some amount of temporary memory (as the available heap size permits), which will be promptly collected, and then finally the code persists `10000 * strlen("foo")+1` == `30001 bytes` of memory for the string at the end of the function.

If Emgc is operating in only-collect-when-stack-is-empty mode, the above code will temporarily require `1 + 4 + 7 + 10 + ... + 30001` = `150,025,000 bytes` of free memory on the Wasm heap!

The recommendation here is hence to be extremely cautious of containers and strings when building without `--spill-pointers`. It is advisable to perform std::vector style **geometric capacity growths** of memory for containers and strings when compiling under this mode to mitigate the quadratic memory growth issue.

### 🔢 WebAssembly SIMD

Emgc utilizes WebAssembly SIMD instruction set to speed up marking.

In a synthetic, possibly best-case performance test ([test/performance.c](test/performance.c)), Emgc achieves a and a 1128.32 MB/sec marking speed in scalar mode and a 3602.85 MB/sec marking speed with SIMD. (3.19x faster)

To enable SIMD optimizations, build with the `-msimd128` flag at both compile and link time.

# 🧪 Running Tests

Execute `python test.py` to run the test suite.
