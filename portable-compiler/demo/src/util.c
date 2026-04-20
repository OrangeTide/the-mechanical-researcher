/* util.c - Diagnostics and allocation helpers. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinc.h"

void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fputs("tinc: error: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

void
warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fputs("tinc: warning: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void *
xmalloc(size_t n)
{
    void *p;

    p = malloc(n);
    if (p == NULL)
        die("out of memory");
    return p;
}

void *
xcalloc(size_t n, size_t sz)
{
    void *p;

    p = calloc(n, sz);
    if (p == NULL)
        die("out of memory");
    return p;
}

char *
xstrdup(const char *s)
{
    size_t n;
    char *p;

    n = strlen(s);
    p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

char *
xstrndup(const char *s, size_t n)
{
    char *p;

    p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
