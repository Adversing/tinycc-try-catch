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

#if defined(__TINYC_TRY_CATCH__) && __TINYC_TRY_CATCH__
#include <setjmp.h>

/* Private ABI shared by the compiler and the runtime.  These names are
   reserved for the implementation.  The compiler only needs the frame
   address and the first member (the checkpoint environment). */
struct __tcc_try_exception;

struct __tcc_try_frame {
    jmp_buf env;
    struct __tcc_try_frame *prev;
    struct __tcc_try_exception *pending;
    struct __tcc_try_exception *caught;
    void (*jump)(jmp_buf, int);
    int phase;
    int action;
    int exit_id;
    int abi_size;
};

/* Reserved typedefs keep the compiler's ABI types independent of ordinary
   local struct tags with the same public spelling. */
typedef struct tcc_exception_type __tcc_exception_type;
typedef struct tcc_exception __tcc_exception_record;
typedef struct __tcc_try_frame __tcc_try_frame_type;

#define __TCC_TRY_PHASE_TRY     0
#define __TCC_TRY_PHASE_CATCH   1
#define __TCC_TRY_PHASE_FINALLY 2

#define __TCC_TRY_ACTION_NORMAL    0
#define __TCC_TRY_ACTION_EXCEPTION 1
#define __TCC_TRY_ACTION_JUMP      2

void __tcc_try_enter(struct __tcc_try_frame *frame,
                     void (*jump)(jmp_buf, int),
                     unsigned long frame_size,
                     unsigned long frame_align,
                     unsigned long env_offset);
int  __tcc_try_landing(struct __tcc_try_frame *frame);
int  __tcc_try_match(struct __tcc_try_frame *frame,
                     const struct tcc_exception_type *type);
int  __tcc_try_match_any(struct __tcc_try_frame *frame);
void __tcc_try_bind(struct __tcc_try_frame *frame, struct tcc_exception *out);
void __tcc_try_normal(struct __tcc_try_frame *frame);
void __tcc_try_begin_finally(struct __tcc_try_frame *frame, int action, int id);
int  __tcc_try_finish(struct __tcc_try_frame *frame);
void __tcc_try_pop(struct __tcc_try_frame *frame);
void __tcc_try_rethrow(struct __tcc_try_frame *frame);
void __tcc_throw_new(const struct tcc_exception_type *type,
                     const void *payload,
                     unsigned long long size,
                     int size_is_signed,
                     const char *file,
                     int line);
void __tcc_try_shutdown(void);
#endif

#pragma pack(pop)

#endif
