#ifndef COMMON_H_
#define COMMON_H_
/* ---- Hardware addresses ------------------------------------------------ */

#define UART_TX_DATA    (*(volatile unsigned char *)0x01150000)
#define UART_TX_STATUS  (*(volatile unsigned int  *)0x01150004)

/* VRAM starts at 0x00800000, framebuffer is 640x480 RGB565 */
#define VRAM_BASE       ((volatile unsigned short *)0x00800000)
#define SCREEN_W        640
#define SCREEN_H        480

void putc_uart(char c);
void puts_uart(const char *s);
void fill_rect(int x, int y, int w, int h, unsigned short color);
#endif
