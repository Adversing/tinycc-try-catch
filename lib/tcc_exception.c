/*
 * TinyCC exception runtime.
 *
 * The compiler lowers try/catch/finally to a stack frame checkpoint and
 * calls the helpers in this file.  Exception objects are reference counted
 * and owned by the runtime; a catch binding is only a copied public view.
 */

#define __TINYC_TRY_CATCH__ 1
#include "include/tcc_exception.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define TCC_EXC_ALIGN 16
#define TCC_EXC_IMMORTAL ((unsigned long)-1)

struct __tcc_try_exception {
    struct tcc_exception view;
    unsigned long refs;
    void *alloc_base;
};

const struct tcc_exception_type TCC_Exception = {
    NULL, "TCC_Exception"
};
const struct tcc_exception_type TCC_OutOfMemory = {
    &TCC_Exception, "TCC_OutOfMemory"
};
const struct tcc_exception_type TCC_InvalidException = {
    &TCC_Exception, "TCC_InvalidException"
};

static struct __tcc_try_exception emergency_oom = {
    { &TCC_OutOfMemory, NULL, 0, "<runtime>", 0 },
    TCC_EXC_IMMORTAL, NULL
};
static struct __tcc_try_exception emergency_invalid = {
    { &TCC_InvalidException, NULL, 0, "<runtime>", 0 },
    TCC_EXC_IMMORTAL, NULL
};

/* ------------------------------------------------------------------------- */
/* diagnostics */

static void try_fatal(const char *msg)
{
    fflush(stdout);
    fprintf(stderr, "tcc try/catch runtime: %s\n", msg);
    fflush(stderr);
    abort();
}

static void uncaught_exception(struct __tcc_try_exception *e)
{
    const char *name = "<unknown>";
    const char *file = "<unknown>";
    int line = 0;

    if (e) {
        if (e->view.type && e->view.type->name)
            name = e->view.type->name;
        if (e->view.file)
            file = e->view.file;
        line = e->view.line;
    }
    fflush(stdout);
    fprintf(stderr, "tcc: uncaught exception '%s' at %s:%d\n",
            name, file, line);
    fflush(stderr);
    abort();
}

/* ------------------------------------------------------------------------- */
