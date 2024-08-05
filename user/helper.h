#include "kernel/types.h"
#include "user/user.h"

#define BUFSIZ 512
#define STDIN  0
#define STDOUT 1
#define STDERR 2
#define null (void *) 0
#define true  1
#define false 0

typedef short bool;

#define EXIT(cond, format, ...)                 \
    if (cond) {                                 \
        fprintf(2, format, ## __VA_ARGS__);     \
        exit(1);                                \
    }
 
#define assert(cond, format, ...)           \
    if (cond) {                             \
        printf(format, ## __VA_ARGS__);     \
        exit(1);                            \
    }
