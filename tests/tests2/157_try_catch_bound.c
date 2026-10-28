#include <tcc_exception.h>

static const struct tcc_exception_type E_BOUND = {
    &TCC_Exception, "test.bound"
};

int main(void)
{
    int a[4];
    int i;

    try {
        throw E_BOUND;
    }
    catch (E_BOUND) {
        a[0] = 1;
    }
    i = 5;
    a[i] = 2;
    return a[0];
}
