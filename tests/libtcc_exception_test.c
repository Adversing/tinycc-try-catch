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

int main(int argc, char **argv)
{
    int cycle, i;
    if (argc < 2)
        return 2;
    worker_key = TlsAlloc();
    if (worker_key == TLS_OUT_OF_INDEXES)
        return 2;
    for (cycle = 0; cycle < NCYCLES; ++cycle) {
        TCCState *s = new_state(argc, argv, "-ftry-catch");
        struct worker workers[NTHREADS];
        HANDLE threads[NTHREADS], done[NTHREADS];
        HANDLE start = CreateEvent(NULL, TRUE, FALSE, NULL);
        HANDLE release = CreateEvent(NULL, TRUE, FALSE, NULL);
        int (*fn)(void);
        if (s == NULL || start == NULL || release == NULL)
            return 2;
        tcc_add_symbol(s, "malloc", review_malloc);
        tcc_add_symbol(s, "free", review_free);
        tcc_add_symbol(s, "review_fail", review_fail);
        tcc_add_symbol(s, "review_allocations", review_allocations);
        tcc_add_symbol(s, "review_id", review_id);
        if (tcc_add_file(s, argv[1]) < 0 || tcc_relocate(s) < 0)
            return 2;
        fn = (int (*)(void))tcc_get_symbol(s, "review_worker");
        if (fn == NULL)
            return 2;
        memset(workers, 0, sizeof workers);
        for (i = 0; i < NTHREADS; ++i) {
            workers[i].fn = fn;
            workers[i].id = i;
            workers[i].start = start;
            workers[i].release = release;
            workers[i].done = done[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (done[i] == NULL)
                return 2;
            threads[i] = CreateThread(NULL, 0, worker_main, &workers[i], 0, NULL);
            if (threads[i] == NULL)
                return 2;
        }
        SetEvent(start);
        if (WaitForMultipleObjects(NTHREADS, done, TRUE, 15000) != WAIT_OBJECT_0)
            return 2;
        for (i = 0; i < NTHREADS; ++i) {
            check(workers[i].result == 0, "generated worker");
            check(workers[i].allocations > 0
                  && workers[i].allocations == workers[i].releases,
                  "per-thread exception ownership");
        }
        check(live == 0, "no live exceptions before runtime deletion");
        tcc_delete(s);
        SetEvent(release);
        if (WaitForMultipleObjects(NTHREADS, threads, TRUE, 15000) != WAIT_OBJECT_0)
            return 2;
        for (i = 0; i < NTHREADS; ++i) {
            DWORD status;
            check(GetExitCodeThread(threads[i], &status) && status == 0,
                  "worker teardown after runtime deletion");
            check(workers[i].allocations == workers[i].releases,
                  "ownership after worker teardown");
            CloseHandle(threads[i]);
            CloseHandle(done[i]);
        }
        CloseHandle(start);
        CloseHandle(release);
        check(live == 0, "no live exceptions after runtime and worker teardown");
    }
    {
        TCCState *s = new_state(argc, argv, "-ftry-catch -fno-try-catch");
        check(tcc_compile_string(s,
              "#ifdef __TINYC_TRY_CATCH__\n#error unexpected extension\n#endif\n"
              "int try; int main(void) { return 0; }") == 0,
              "last option disables extension in an independent state");
        tcc_delete(s);
        s = new_state(argc, argv, "-fno-try-catch -ftry-catch");
        check(tcc_compile_string(s,
              "#if __TINYC_TRY_CATCH__ != 1\n#error missing extension\n#endif\n"
              "int main(void) { return 0; }") == 0,
              "last option enables extension in an independent state");
        tcc_delete(s);
    }
    check(allocations > 0 && allocations == releases && live == 0 && failed > 0,
          "allocator boundary accounting and failure injection");
    TlsFree(worker_key);
    printf("exception allocations=%ld releases=%ld live=%ld failures=%ld\n",
           allocations, releases, live, failed);
    printf("libtcc exception failures=%d\n", errors);
    return errors != 0;
}
