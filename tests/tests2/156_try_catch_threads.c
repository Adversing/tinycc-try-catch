#include <stdio.h>
#include <string.h>
#include <tcc_exception.h>
#include "threads.h"

#define NTHREADS 8
#define NTHROWS 1000

static const struct tcc_exception_type E_THREAD = {
    &TCC_Exception, "test.thread"
};
static const struct tcc_exception_type E_THREAD_CHILD = {
    &E_THREAD, "test.thread.child"
};

static void *thread_main(void *p)
{
    int id = (int)(size_t)p;
    int i, j, errors = 0;

    for (i = 0; i < NTHROWS; ++i) {
        unsigned char payload[8];

        for (j = 0; j < 8; ++j)
            payload[j] = (unsigned char)(id * 100 + i + j);
        try {
            int local = i + 1;

            if (i & 1)
                throw E_THREAD_CHILD, payload, sizeof payload;
            if (local != i + 1)
                ++errors;
        }
        catch (E_THREAD_CHILD e) {
            if (e.payload == NULL || e.payload_size != sizeof payload
                || e.type != &E_THREAD_CHILD) {
                ++errors;
            } else {
                for (j = 0; j < 8; ++j)
                    if (((const unsigned char *)e.payload)[j]
                        != (unsigned char)(id * 100 + i + j))
                        ++errors;
            }
        }
        catch (E_THREAD) {
            errors += 100;
        }
        finally {
            if (i < 0)
                ++errors;
        }
    }
    return (void *)(size_t)errors;
}

int main(void)
{
    pthread_t th[NTHREADS];
    int i, errors = 0;

    for (i = 0; i < NTHREADS; ++i) {
        if (pthread_create(&th[i], NULL, thread_main, (void *)(size_t)i)) {
            printf("thread create failed\n");
            return 1;
        }
    }
    for (i = 0; i < NTHREADS; ++i) {
        void *ret = NULL;

        pthread_join(th[i], &ret);
        errors += (int)(size_t)ret;
    }
    printf("try/catch threads errors=%d\n", errors);
    return errors != 0;
}
