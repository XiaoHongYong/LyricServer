#pragma once

#ifdef DEBUG

#include <stdio.h>
#include <string.h>


#define LOG(...) {                                                          \
        const char *__name__ = strrchr(__FILE__, '/');                      \
        if (__name__ == NULL) { __name__ = __FILE__; } else { ++__name__; } \
        printf("%s:%s(%d): ", __name__, __func__, __LINE__);                \
        printf(__VA_ARGS__);                                                \
        printf("\n");                                                       \
    }

#else

#define LOG(...) ;

#endif
