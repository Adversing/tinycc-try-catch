#include <stdio.h>
#include <stdlib.h>
#include <tcc_exception.h>

static const struct tcc_exception_type E = { &TCC_Exception, "atexit.ctor" };

static void finish(void)
{
    try { throw E; }
    catch (E e) { printf("constructor atexit: %s\n", e.type == &E ? "ok" : "FAIL"); }
}

static void init(void) __attribute__((constructor));
static void init(void)
{
    if (atexit(finish) != 0)
        abort();
}

int main(void)
{
    try { }
    finally { puts("constructor main: ok"); }
    return 0;
}
