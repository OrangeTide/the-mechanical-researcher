/*
 * hello.c -- Triton test program
 *
 * Bare-metal ColdFire program loaded by the monitor ROM.
 * Prints to UART and draws colored rectangles to the framebuffer.
 *
 * Cross-compile:
 *   m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
 *       -T hello_link.ld -o hello.elf hello.c
 */

/* ---- Hardware addresses ------------------------------------------------ */

#define UART_TX_DATA    (*(volatile unsigned char *)0x01150000)
#define UART_TX_STATUS  (*(volatile unsigned int  *)0x01150004)

/* VRAM starts at 0x00800000, framebuffer is 640x480 RGB565 */
#define VRAM_BASE       ((volatile unsigned short *)0x00800000)
#define SCREEN_W        640
#define SCREEN_H        480

/* RGB565 color values */
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define WHITE   0xFFFF
#define BLACK   0x0000

/* ---- Utility functions ------------------------------------------------- */

static void
putc_uart(char c)
{
    while (!(UART_TX_STATUS & 1))
        ;
    UART_TX_DATA = (unsigned char)c;
}

static void
puts_uart(const char *s)
{
    while (*s)
        putc_uart(*s++);
}

/* ---- Framebuffer drawing ----------------------------------------------- */

static void
fill_rect(int x, int y, int w, int h, unsigned short color)
{
    int row, col;

    for (row = y; row < y + h && row < SCREEN_H; row++) {
        for (col = x; col < x + w && col < SCREEN_W; col++) {
            VRAM_BASE[row * SCREEN_W + col] = color;
        }
    }
}

/* ---- Entry point ------------------------------------------------------- */

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    puts_uart("Hello from Triton!\r\n");

    /* Clear framebuffer to black */
    fill_rect(0, 0, SCREEN_W, SCREEN_H, BLACK);

    /* Draw three colored rectangles */
    fill_rect( 40,  40, 160, 120, RED);     /* top-left */
    fill_rect(240, 180, 160, 120, GREEN);   /* center */
    fill_rect(440, 320, 160, 120, BLUE);    /* bottom-right */

    /* Draw a white border around the screen edge */
    fill_rect(0, 0, SCREEN_W, 2, WHITE);               /* top */
    fill_rect(0, SCREEN_H - 2, SCREEN_W, 2, WHITE);    /* bottom */
    fill_rect(0, 0, 2, SCREEN_H, WHITE);               /* left */
    fill_rect(SCREEN_W - 2, 0, 2, SCREEN_H, WHITE);    /* right */

    puts_uart("VRAM test complete\r\n");

    /* Signal completion */
    __asm__ volatile("trap #0");

    /* Should not reach here */
    for (;;)
        ;
}
