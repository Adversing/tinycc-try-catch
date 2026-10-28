#include <tcc_exception.h>

const struct tcc_exception_type E_LINK = {
    &TCC_Exception, "test.link"
};
const struct tcc_exception_type E_LINK_CHILD = {
    &E_LINK, "test.link.child"
};

void link_throw_shared(void)
{
    throw E_LINK_CHILD;
}
