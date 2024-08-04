#include "kernel/types.h"
#include "user/user.h"

#define BUFSIZ 512

#define EXIT(exp, info, ...)            \
    if (exp) {                          \
        fprintf(2, info, ## __VA_ARGS__);  \
        exit(1);                        \
    }
 
#define assert(exp, info, ...)          \
    if (exp) {                          \
        printf(info, ## __VA_ARGS__);      \
        exit(1);                        \
    }

