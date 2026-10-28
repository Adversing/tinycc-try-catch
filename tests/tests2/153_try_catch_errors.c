/* Each block below is selected in turn by tcc -dt.  Every block must fail
   with the diagnostic shown in 153_try_catch_errors.expect. */

#if defined test_try_only
#include <tcc_exception.h>
int main(void)
{
    try {
    }
    return 0;
}

#elif defined test_rethrow_outside
#include <tcc_exception.h>
int main(void)
{
    throw;
    return 0;
}

#elif defined test_goto_in
#include <tcc_exception.h>
int main(void)
{
    goto inside;
    try {
inside:
        return 0;
    }
    finally {
    }
    return 0;
}

#elif defined test_switch_in
#include <tcc_exception.h>
int main(void)
{
    int x = 1;
    switch (x) {
        try {
    case 1:
            return 0;
        }
        finally {
        }
    }
    return 0;
}

#elif defined test_finally_return
#include <tcc_exception.h>
int main(void)
{
    try {
    }
    finally {
        return 0;
    }
}

#elif defined test_finally_goto
#include <tcc_exception.h>
int main(void)
{
    int n = 0;
    try {
    }
    finally {
        goto out;
    }
out:
    return n;
}

#elif defined test_finally_break
#include <tcc_exception.h>
int main(void)
{
    int i;
    for (i = 0; i < 1; ++i) {
        try {
        }
        finally {
            break;
        }
    }
    return 0;
}

#elif defined test_keyword_var
#include <tcc_exception.h>
int try;
int main(void)
{
    return 0;
}

#elif defined test_keyword_member
#include <tcc_exception.h>
struct S {
    int catch;
};
int main(void)
{
    return 0;
}

#elif defined test_keyword_tag
#include <tcc_exception.h>
struct finally {
    int x;
};
int main(void)
{
    return 0;
}

#elif defined test_keyword_label
#include <tcc_exception.h>
int main(void)
{
finally:
    return 0;
}

#elif defined test_keyword_param
#include <tcc_exception.h>
int f(int throw)
{
    return throw;
}
int main(void)
{
    return f(0);
}

#elif defined test_keyword_enum
#include <tcc_exception.h>
enum { throw = 1 };
int main(void)
{
    return throw - 1;
}

#elif defined test_missing_header
struct tcc_exception_type {
    const struct tcc_exception_type *parent;
    const char *name;
};
static const struct tcc_exception_type E = { 0, "e" };
int main(void)
{
    try {
    }
    finally {
    }
    return 0;
}

#elif defined test_computed_goto
#include <tcc_exception.h>
int main(void)
{
    void *p = &&target;
    try {
    }
    finally {
    }
target:
    return p != 0;
}

#elif defined test_throw_negative_size
#include <tcc_exception.h>
static const struct tcc_exception_type E = { &TCC_Exception, "e" };
int main(void)
{
    int x;
    throw E, &x, -1;
    return 0;
}

#elif defined test_throw_expr
#include <tcc_exception.h>
int main(void)
{
    return try;
}
#endif
