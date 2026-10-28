#include <stdio.h>
#include <stddef.h>

/* Enabling the option keeps computed goto in functions without protected
   constructs, including TinyCC's existing VLA restoration behavior. */
static int computed(int n)
{
    int iteration = 0;
    size_t first = 0;
    void *target = &&again;
again:
    ;
    int a[n];
    a[0] = 42;
    if (!first)
        first = (size_t)a;
    if (first != (size_t)a)
        return 1;
    if (++iteration < 20)
        goto *target;
    return a[0] != 42;
}

int main(void)
{
    int result = computed(128);
    printf("ordinary control flow failures=%d\n", result);
    return result;
}
