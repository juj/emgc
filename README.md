# Emgc

A tiny low-level research garbage collector to be used solely on the Emscripten WebAssembly platform.

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
  gc_free(data); // Explicitly frees up the allocated memory. May be useful to immediately force huge allocs to go away.
}
```

# Usage

To use emgc in your program, compile the file `emgc.c` along with your program code, and `#include "emgc.h"`.

Additionally, you must choose one of the two possible stack scanning modes in order to use Emgc. See the section [Stack Scanning](#stack-scanning) below.

# Details

See the following sections for more detailed information on Emgc:
 - [Pointer Identification](#pointer-identification)
 - [Global Memory](#global-memory)
 - [Roots and Leaves](#roots-and-leaves)
 - [Stack Scanning](#stack-scanning)

### Pointer Identification

During marking, Emgc scans raw memory regions to identify any values that could look like managed pointers. This process requires that all pointer values are stored at aligned addresses, i.e. at addresses that are multiples of 4 in a 32-bit build, and at multiples of 8 in a 64-bit build. Pointers that would be stored at unaligned addresses would go undetected by the marking process.

All pointers need to point to the starting address of the memory buffer. Emgc does not detect pointers that point to the interior address of a managed allocation.

### Global Memory

By default, Emgc scans (i.e. marks) all static data (the memory area holding global variables) during garbage collection to find managed pointers.

This is convenient for getting started, although in a larger application, the static data section can grow large and typically only few global variables might hold managed pointers, so this automatic scanning of global data can become inefficient.

To disable automatic static data marking, pass the define `-DEMGC_SKIP_AUTOMATIC_STATIC_MARKING=1` when compiling `emgc.c`.

### Roots and Leaves

Managed allocations may be specialized into two flavors: roots and leaves.

A managed allocation may be declared as a **root allocation** with the `gc_make_root(ptr)` function. A root allocation is one that is always assumed to be reachable by the collector, and will never be freed by automatically by the GC process. A manual call to `gc_free(ptr)` is required to free a root allocation. For example:

```c
#include "emgc.h"

int **global;

int main()
{
    global = (int**)gc_malloc(128);
    gc_make_root(global);

    global[0] = (int*)gc_malloc(42);
    gc_collect(); // will not free memory.
}
```

During garbage collection, all root pointers are always scanned.

Even if the program is compiled with `-DEMGC_SKIP_AUTOMATIC_STATIC_MARKING=1`, the above code will not free up any memory, since `global` is declared to be a root, and it references the second allocation, so both are kept alive.

### Stack Scanning

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

- In the collect-only-when-stack-is-empty mode, the application will be unable to resolve any OOM situations by collecting on the spot inside a `gc_malloc()` call. If the application developer knows they will not perform too many temp allocations, this might not sound too bad; but there is a grave gotcha:

Any code that performs a linear number of linearly growing temporary calls to `gc_malloc()`, will turn into a quadratic memory usage under the collect-only-when-stack-is-empty scheme. For example, the following code:

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

The above code generates a long string by concatenating `"foo"` 10000 times. If the heap is about to run out of memory, `gc_malloc()` can collect garbage on demand if building with the `--spill-pointers` flag. This means that the above code will first consume some amount of temporary memory (as the available heap size permits), which will be promptly collected, and then finally the code persists `10000 * strlen("foo")+1` == `30001 bytes` of memory for the string at the end of the function.

If emgc is not able to collect except when the stack is empty, the above code will instead require there to be `1 + 4 + 7 + 10 + ... + 30001` = `150,025,000 bytes` of free memory on the Wasm heap!

The recommendation here is hence to be extremely cautious of containers and strings when building without `--spill-pointers`. It is advisable to perform std::vector style **geometric capacity growths** of memory for containers and strings when compiling under this mode to mitigate the quadratic memory growth issue.

# Testing

Execute `python test.py` to run the test suite.