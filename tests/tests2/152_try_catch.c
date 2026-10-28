#include <stdio.h>
#include <string.h>
#include <tcc_exception.h>

#ifndef __TINYC_TRY_CATCH__
#error __TINYC_TRY_CATCH__ must be defined for this test
#endif
#if __TINYC_TRY_CATCH__ != 1
#error __TINYC_TRY_CATCH__ must be 1
#endif

static const struct tcc_exception_type E_BASE = {
    &TCC_Exception, "test.base"
};
static const struct tcc_exception_type E_CHILD = {
    &E_BASE, "test.child"
};
static const struct tcc_exception_type E_OTHER = {
    &TCC_Exception, "test.other"
};

static int fails;
static int finals;
static char trace[32];

#define CHECK(c) do { \
    if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); ++fails; } \
} while (0)

static void note(char c)
{
    size_t n = strlen(trace);
    trace[n] = c;
    trace[n + 1] = 0;
}

struct pair { long a; double b; };

int main(void)
{
    int n, matched;
    struct pair p;
    unsigned char data[4];

    trace[0] = 0;
    finals = 0;

    /* Source order and descriptor identity. */
    matched = 0;
    try {
        throw E_CHILD;
    }
    catch (E_BASE e) {
        matched = e.type == &E_CHILD;
    }
    catch (E_CHILD e) {
        matched = 0;
    }
    CHECK(matched);

    /* A sibling branch must not match. */
    matched = 0;
    try {
        throw E_CHILD;
    }
    catch (E_OTHER) {
        matched = -1;
    }
    catch (E_BASE) {
        matched = 1;
    }
    CHECK(matched == 1);

    /* Catch-all. */
    matched = 0;
    try {
        throw E_OTHER;
    }
    catch (...) {
        matched = 1;
    }
    CHECK(matched);

    /* Payload is copied before finally can change the source. */
    data[0] = 1; data[1] = 2; data[2] = 3; data[3] = 4;
    matched = 0;
    try {
        try {
            throw E_CHILD, data, sizeof data;
        }
        finally {
            data[0] = 9;
        }
    }
    catch (E_CHILD e) {
        matched = e.payload_size == sizeof data
                  && memcmp(e.payload, "\1\2\3\4", 4) == 0;
    }
    CHECK(matched);

    /* Rethrow keeps the concrete type and source location. */
    matched = 0;
    try {
        try {
            throw E_CHILD;
        }
        catch (E_BASE e) {
            CHECK(e.type == &E_CHILD);
            throw;
        }
    }
    catch (E_CHILD e) {
        matched = e.type == &E_CHILD;
    }
    CHECK(matched);

    /* Rethrow from an unbound catch. */
    matched = 0;
    try {
        try {
            throw E_OTHER;
        }
        catch (...) {
            ++matched;
            throw;
        }
    }
    catch (E_OTHER) {
        ++matched;
    }
    CHECK(matched == 2);

    /* Invalid payload arguments become TCC_InvalidException. */
    matched = 0;
    try {
        throw E_CHILD, (void *)0, 1;
    }
    catch (TCC_InvalidException e) {
        matched = e.type == &TCC_InvalidException;
    }
    CHECK(matched);

    /* Allocation failure becomes TCC_OutOfMemory. */
    matched = 0;
    try {
#if __SIZEOF_POINTER__ == 8
        throw E_CHILD, (void *)1, (size_t)1 << 60;
#else
        throw E_CHILD, (void *)1, (size_t)1 << 30;
#endif
    }
    catch (TCC_OutOfMemory e) {
        matched = e.type == &TCC_OutOfMemory
                  && e.payload == NULL && e.payload_size == 0;
    }
    CHECK(matched);

    printf("try/catch core failures=%d\n", fails);
    return fails != 0;
}
