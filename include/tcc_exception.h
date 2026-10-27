#ifndef TCC_EXCEPTION_H
#define TCC_EXCEPTION_H

#include <stddef.h>

#pragma pack(push, 16)

struct tcc_exception_type {
    const struct tcc_exception_type *parent;
    const char *name;
};

struct tcc_exception {
    const struct tcc_exception_type *type;
    const void *payload;
    size_t payload_size;
    const char *file;
    int line;
};

extern const struct tcc_exception_type TCC_Exception;
extern const struct tcc_exception_type TCC_OutOfMemory;
extern const struct tcc_exception_type TCC_InvalidException;

#pragma pack(pop)

#endif
