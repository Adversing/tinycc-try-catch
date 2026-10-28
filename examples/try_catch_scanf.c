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

static void write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");

    if (fp != NULL) {
        fputs(text, fp);
        fclose(fp);
    }
}

static void report(const char *label, const char *path)
{
    struct record record;

    try {
        if (read_record(path, &record))
            printf("%s: assigned 2, number=%d, word=%s\n",
                   label, record.number, record.word);
    }
    catch (E_SCAN e) {
        struct scan_failure detail;

        if (e.payload != NULL && e.payload_size == sizeof detail) {
            memcpy(&detail, e.payload, sizeof detail);
            if (detail.kind == SCAN_EOF)
                printf("%s: EOF\n", label);
            else if (detail.kind == SCAN_MATCH)
                printf("%s: format mismatch\n", label);
            else
                printf("%s: I/O failure in %.*s, errno=%d\n", label,
                       (int)sizeof detail.operation, detail.operation,
                       detail.error_number);
        }
    }
    catch (TCC_Exception e) {
        printf("%s: unexpected %s\n", label, e.type->name);
    }
}

int main(void)
{
    const char *good = "examples_try_catch_scan_good.tmp";
    const char *bad = "examples_try_catch_scan_bad.tmp";
    const char *missing = "examples_try_catch_scan_missing.tmp";
    const char *empty = "examples_try_catch_scan_empty.tmp";
    const char *partial = "examples_try_catch_scan_partial.tmp";

    write_file(good, "42 word\n");
    write_file(bad, "bad word\n");
    write_file(empty, "");
    write_file(partial, "42\n");

    report("good", good);
    report("bad", bad);
    report("empty", empty);
    report("partial", partial);
    report("missing", missing);

    remove(good);
    remove(bad);
    remove(empty);
    remove(partial);
    return 0;
}
