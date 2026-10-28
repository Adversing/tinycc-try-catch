# tcc-try-catch

A fork of [TinyCC (TCC)](https://bellard.org/tcc/) that brings native `try`, `catch`, `finally`, and `throw` exception handling to C.

This fork implements exception handling in TCC, including keyword parsing, code generation and a dedicated runtime.

## Quick Start

To use exceptions, include `<tcc_exception.h>` and compile your code with the `-ftry-catch` flag:

```bash
tcc -ftry-catch program.c -o program
```

```c
#include <stdio.h>
#include <tcc_exception.h>

static const struct tcc_exception_type E_IO = {
    &TCC_Exception, "app.io"
};

int main(void)
{
    try {
        throw E_IO;
    }
    catch (E_IO e) {
        puts(e.type->name);
    }
    finally {
        puts("done");
    }
    return 0;
}
```

## How It Works

### Compiler Flags

Exception support is **disabled by default** to preserve standard C compatibility.

* Enable it with `-ftry-catch`.
* Disable it with `-fno-try-catch`.
* When multiple flags are passed, the last one on the command line takes precedence.
* Compiling code that uses exceptions requires both this fork's compiler and `<tcc_exception.h>`.

### Exception Categories & Matching

* Exception categories are `const` descriptor objects (`struct tcc_exception_type`). A `catch` matches its category or a descendant.
* Use `catch (...)` to handle any exception category.
* A standalone `throw;` rethrows the exception caught by the lexically enclosing `catch`.

### Payloads & Context

* `throw TYPE, address, size;` copies a byte payload into runtime-managed storage.
* A `catch` binding provides the exception's category, payload and throw location.
* Each thread has its own exception handler stack.

### Resource Cleanup with `finally`

A `finally` block is guaranteed to run when leaving a `try`/`catch` construct—whether through normal execution, an unhandled exception propagating up the call stack, or explicit control-flow jumps (`return`, `break`, `continue`, `goto`).

> **Note:** `finally` is designed for releasing acquired resources, but finalization is **not** guaranteed upon abrupt process termination (e.g., `exit()`, `abort()`) or when bypassing the runtime via a raw `longjmp`.

## Platform Support & Status

| Platform | Native Executables | `-run` / In-Memory `libtcc` | Notes |
| :--- | :---: | :---: | :--- |
| **Windows (x86_64)** | Tested | Tested | Tested with separately compiled objects, threads, and bounds checking. |
| **Linux (x86_64)** | Tested | Unsupported | `-run` and in-memory `libtcc` execution are currently blocked by the runtime's use of Thread-Local Storage (TLS). |
| **Other Targets** | Untested | Untested | — |

### Known Limitations

* Propagation across DLL boundaries is not supported.
* On Ubuntu, tests using `cleanup` or `constructor` attributes need adjustment for system `glibc` headers.

## Documentation & Examples

* [tcc-doc.texi](tcc-doc.texi): "Try, catch and finally" specifies the extension.
* [examples/try_catch_resource.c](examples/try_catch_resource.c): file and buffer cleanup with `finally`.
* [examples/try_catch_scanf.c](examples/try_catch_scanf.c): error handling with standard I/O.

## License

TinyCC is distributed under the **GNU Lesser General Public License (LGPL)**. See the `COPYING` file for full license details.

Original TinyCC compiler by Fabrice Bellard.
