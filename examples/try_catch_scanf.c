/*
 * fscanf example: assignment counts, EOF, format mismatch and read errors.
 *
 * Build and run:
 *   tcc -ftry-catch -run examples/try_catch_scanf.c
 *
 * The example creates two small files, reads one record from each, and
 * closes every stream in finally.  It prints one line per case.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <tcc_exception.h>

static const struct tcc_exception_type E_SCAN = {
    &TCC_Exception, "example.scan"
};

struct scan_failure {
    int kind;
    int error_number;
    char operation[16];
};

enum {
    SCAN_EOF = 1,
    SCAN_MATCH = 2,
    SCAN_IO = 3
};

struct record {
    int number;
    char word[8];
};

/* Returns 1 on a complete record.  A partial read, EOF or an I/O failure
   throws one copied failure record after finally closes the stream. */
static int read_record(const char *path, struct record *record)
{
    FILE *fp = NULL;
    int assigned = 0;
    int scan_errno = 0;

    try {
        errno = 0;
        fp = fopen(path, "r");
        if (fp == NULL) {
            struct scan_failure detail = { SCAN_IO, 0, "open" };

            detail.error_number = errno;
            throw E_SCAN, &detail, sizeof detail;
        }

        errno = 0;
        assigned = fscanf(fp, "%d %7s", &record->number, record->word);
        scan_errno = errno;
        if (assigned != 2) {
            struct scan_failure detail = { SCAN_MATCH, 0, "fscanf" };

            detail.error_number = scan_errno;
            if (ferror(fp))
                detail.kind = SCAN_IO;
            else if (feof(fp))
                detail.kind = SCAN_EOF;
            throw E_SCAN, &detail, sizeof detail;
        }
    }
    finally {
        if (fp != NULL) {
            fclose(fp);
            fp = NULL;
        }
    }
    return assigned == 2;
}

