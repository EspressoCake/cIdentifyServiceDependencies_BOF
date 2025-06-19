#pragma once

#define COUNT_NEWLINES(str, count_var)            \
    do {                                          \
        const char* _p = (str);                   \
        (count_var) = 0;                          \
        if (_p) {                                 \
            while (*_p) {                         \
                if (*_p++ == '\n') ++(count_var); \
            }                                     \
        }                                         \
    } while (0)
