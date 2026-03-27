#ifndef COMMON_H_
#define COMMON_H_
/* ---- Types ------------------------------------------------------------- */

typedef unsigned long size_t;

/* ---- Hardware addresses ------------------------------------------------ */

#define UART_TX_DATA    (*(volatile unsigned char *)0x01150000)
#define UART_TX_STATUS  (*(volatile unsigned int  *)0x01150004)

/* VRAM starts at 0x00800000, framebuffer is 640x480 RGB565 */
#define VRAM_BASE       ((volatile unsigned short *)0x00800000)
#define SCREEN_W        640
#define SCREEN_H        480

/* Audio registers at 0x01100000 */
#define AUDIO_BASE      ((volatile unsigned char *)0x01100000)

/* ---- UART output ------------------------------------------------------- */

void putc_uart(char c);
void puts_uart(const char *s);
void put_int(int n);
void put_hex(unsigned int n);

/* ---- Memory functions -------------------------------------------------- */

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);

/* ---- Framebuffer drawing ----------------------------------------------- */

void fill_rect(int x, int y, int w, int h, unsigned short color);
#endif
