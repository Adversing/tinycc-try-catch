#include <stdio.h>
#include <tcc_exception.h>

static const struct tcc_exception_type E_UNCAUGHT = {
    &TCC_Exception, "test.uncaught"
};

static int finals;

static void deep(void)
{
    try {
        throw E_UNCAUGHT;
    }
    finally {
        ++finals;
    }
}

int main(void)
{
    try {
        deep();
    }
    finally {
        ++finals;
        printf("finalizers=%d\n", finals);
        fflush(stdout);
    }
    printf("not reached\n");
    return 0;
}
