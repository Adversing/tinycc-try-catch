/* Public libtcc lifecycle and real runtime allocator-boundary tests. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "libtcc.h"

#define NTHREADS 8
#define NCYCLES 3

struct worker {
    int (*fn)(void);
    int id, fail, result;
    long allocations, releases;
    HANDLE start, done, release;
};

static DWORD worker_key;
static volatile LONG live, allocations, releases, failed;
static int errors;

static struct worker *current(void)
{
    return (struct worker *)TlsGetValue(worker_key);
}

static void review_fail(int fail)
{
    current()->fail = fail;
}

static long review_allocations(void)
{
    return current()->allocations;
}

static int review_id(void)
{
    return current()->id;
}

static void *review_malloc(size_t size)
{
    struct worker *w = current();
    void *p;
    if (w != NULL && w->fail) {
        InterlockedIncrement(&failed);
        return NULL;
    }
    p = malloc(size);
    if (p != NULL) {
        InterlockedIncrement(&live);
        InterlockedIncrement(&allocations);
        if (w != NULL)
            ++w->allocations;
    }
    return p;
}

static void review_free(void *p)
{
    struct worker *w = current();
    if (p != NULL) {
        InterlockedDecrement(&live);
        InterlockedIncrement(&releases);
        if (w != NULL)
            ++w->releases;
    }
    free(p);
}

static void diagnostic(void *opaque, const char *message)
{
    (void)opaque;
    fprintf(stderr, "%s\n", message);
}

static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "FAIL %s\n", what);
        ++errors;
    }
}

static DWORD WINAPI worker_main(LPVOID arg)
{
    struct worker *w = arg;
    if (!TlsSetValue(worker_key, w))
        return 1;
    WaitForSingleObject(w->start, INFINITE);
    w->result = w->fn();
    SetEvent(w->done);
    /* Remain alive after generated code returns, including during deletion. */
    WaitForSingleObject(w->release, INFINITE);
    TlsSetValue(worker_key, NULL);
    return 0;
}

static TCCState *new_state(int argc, char **argv, const char *options)
{
    TCCState *s = tcc_new();
    int i;
    if (s == NULL)
        return NULL;
    tcc_set_error_func(s, NULL, diagnostic);
    for (i = 2; i < argc; ++i) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] == 'B')
            tcc_set_lib_path(s, a + 2);
        else if (a[0] == '-' && a[1] == 'I')
            tcc_add_include_path(s, a + 2);
        else if (a[0] == '-' && a[1] == 'L')
            tcc_add_library_path(s, a + 2);
    }
    tcc_set_options(s, options);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    return s;
}

