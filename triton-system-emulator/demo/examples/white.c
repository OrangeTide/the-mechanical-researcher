/*
 * white.c -- Triton example program - draw some white boxes
 *
 * Triton programs to demonstrate simple framebuffer art.
 *
 */
#include "common.h"

/* RGB565 color values */
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define WHITE   0xFFFF
#define BLACK   0x0000

/* ---- Entry point ------------------------------------------------------- */

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    puts_uart("Triton demo 1:\r\n");

    /* Clear framebuffer to white */
    fill_rect(0, 0, SCREEN_W, SCREEN_H, WHITE);

    // TODO: draw some random rectangles
    fill_rect( 40,  40, 160, 120, BLACK);     /* top-left */
    fill_rect(240, 180, 160, 120, BLACK);   /* center */
    fill_rect(440, 320, 160, 120, BLACK);    /* bottom-right */

    /* Draw a red border around the screen edge */
    fill_rect(0, 0, SCREEN_W, 2, RED);              /* top */
    fill_rect(0, SCREEN_H - 2, SCREEN_W, 2, RED);   /* bottom */
    fill_rect(0, 0, 2, SCREEN_H, RED);              /* left */
    fill_rect(SCREEN_W - 2, 0, 2, SCREEN_H, RED);   /* right */

    puts_uart("DEMO COMPLETE\r\n");

    /* Signal completion */
    __asm__ volatile("trap #0");

    /* Should not reach here */
    for (;;)
        ;
}
