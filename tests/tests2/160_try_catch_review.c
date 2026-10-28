#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcc_exception.h>

typedef struct tcc_exception_type category;
typedef struct tcc_exception record;
static const category E = { &TCC_Exception, "review.e" };
static const category F = { &TCC_Exception, "review.f" };
static int failures, cleaned, finalized;
static int throw_cleanup;
static char trace[64];

#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void note(char c)
{
    size_t n = strlen(trace);
    CHECK(n + 1 < sizeof trace);
    if (n + 1 < sizeof trace) {
        trace[n] = c;
        trace[n + 1] = 0;
    }
}

static void clean(int *p)
{
    cleaned += *p;
    note('c');
    if (throw_cleanup) {
        throw_cleanup = 0;
        throw E;
    }
}

void review_clean_raw(void *p)
{
    int n;
    memcpy(&n, p, sizeof n);
    cleaned += n;
}

static void types(void)
{
    struct tcc_exception_type { int unrelated; };
    struct tcc_exception { int unrelated; };
    static const category Local = { &E, "review.local" };
    int caught = 0;

    try {
        try { throw Local; }
        catch (E e) {
            CHECK(sizeof e == sizeof(record));
            CHECK(e.type == &Local);
            CHECK(strcmp(e.type->name, "review.local") == 0);
            throw;
        }
    }
    catch (Local e) {
        CHECK(e.type == &Local && e.payload_size == 0);
        caught = 1;
    }
    CHECK(caught);
}

static void plain_cleanup(void)
{
    {
        int x __attribute__((cleanup(clean))) = 7;
        goto out;
    }
out:
    CHECK(cleaned == 7);
}

static void cleanup(void)
{
    cleaned = 0;
    trace[0] = 0;
    plain_cleanup();
    cleaned = finalized = 0;
    trace[0] = 0;
    {
        int outer __attribute__((cleanup(clean))) = 3;
        try {
            struct local { int n; };
            void review_clean_raw(void *);
            struct local x __attribute__((cleanup(review_clean_raw))) = { 7 };
            goto done;
        }
        finally {
            CHECK(cleaned == 7);
            note('f');
            ++finalized;
        }
done:
        CHECK(cleaned == 7 && finalized == 1);
    }
    CHECK(cleaned == 10 && strcmp(trace, "fc") == 0);

    cleaned = finalized = 0;
    throw_cleanup = 1;
    trace[0] = 0;
    try {
        int x __attribute__((cleanup(clean))) = 5;
        goto cancelled;
    }
    catch (E) {
        note('h');
    }
    finally {
        ++finalized;
        note('f');
    }
    CHECK(cleaned == 5 && finalized == 1);
    CHECK(strcmp(trace, "chf") == 0);
    goto after_cancelled;
cancelled:
    CHECK(0);
after_cancelled:
    cleaned = 0;
    trace[0] = 0;
    throw_cleanup = 1;
    try {
        {
            int x __attribute__((cleanup(clean))) = 2;
            try { goto outer_done; }
            finally { note('i'); }
        }
outer_done:
        CHECK(0);
    }
    catch (E) { note('o'); }
    CHECK(cleaned == 2 && strcmp(trace, "ico") == 0);

    cleaned = 0;
    {
        int x __attribute__((cleanup(clean))) = 4;
        int n = 0;
        goto inside;
inside:
        CHECK(cleaned == 0);
        if (++n < 2)
            goto inside;
    }
    CHECK(cleaned == 4);
}

static void labels(void)
{
    int n = 0;

    try {
        __label__ target;
        goto target;
target:
        ++n;
    }
    finally {
        __label__ target;
        goto target;
target:
        n += 2;
    }
    {
        __label__ target;
        goto target;
target:
        n += 4;
    }
    CHECK(n == 7);
}

static void clobber(void)
{
    volatile int scratch[2048];
    int i;
    for (i = 0; i < 2048; ++i)
        scratch[i] = 99;
}

static int *live_vla;
static void clean_vla(int *p)
{
    clobber();
    CHECK(live_vla[0] == 42 && live_vla[*p - 1] == 42);
}

static int nested_vla_return(int n)
{
    int a[n], i;
    for (i = 0; i < n; ++i)
        a[i] = 42;
    try {
        int b[n];
        for (i = 0; i < n; ++i)
            b[i] = 17;
        try { return a[0] + b[n - 1]; }
        finally {
            clobber();
            CHECK(a[0] == 42 && b[n - 1] == 17);
        }
    }
    finally {
        clobber();
        CHECK(a[0] == 42 && a[n - 1] == 42);
    }
}

static void backward_vla(int n)
{
    int iteration = 0, i;
    size_t first = 0;
before_declaration:
    ;
    int a[n];
    for (i = 0; i < n; ++i)
        a[i] = 42;
    if (!first)
        first = (size_t)a;
    CHECK((size_t)a == first);
    try {
        if (++iteration < 20)
            goto before_declaration;
    }
    finally {
        clobber();
        CHECK(a[0] == 42 && a[n - 1] == 42);
    }
}

static void backward_second_vla(int n)
{
    int iteration = 0;
    size_t first = 0;
    int a[n];
    a[0] = 17;
between_declarations:
    ;
    int b[n];
    b[0] = 42;
    if (!first)
        first = (size_t)b;
    CHECK((size_t)b == first);
    try {
        if (++iteration < 20)
            goto between_declarations;
    }
    finally {
        clobber();
        CHECK(a[0] == 17 && b[0] == 42);
    }
}

static void vla(void)
{
    int n = 128, i, rounds = 0;
    int a[n];

    for (i = 0; i < n; ++i)
        a[i] = 42;
    try { goto out; }
    finally {
        clobber();
        CHECK(a[0] == 42 && a[n - 1] == 42);
    }
out:
    clobber();
    CHECK(a[0] == 42 && a[n - 1] == 42);
again:
    if (rounds++ < 3) {
        try { goto again; }
        finally {
            clobber();
            CHECK(a[0] == 42 && a[n - 1] == 42);
        }
    }
    try {
        int m = n;
        int b[m];
        int len __attribute__((cleanup(clean_vla))) = m;
        for (i = 0; i < m; ++i)
            b[i] = 42;
        live_vla = b;
    }
    finally { CHECK(a[0] == 42); }
    CHECK(nested_vla_return(n) == 59);
    backward_vla(n);
    backward_second_vla(n);
    try {
        goto before_vla_done;
        {
            int m = n;
            int skipped[m];
            skipped[0] = 1;
        }
    }
    finally { CHECK(a[0] == 42); }
before_vla_done:
    CHECK(a[n - 1] == 42);
}

static void fresh_throw(void)
{
    int caught = 0;
    try { throw F; }
    catch (F) { caught = 1; }
    CHECK(caught);
}

int main(int argc, char **argv)
{
    int all = argc == 1;
    if (all || strcmp(argv[1], "types") == 0) { types(); fresh_throw(); }
    if (all || strcmp(argv[1], "cleanup") == 0) { cleanup(); fresh_throw(); }
    if (all || strcmp(argv[1], "labels") == 0) { labels(); fresh_throw(); }
    if (all || strcmp(argv[1], "vla") == 0) { vla(); fresh_throw(); }
    printf("try/catch review failures=%d\n", failures);
    return failures != 0;
}
