#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcc_exception.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int fails;
static int allocs;
static int frees;
static int closes;
static int force_read_error;
static int force_close_error;

#define CHECK(c) do { \
    if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); ++fails; } \
} while (0)

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (p != NULL)
        ++allocs;
    return p;
}

static void xfree(void *p)
{
    if (p != NULL) {
        ++frees;
        free(p);
    }
}

static FILE *xopen(const char *path, const char *mode)
{
    FILE *fp = fopen(path, mode);
    return fp;
}

static int xclose(FILE *fp)
{
    int result = fclose(fp);
    ++closes;
    if (force_close_error) {
        errno = ENOSPC;
        return EOF;
    }
    return result;
}

static size_t xread(void *p, size_t size, size_t count, FILE *fp)
{
    if (force_read_error) {
        errno = EIO;
        return 0;
    }
    return fread(p, size, count, fp);
}

static int xerror(FILE *fp)
{
    return force_read_error || ferror(fp);
}

/* Exercise the shipped example itself. Only this test replaces its I/O
   boundary to produce failures which the OS cannot reliably reproduce. */
#define main resource_example_main
#define malloc xmalloc
#define free xfree
#define fopen xopen
#define fread xread
#define ferror xerror
#define fclose xclose
#include "../../examples/try_catch_resource.c"
#undef fclose
#undef ferror
#undef fread
#undef fopen
#undef free
#undef malloc
#undef main

static const struct tcc_exception_type E_FORMAT = {
    &TCC_Exception, "test.format"
};
static const struct tcc_exception_type E_EOF = {
    &TCC_Exception, "test.eof"
};

static void close_stream(FILE **pf)
{
    if (*pf != NULL) {
        xclose(*pf);
        *pf = NULL;
    }
}

static void test_reads(void)
{
    FILE *fp;
    unsigned char data[5000];
    unsigned char chunk[4];
    size_t n;
    int i;

    for (i = 0; i < (int)sizeof data; ++i)
        data[i] = (unsigned char)(i * 7);
    fp = fopen("159_try_catch_io.tmp", "wb");
    CHECK(fp != NULL);
    if (fp == NULL)
        return;
    CHECK(fwrite(data, 1, sizeof data, fp) == sizeof data);
    CHECK(fclose(fp) == 0);

    allocs = frees = closes = 0;
    CHECK(count_bytes("159_try_catch_io.tmp") == sizeof data);
    CHECK(allocs == 1 && frees == 1 && closes == 1);

    /* missing file: allocation succeeds, open fails */
    allocs = frees = closes = 0;
    try {
        count_bytes("159_try_catch_io.no-such-file");
        CHECK(0 && "missing file must throw");
    }
    catch (E_IO e) {
        struct io_failure detail;

        CHECK(e.type == &E_IO);
        CHECK(e.payload_size == sizeof detail);
        if (e.payload_size == sizeof detail) {
            memcpy(&detail, e.payload, sizeof detail);
            CHECK(strcmp(detail.operation, "open") == 0);
        }
    }
    CHECK(allocs == 1 && frees == 1 && closes == 0);

    /* short read at EOF distinguishes EOF from error */
    fp = fopen("159_try_catch_io.tmp", "rb");
    CHECK(fp != NULL);
    if (fp != NULL) {
        n = fread(chunk, 1, sizeof chunk, fp);
        CHECK(n == sizeof chunk);
        n = fread(chunk, 1, sizeof chunk, fp);
        CHECK(n == sizeof chunk);
        fseek(fp, 0, SEEK_END);
        n = fread(chunk, 1, sizeof chunk, fp);
        CHECK(n == 0 && feof(fp) && !ferror(fp));
        fclose(fp);
    }
    remove("159_try_catch_io.tmp");
}

static void test_fscanf(void)
{
    FILE *fp;
    int number;
    char word[16];
    int rc;

    fp = fopen("159_try_catch_io.scan1.tmp", "w+b");
    CHECK(fp != NULL);
    if (fp == NULL)
        return;
    fputs("42 word\n", fp);
    rewind(fp);
    rc = fscanf(fp, "%d %15s", &number, word);
    CHECK(rc == 2 && number == 42 && strcmp(word, "word") == 0);
    rc = fscanf(fp, "%d", &number);
    CHECK(rc == EOF && feof(fp));
    fclose(fp);
    remove("159_try_catch_io.scan1.tmp");

    fp = fopen("159_try_catch_io.scan2.tmp", "w+b");
    CHECK(fp != NULL);
    if (fp == NULL)
        return;
    fputs("not-a-number\n", fp);
    rewind(fp);
    rc = fscanf(fp, "%d", &number);
    CHECK(rc == 0 && !feof(fp));
    fclose(fp);
    remove("159_try_catch_io.scan2.tmp");
}

