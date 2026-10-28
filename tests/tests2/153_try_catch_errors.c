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

#endif
