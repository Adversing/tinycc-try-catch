#include <tcc_exception.h>

#if defined test_foreign_throw
int main(void)
{
    struct tcc_exception_type {
        const void *parent;
        const char *name;
    };
    static const struct tcc_exception_type Fake = { &TCC_Exception, "fake" };
    throw Fake;
}
#elif defined test_foreign_catch
int main(void)
{
    struct tcc_exception_type { int unrelated; };
    static const struct tcc_exception_type Fake = { 0 };
    try { }
    catch (Fake e) { }
    return 0;
}
#endif
