/*
 * Complete try/catch/finally resource-management example.
 *
 * Build and run:
 *   tcc -ftry-catch -run examples/try_catch_resource.c file.txt
 *
 * The stream and the read buffer are released on normal completion and on
 * every exception path.  The caller receives either the byte count or a
 * copied application error record.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcc_exception.h>

static const struct tcc_exception_type E_IO = {
    &TCC_Exception, "example.io"
};
static const struct tcc_exception_type E_IO_READ = {
    &E_IO, "example.io.read"
};
static const struct tcc_exception_type E_OVERFLOW = {
    &TCC_Exception, "example.count_overflow"
};

struct io_failure {
    int error_number;
    char operation[16];
};

static size_t add_count(size_t count, size_t n)
{
    if (n > (size_t)-1 - count)
        throw E_OVERFLOW;
    return count + n;
}

static size_t count_bytes(const char *path)
{
    FILE *fp = NULL;
    unsigned char *buffer = NULL;
    size_t count = 0;
    size_t n;
    int read_errno;
    int close_status = 0;
    int close_errno = 0;

    try {
        struct io_failure detail = { 0, "open" };

        buffer = malloc(4096);
        if (buffer == NULL)
            throw TCC_OutOfMemory;
        errno = 0;
        fp = fopen(path, "rb");
        if (fp == NULL) {
            detail.error_number = errno;
            throw E_IO, &detail, sizeof detail;
        }

        for (;;) {
            errno = 0;
            n = fread(buffer, 1, 4096, fp);
            read_errno = errno;
            count = add_count(count, n);
            if (n < 4096) {
                if (ferror(fp)) {
                    struct io_failure read_error = { 0, "read" };

                    read_error.error_number = read_errno;
                    throw E_IO_READ, &read_error, sizeof read_error;
                }
                break;
            }
        }
    }
    finally {
        if (fp != NULL) {
            errno = 0;
            close_status = fclose(fp);
            close_errno = errno;
            fp = NULL;
        }
        free(buffer);
        buffer = NULL;
    }

    if (close_status == EOF) {
        struct io_failure detail = { 0, "close" };

        detail.error_number = close_errno;
        throw E_IO, &detail, sizeof detail;
    }
    return count;
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;

    try {
        printf("%llu\n", (unsigned long long)count_bytes(argv[1]));
    }
    catch (E_IO e) {
        struct io_failure detail;

        fprintf(stderr, "%s at %s:%d\n", e.type->name, e.file, e.line);
        if ((e.type == &E_IO || e.type == &E_IO_READ)
            && e.payload != NULL && e.payload_size == sizeof detail) {
            memcpy(&detail, e.payload, sizeof detail);
            fprintf(stderr, "%.*s failed (errno=%d)\n",
                    (int)sizeof detail.operation, detail.operation,
                    detail.error_number);
        }
        return 1;
    }
    catch (TCC_Exception e) {
        fprintf(stderr, "%s\n", e.type->name);
        return 1;
    }
    return 0;
}
