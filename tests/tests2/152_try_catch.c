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

static int ret_nested(void)
{
    int x = 3;

    try {
        try {
            return x;
        }
        finally {
            note('i');
            x = 4;
        }
    }
    finally {
        note('o');
        x = 5;
    }
}

struct pair {
    long a;
    double b;
};

static struct pair ret_struct(void)
{
    struct pair p = { 11, 2.25 };

    try {
        return p;
    }
    finally {
        p.a = 0;
        p.b = 0;
        ++finals;
    }
}

static int ret_double(void)
{
    double d = 1.25;

    try {
        return (int)(d * 4.0);
    }
    finally {
        d = 9.0;
        ++finals;
    }
}

static int loop_break(void)
{
    int i, n = 0;

    for (i = 0; i < 5; ++i) {
        try {
            if (i == 1)
                break;
            ++n;
        }
        finally {
            n += 10;
        }
    }
    return n;
}

static int loop_continue(void)
{
    int i, n = 0;

    for (i = 0; i < 4; ++i) {
        try {
            if (i == 1)
                continue;
            ++n;
        }
        finally {
            n += 10;
        }
    }
    return n;
}

static int goto_out(void)
{
    int n = 0;

    try {
        goto out;
    }
    finally {
        n += 2;
    }
out:
    return n;
}

static int goto_inside(void)
{
    int n = 0;

    try {
        goto inside;
inside:
        n = 7;
    }
    finally {
        n += 10;
    }
    return n;
}

static void deep_finally(void)
{
    try {
        note('d');
        throw E_CHILD;
    }
    finally {
        note('D');
        ++finals;
    }
}

static void middle_finally(void)
{
    try {
        deep_finally();
    }
    finally {
        note('M');
        ++finals;
    }
}

static void replace_pending(void)
{
    try {
        try {
            throw E_BASE;
        }
        finally {
            note('r');
            throw E_OTHER;
        }
    }
    catch (E_BASE e) {
        note('a');
    }
    catch (E_OTHER e) {
        note('o');
    }
}

static void return_replaced(void)
{
    try {
        return;
    }
    finally {
        throw E_OTHER;
    }
}

static void vla_and_locals(void)
{
    int n = 6;
    int a[n];
    int i;
    double d = 1.5;
    const char *p = "before";

    for (i = 0; i < n; ++i)
        a[i] = i * 2;
    try {
        int m = 4;
        int b[m];

        for (i = 0; i < m; ++i)
            b[i] = 100 + i;
        d = 6.25;
        p = "after";
        throw E_CHILD, &a[0], sizeof a[0];
    }
    catch (E_CHILD e) {
        int got;

        CHECK(a[0] == 0 && a[5] == 10);
        CHECK(d == 6.25);
        CHECK(strcmp(p, "after") == 0);
        got = *(const int *)e.payload;
        CHECK(got == 0);
    }
    finally {
        CHECK(a[3] == 6);
    }
}

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

    /* Returns run finalizers and keep the evaluated value. */
    trace[0] = 0;
    CHECK(ret_nested() == 3);
    CHECK(strcmp(trace, "io") == 0);

    finals = 0;
    CHECK(ret_double() == 5);
    CHECK(finals == 1);

    finals = 0;
    p = ret_struct();
    CHECK(p.a == 11 && p.b == 2.25);
    CHECK(finals == 1);

    /* break/continue/goto out run finalizers exactly once. */
    CHECK(loop_break() == 21);
    CHECK(loop_continue() == 43);
    CHECK(goto_out() == 2);
    CHECK(goto_inside() == 17);

    /* Propagation through called functions. */
    finals = 0;
    trace[0] = 0;
    try {
        middle_finally();
        CHECK(0 && "must throw");
    }
    catch (E_BASE e) {
        CHECK(e.type == &E_CHILD);
    }
    CHECK(finals == 2);
    CHECK(strcmp(trace, "dDM") == 0);

    /* A throw from finally replaces the pending action. */
    trace[0] = 0;
    replace_pending();
    CHECK(strcmp(trace, "ro") == 0);

    /* A throw from finally replaces a pending return. */
    matched = 0;
    try {
        return_replaced();
    }
    catch (E_OTHER e) {
        matched = 1;
    }
    CHECK(matched);

    vla_and_locals();

    printf("try/catch core failures=%d\n", fails);
    return fails != 0;
}
