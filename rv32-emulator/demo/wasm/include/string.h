/* string.h : the three declarations the emulator needs, for freestanding
 * WebAssembly builds where no C library is present */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

#ifndef RV32_FREESTANDING_STRING_H
#define RV32_FREESTANDING_STRING_H

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);

#endif /* RV32_FREESTANDING_STRING_H */
