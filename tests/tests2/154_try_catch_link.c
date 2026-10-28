#include <stdio.h>
#include <stdlib.h>
#include <tcc_exception.h>

extern const struct tcc_exception_type E_LINK;
extern const struct tcc_exception_type E_LINK_CHILD;

extern void link_throw_shared(void);

/* The runtime must stay alive until after user atexit callbacks run. */
static void atexit_check(void)
{
    int matched = 0;

    try {
        link_throw_shared();
    }
    catch (E_LINK e) {
        matched = e.type == &E_LINK_CHILD;
    }
    printf("try/catch atexit: %s\n", matched ? "ok" : "FAIL");
}

int main(void)
{
    int matched = 0;

    atexit(atexit_check);
    try {
        link_throw_shared();
    }
    catch (E_LINK e) {
        matched = e.type == &E_LINK_CHILD;
    }
    printf("try/catch link: %s\n", matched ? "ok" : "FAIL");
    return !matched;
}
