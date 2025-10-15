// floason (C) 2025
// Licensed under the MIT License.

#pragma once

#include <stdlib.h>
#include <ctype.h>

inline void* quick_calloc(size_t count, size_t size)
{
    void* ptr = calloc(count, size);
    if (!ptr)
        abort();
    return ptr;
}

inline void* quick_malloc(size_t size)
{
    return quick_calloc(1, size);
}

inline int util_strncasecmp(const char* lhs, const char* rhs, size_t length)
{
    int lhs_c, rhs_c;
    int count = 0;
    do
    {
        lhs_c = toupper(*lhs++);
        rhs_c = toupper(*rhs++);
        count += 1;
    } while (lhs_c == rhs_c && lhs_c != '\0' && count < (length));
    return lhs_c - rhs_c;
}