#include <stdio.h>
#include <stddef.h>

#pragma pack(push, 1)

#include <tcc_exception.h>
struct after_header { char c; int i; };

#pragma pack(pop)

int main(void)
{
    printf("%d %d %d %d %d %d %d\n",
           (int)sizeof(struct tcc_exception_type),
           (int)sizeof(struct tcc_exception),
           (int)sizeof(struct __tcc_try_frame),
           (int)__alignof__(struct __tcc_try_frame),
           (int)offsetof(struct __tcc_try_frame, pending),
           (int)sizeof(struct after_header),
           (int)offsetof(struct after_header, i));
    return sizeof(struct after_header) != 5;
}