static void test_controlled_failures(void)
{
    const char *path = "159_try_catch_io.failure.tmp";
    FILE *fp = fopen(path, "wb");
    int i, scenario;

    CHECK(fp != NULL);
    if (fp == NULL)
        return;
    fputs("data", fp);
    fclose(fp);
    for (scenario = 0; scenario < 3; ++scenario) {
        for (i = 0; i < 20; ++i) {
            int caught = 0;
            allocs = frees = closes = 0;
            force_read_error = scenario != 0;
            force_close_error = scenario != 1;
            try {
                count_bytes(path);
                CHECK(0 && "controlled failure must throw");
            }
            catch (E_IO e) {
                struct io_failure detail;
                CHECK(e.payload != NULL && e.payload_size == sizeof detail);
                if (e.payload != NULL && e.payload_size == sizeof detail) {
                    memcpy(&detail, e.payload, sizeof detail);
                    CHECK(e.type == (force_read_error ? &E_IO_READ : &E_IO));
                    CHECK(strcmp(detail.operation,
                                 force_read_error ? "read" : "close") == 0);
                    CHECK(detail.error_number == (force_read_error ? EIO : ENOSPC));
                }
                caught = 1;
            }
            CHECK(caught && allocs == 1 && frees == 1 && closes == 1);
        }
    }
    force_read_error = force_close_error = 0;
    CHECK(add_count((size_t)-4, 3) == (size_t)-1);
    allocs = frees = closes = 0;
    try {
        FILE *owned_stream = NULL;
        void *owned_buffer = NULL;
        try {
            owned_buffer = xmalloc(4096);
            owned_stream = xopen(path, "rb");
            CHECK(owned_buffer != NULL && owned_stream != NULL);
            add_count((size_t)-4, 4);
            CHECK(0 && "count overflow must throw");
        }
        finally {
            close_stream(&owned_stream);
            xfree(owned_buffer);
        }
    }
    catch (E_OVERFLOW e) {
        CHECK(e.type == &E_OVERFLOW && e.payload == NULL && e.payload_size == 0);
    }
    CHECK(allocs == 1 && frees == 1 && closes == 1);
    remove(path);
}

static int scan_pair(FILE *fp)
{
    int a, b, assigned, scan_errno;

    errno = 0;
    assigned = fscanf(fp, "%d %d", &a, &b);
    scan_errno = errno;
    if (assigned != 2) {
        struct io_failure detail = { 0, "scan" };

        if (ferror(fp)) {
            detail.error_number = scan_errno;
            throw E_IO_READ, &detail, sizeof detail;
        }
        if (feof(fp))
            throw E_EOF;
        throw E_FORMAT;
    }
    return a + b;
}

static int read_pair(void)
{
    FILE *fp = NULL;
    int result = 0;

    try {
        fp = fopen("159_try_catch_io.called.tmp", "w+b");
        if (fp == NULL)
            throw E_IO;
        fputs("10 bad\n", fp);
        rewind(fp);
        result = scan_pair(fp);
    }
    finally {
        close_stream(&fp);
        remove("159_try_catch_io.called.tmp");
    }
    return result;
}

static void test_called_failure(void)
{
    int got = 0;

    closes = 0;
    try {
        read_pair();
        CHECK(0 && "scan failure must throw");
    }
    catch (E_FORMAT e) {
        got = e.type == &E_FORMAT && e.payload_size == 0;
    }
    CHECK(got);
    CHECK(closes == 1);
}

static void test_handles(void)
{
#ifdef _WIN32
    typedef BOOL (WINAPI *GET_HANDLE_COUNT)(HANDLE, PDWORD);
    HMODULE kernel;
    GET_HANDLE_COUNT get_count;
    DWORD before, after;
    int i;

    kernel = LoadLibraryA("kernel32.dll");
    if (kernel == NULL)
        return;
    get_count = (GET_HANDLE_COUNT)GetProcAddress(kernel, "GetProcessHandleCount");
    if (get_count == NULL) {
        FreeLibrary(kernel);
        return;
    }
    get_count(GetCurrentProcess(), &before);
    for (i = 0; i < 100; ++i) {
        try {
            count_bytes("159_try_catch_io.missing");
            ++fails;
        }
        catch (E_IO) {
        }
    }
    get_count(GetCurrentProcess(), &after);
    CHECK(after <= before);
    FreeLibrary(kernel);
#endif
}

int main(void)
{
    test_reads();
    test_fscanf();
    test_controlled_failures();
    test_called_failure();
    test_handles();
    printf("try/catch io failures=%d\n", fails);
    return fails != 0;
}
