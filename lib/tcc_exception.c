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
/* per-thread top frame */

#ifdef _WIN32

static DWORD tls_key = TLS_OUT_OF_INDEXES;
static volatile LONG tls_state; /* 0=uninit, 1=busy, 2=ready, 3=failed, 4=off */

static void tls_init(void)
{
    LONG old;
    DWORD key;

    for (;;) {
        old = InterlockedCompareExchange(&tls_state, 1, 0);
        if (old == 0) {
            key = TlsAlloc();
            if (key == TLS_OUT_OF_INDEXES) {
                InterlockedExchange(&tls_state, 3);
                try_fatal("cannot allocate TLS key");
            }
            tls_key = key;
            InterlockedExchange(&tls_state, 2);
            return;
        }
        if (old == 2)
            return;
        if (old == 3)
            try_fatal("TLS initialization failed earlier");
        if (old == 4)
            try_fatal("runtime used after shutdown");
        Sleep(0);
    }
}

static struct __tcc_try_frame *tls_get_top(void)
{
    tls_init();
    return (struct __tcc_try_frame *)TlsGetValue(tls_key);
}

static void tls_set_top(struct __tcc_try_frame *frame)
{
    if (!TlsSetValue(tls_key, frame))
        try_fatal("cannot update TLS frame pointer");
}

void __tcc_try_shutdown(void)
{
    LONG old = InterlockedExchange(&tls_state, 4);
    DWORD key;

    if (old == 2) {
        key = tls_key;
        tls_key = TLS_OUT_OF_INDEXES;
        TlsSetValue(key, NULL);
        TlsFree(key);
    }
}

#else

static __thread struct __tcc_try_frame *tls_top;

static struct __tcc_try_frame *tls_get_top(void)
{
    return tls_top;
}

static void tls_set_top(struct __tcc_try_frame *frame)
{
    tls_top = frame;
}

void __tcc_try_shutdown(void)
{
    tls_top = NULL;
}

#endif

/* ------------------------------------------------------------------------- */
/* exception objects */

static void exc_retain(struct __tcc_try_exception *e)
{
    if (e->refs != TCC_EXC_IMMORTAL)
        ++e->refs;
}

static void exc_release(struct __tcc_try_exception *e)
{
    if (!e || e->refs == TCC_EXC_IMMORTAL)
        return;
    if (--e->refs == 0)
        free(e->alloc_base);
}

static struct __tcc_try_exception *exc_new(const struct tcc_exception_type *type,
                                           const void *payload,
                                           size_t size,
                                           const char *file,
                                           int line)
{
    struct __tcc_try_exception *e;
    char *p;
    size_t total;

    total = sizeof *e + TCC_EXC_ALIGN - 1 + size;
    if (total < sizeof *e || total < size)
        return NULL;
    e = (struct __tcc_try_exception *)malloc(total);
    if (!e)
        return NULL;
    memset(e, 0, sizeof *e);
    e->refs = 1;
    e->alloc_base = e;
    e->view.type = type;
    e->view.payload_size = size;
    e->view.file = file;
    e->view.line = line;
    if (size) {
        p = (char *)(e + 1);
        p = (char *)(((size_t)p + TCC_EXC_ALIGN - 1) & ~(size_t)(TCC_EXC_ALIGN - 1));
        memcpy(p, payload, size);
        e->view.payload = p;
    }
    return e;
}

static struct __tcc_try_exception *invalid_exception(const char *file, int line)
{
    struct __tcc_try_exception *e;
    e = exc_new(&TCC_InvalidException, NULL, 0, file, line);
    return e ? e : &emergency_invalid;
}

/* Validate an immutable category chain without an arbitrary depth limit. */
static int valid_category(const struct tcc_exception_type *type)
{
    const struct tcc_exception_type *slow, *fast, *p;

    if (!type)
        return 0;
    if (type == &TCC_Exception)
        return 1;

    slow = type;
    fast = type;
    while (fast && fast->parent) {
        slow = slow->parent;
        fast = fast->parent->parent;
        if (slow == fast)
            return 0;
    }
    for (p = type; p; p = p->parent)
        if (p == &TCC_Exception)
            return 1;
    return 0;
}

