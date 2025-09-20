#include "sish.h"

char *my_strncat(char *src, size_t start, size_t end)
{
    size_t dest_len = 0;
    size_t j = 0;

    if (start >= end)
        return  NULL;

    if (!src)
        return NULL;

    if (end > strlen(src))
        end = strlen(src);

    char *dest = malloc(end - start + 1);
    if (!dest)
        return NULL;

    for (size_t i = start; i < end && src[i] != '\0'; i++) {
        dest[dest_len + j] = src[i];
        j++;
    }
    dest[dest_len + j] = '\0';
    return dest;
}

