#include "string.h"

int strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

int strlen(const char *s)
{
    int n = 0;

    while (s[n]) {
        n++;
    }

    return n;
}