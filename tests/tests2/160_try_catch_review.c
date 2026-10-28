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
    if (all || strcmp(argv[1], "labels") == 0) { labels(); fresh_throw(); }
    printf("try/catch review failures=%d\n", failures);
    return failures != 0;
}
