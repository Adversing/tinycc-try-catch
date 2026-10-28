#include <stdio.h>

/* With the extension disabled these are ordinary identifiers. */
int try, catch, throw, finally;

static int f(int throw)
{
    int try = throw + 1;
    int catch = try + 1;
    int finally = catch + 1;

    return finally;
}

int main(void)
{
    try = 1;
    catch = 2;
    throw = 3;
    finally = 4;
    if (try + catch + throw + finally + f(0) != 13)
        return 1;
    printf("try/catch disabled: ok\n");
    return 0;
}
