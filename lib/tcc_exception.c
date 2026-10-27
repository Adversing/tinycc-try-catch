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

static int category_matches(const struct tcc_exception_type *actual,
                            const struct tcc_exception_type *wanted)
{
    const struct tcc_exception_type *p;
    for (p = actual; p; p = p->parent)
        if (p == wanted)
            return 1;
    return 0;
}

static void throw_current(struct __tcc_try_exception *e)
{
    struct __tcc_try_frame *f = tls_get_top();

    if (!f)
        uncaught_exception(e);
    if (f->pending)
        exc_release(f->pending);
    f->pending = e;
    f->action = __TCC_TRY_ACTION_EXCEPTION;
    f->jump(f->env, 1);
    try_fatal("longjmp returned to throw site");
}

/* ------------------------------------------------------------------------- */
/* compiler-facing helpers */

void __tcc_try_enter(struct __tcc_try_frame *frame,
                     void (*jump)(jmp_buf, int),
                     unsigned long frame_size,
                     unsigned long frame_align,
                     unsigned long env_offset)
{
    if (frame_size != sizeof *frame
        || frame_align != (unsigned long)__alignof__(struct __tcc_try_frame)
        || env_offset != (unsigned long)offsetof(struct __tcc_try_frame, env))
        try_fatal("compiler/runtime frame ABI mismatch");
    if (!jump)
        try_fatal("missing longjmp entry point");

    frame->prev = tls_get_top();
    frame->pending = NULL;
    frame->caught = NULL;
    frame->jump = jump;
    frame->phase = __TCC_TRY_PHASE_TRY;
    frame->action = __TCC_TRY_ACTION_NORMAL;
    frame->exit_id = 0;
    frame->abi_size = (int)frame_size;
    tls_set_top(frame);
}

int __tcc_try_landing(struct __tcc_try_frame *frame)
{
    if (!frame || tls_get_top() != frame)
        try_fatal("landing on a frame that is not the current frame");
    return frame->phase;
}

int __tcc_try_match(struct __tcc_try_frame *frame,
                    const struct tcc_exception_type *type)
{
    struct __tcc_try_exception *e;

    if (!frame || frame->phase != __TCC_TRY_PHASE_TRY || !frame->pending)
        return 0;
    if (!category_matches(frame->pending->view.type, type))
        return 0;
    e = frame->pending;
    frame->pending = NULL;
    frame->caught = e;
    frame->phase = __TCC_TRY_PHASE_CATCH;
    frame->action = __TCC_TRY_ACTION_NORMAL;
    return 1;
}

int __tcc_try_match_any(struct __tcc_try_frame *frame)
{
    if (!frame || frame->phase != __TCC_TRY_PHASE_TRY || !frame->pending)
        return 0;
    frame->caught = frame->pending;
    frame->pending = NULL;
    frame->phase = __TCC_TRY_PHASE_CATCH;
    frame->action = __TCC_TRY_ACTION_NORMAL;
    return 1;
}

void __tcc_try_bind(struct __tcc_try_frame *frame, struct tcc_exception *out)
{
    if (!frame || !frame->caught)
        try_fatal("catch binding without a caught exception");
    if (out)
        *out = frame->caught->view;
}

void __tcc_try_normal(struct __tcc_try_frame *frame)
{
    if (!frame)
        try_fatal("normal completion without a frame");
    if (frame->pending) {
        exc_release(frame->pending);
        frame->pending = NULL;
    }
    frame->action = __TCC_TRY_ACTION_NORMAL;
    frame->phase = __TCC_TRY_PHASE_FINALLY;
}

void __tcc_try_begin_finally(struct __tcc_try_frame *frame, int action, int id)
{
    if (!frame)
        try_fatal("finalizer without a frame");
    frame->action = action;
    frame->exit_id = id;
    frame->phase = __TCC_TRY_PHASE_FINALLY;
}

static void pop_frame(struct __tcc_try_frame *frame)
{
    if (!frame || tls_get_top() != frame)
        try_fatal("frame stack corruption");
    if (frame->pending) {
        exc_release(frame->pending);
        frame->pending = NULL;
    }
    if (frame->caught) {
        exc_release(frame->caught);
        frame->caught = NULL;
    }
    tls_set_top(frame->prev);
    frame->prev = NULL;
}

int __tcc_try_finish(struct __tcc_try_frame *frame)
{
    int id;

    if (!frame)
        try_fatal("finish without a frame");

    if (frame->action == __TCC_TRY_ACTION_EXCEPTION) {
        struct __tcc_try_exception *e = frame->pending;
        struct __tcc_try_frame *prev;

        frame->pending = NULL;
        if (frame->caught) {
            exc_release(frame->caught);
            frame->caught = NULL;
        }
        prev = frame->prev;
        if (!frame || tls_get_top() != frame)
            try_fatal("frame stack corruption");
        tls_set_top(prev);
        frame->prev = NULL;

        if (!prev)
            uncaught_exception(e);
        if (prev->pending)
            exc_release(prev->pending);
        prev->pending = e;
        prev->action = __TCC_TRY_ACTION_EXCEPTION;
        prev->jump(prev->env, 1);
        try_fatal("longjmp returned during propagation");
    }

    id = frame->exit_id;
    pop_frame(frame);
    return id;
}

void __tcc_try_pop(struct __tcc_try_frame *frame)
{
    pop_frame(frame);
}

void __tcc_try_rethrow(struct __tcc_try_frame *frame)
{
    struct __tcc_try_exception *e;

    if (!frame || !frame->caught)
        try_fatal("rethrow without a caught exception");
    e = frame->caught;
    exc_retain(e);
    throw_current(e);
}

void __tcc_throw_new(const struct tcc_exception_type *type,
                     const void *payload,
                     unsigned long long size,
                     int size_is_signed,
                     const char *file,
                     int line)
{
    struct __tcc_try_exception *e;
    size_t sz;

    if (!valid_category(type)) {
        e = invalid_exception(file, line);
        throw_current(e);
    }

    if (size_is_signed && (long long)size < 0) {
        e = invalid_exception(file, line);
        throw_current(e);
    }
    if (size > (unsigned long long)(size_t)-1) {
        e = invalid_exception(file, line);
        throw_current(e);
    }
    sz = (size_t)size;
    if (sz != 0 && payload == NULL) {
        e = invalid_exception(file, line);
        throw_current(e);
    }
    if (sz > (size_t)-1
             - (sizeof(struct __tcc_try_exception) + TCC_EXC_ALIGN - 1)) {
        e = invalid_exception(file, line);
        throw_current(e);
    }

    e = exc_new(type, payload, sz, file, line);
    if (!e)
        e = &emergency_oom;
    throw_current(e);
}
