#include <string.h>
#include <tcc_exception.h>

static const struct tcc_exception_type E = { &TCC_Exception, "ownership.e" };
static const struct tcc_exception_type F = { &TCC_Exception, "ownership.f" };
extern void review_fail(int);
extern long review_allocations(void);
extern int review_id(void);
static void deep(void)
{
    try { throw E; }
    finally { }
}

static int replaced(void)
{
    try { return 7; }
    finally { throw F; }
}

int review_worker(void)
{
    int i, n = 0, id = review_id();
    long rethrow_allocs = -1;
    for (i = 0; i < 200; ++i) {
        int source[2], copy[2];
        source[0] = id;
        source[1] = i;
        try {
            try { throw E, source, sizeof source; }
            catch (E e) {
                try { deep(); } catch (E) { }
                rethrow_allocs = review_allocations();
                throw;
            }
            finally { source[0] = -1; }
        }
        catch (E e) {
            if (e.payload == NULL || e.payload_size != sizeof copy)
                return 1;
            memcpy(copy, e.payload, sizeof copy);
            if (copy[0] != id || copy[1] != i
                || review_allocations() != rethrow_allocs)
                return 2;
            ++n;
        }
        try { try { throw E; } finally { throw F; } }
        catch (F) { ++n; }
        try { replaced(); }
        catch (F) { ++n; }
        review_fail(1);
        try { throw E; }
        catch (TCC_OutOfMemory e) {
            try { throw F; }
            catch (TCC_OutOfMemory inner) {
                if (e.type != &TCC_OutOfMemory || e.type != inner.type
                    || e.payload != NULL || e.payload_size != 0
                    || strcmp(e.file, "<runtime>") || e.line != 0)
                    return 3;
            }
            ++n;
        }
        try { throw E, (void *)0, 1; }
        catch (TCC_InvalidException e) {
            if (e.type != &TCC_InvalidException || e.payload != NULL)
                return 4;
            ++n;
        }
        review_fail(0);
    }
    return n != 1000;
}
