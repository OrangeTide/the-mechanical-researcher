/*
 * common.c -- Triton example common functions
 *
 * Utility functions for making Bare-metal Triton programs.
 *
 */

#include "common.h"

/* ---- Utility functions ------------------------------------------------- */

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
