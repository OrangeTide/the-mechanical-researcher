/*
 * common.c -- Triton example common functions
 *
 * Utility functions for bare-metal Triton programs.
 * Provides UART output, memory functions, and framebuffer drawing.
 */

#include "common.h"

/* ---- UART output ------------------------------------------------------- */

void
putc_uart(char c)
{
    while (!(UART_TX_STATUS & 1))
        ;
    UART_TX_DATA = (unsigned char)c;
}

void
puts_uart(const char *s)
{
    while (*s)
        putc_uart(*s++);
}

void
put_int(int n)
{
    char buf[12];
    int i = 0;
    int neg = 0;

    if (n < 0) {
        neg = 1;
        n = -n;
    }
    do {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg)
        putc_uart('-');
    while (i > 0)
        putc_uart(buf[--i]);
}

void
put_hex(unsigned int n)
{
    static const char hex[] = "0123456789abcdef";
    int i;

    puts_uart("0x");
    for (i = 28; i >= 0; i -= 4)
        putc_uart(hex[(n >> i) & 0xf]);
}

/* ---- Memory functions -------------------------------------------------- */

void *
memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
    return dest;
}

void *
memset(void *s, int c, size_t n)
{
    unsigned char *p = s;

    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

int
memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = s1, *b = s2;

    while (n--) {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return 0;
}

size_t
strlen(const char *s)
{
    const char *p = s;

    while (*p)
        p++;
    return (size_t)(p - s);
}

/* ---- Framebuffer drawing ----------------------------------------------- */

void
fill_rect(int x, int y, int w, int h, unsigned short color)
{
    int row, col;

    for (row = y; row < y + h && row < SCREEN_H; row++) {
        for (col = x; col < x + w && col < SCREEN_W; col++) {
            VRAM_BASE[row * SCREEN_W + col] = color;
        }
    }
}
