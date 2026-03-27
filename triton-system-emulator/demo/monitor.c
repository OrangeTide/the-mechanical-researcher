/*
 * monitor.c -- Triton monitor ROM
 *
 * Bare-metal ColdFire V4e program that runs from NOR flash (0x01200000).
 * Provides hardware init, an ELF loader, and default exception handlers.
 *
 * Cross-compile:
 *   m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
 *       -T monitor_link.ld -o monitor.elf monitor.c
 */

/* ---- Hardware addresses ------------------------------------------------ */

#define UART_TX_DATA    (*(volatile unsigned char *)0x01150000)
#define UART_TX_STATUS  (*(volatile unsigned int  *)0x01150004)

#define GPU_STATUS      (*(volatile unsigned int  *)0x01000000)
#define GPU_VID_PROC    (*(volatile unsigned int  *)0x01000004)
#define GPU_FB_BASE     (*(volatile unsigned int  *)0x01000008)

#define AUDIO_GLOBAL    (*(volatile unsigned int  *)0x01100000)

#define ELF_STAGE_ADDR  ((unsigned char *)0x00001000)

/* ---- ELF constants ----------------------------------------------------- */

#define EI_MAG0     0
#define EI_CLASS    4
#define EI_DATA     5
#define ELFCLASS32  1
#define ELFDATA2MSB 2
#define ET_EXEC     2
#define EM_68K      4
#define PT_LOAD     1

/* ---- Utility functions ------------------------------------------------- */

static void
mon_putc(char c)
{
    while (!(UART_TX_STATUS & 1))
        ;
    UART_TX_DATA = (unsigned char)c;
}

static void
mon_puts(const char *s)
{
    while (*s)
        mon_putc(*s++);
}

static void
mon_puthex(unsigned int val)
{
    static const char hex[] = "0123456789ABCDEF";
    int i;

    mon_puts("0x");
    for (i = 28; i >= 0; i -= 4)
        mon_putc(hex[(val >> i) & 0xF]);
}

static void
mon_putdec(unsigned int val)
{
    char buf[12];
    int i = 0;

    if (val == 0) {
        mon_putc('0');
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (--i >= 0)
        mon_putc(buf[i]);
}

static void *
mon_memcpy(void *dst, const void *src, unsigned int n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    unsigned int i;

    for (i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

static void
mon_memset(void *dst, int val, unsigned int n)
{
    unsigned char *d = dst;
    unsigned int i;

    for (i = 0; i < n; i++)
        d[i] = (unsigned char)val;
}

/* ---- ELF header field access ------------------------------------------- */

/*
 * On ColdFire (big-endian), ELF fields for EM_68K are also big-endian.
 * Direct pointer casts give correct values without byte-swapping.
 */

static unsigned int
elf_u32(const unsigned char *p)
{
    return *(const unsigned int *)p;
}

static unsigned short
elf_u16(const unsigned char *p)
{
    return *(const unsigned short *)p;
}

/* ---- ELF loader -------------------------------------------------------- */

static unsigned int
mon_load_elf(void)
{
    const unsigned char *elf = ELF_STAGE_ADDR;
    unsigned int e_entry, e_phoff, e_phnum, e_phentsize;
    unsigned int i;

    /* Validate ELF magic */
    if (elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F') {
        mon_puts("No ELF found at ");
        mon_puthex((unsigned int)elf);
        mon_puts("\r\n");
        return 0;
    }

    /* Validate class, endianness, machine */
    if (elf[EI_CLASS] != ELFCLASS32) {
        mon_puts("Error: not ELF32\r\n");
        return 0;
    }
    if (elf[EI_DATA] != ELFDATA2MSB) {
        mon_puts("Error: not big-endian\r\n");
        return 0;
    }

    /* e_type at offset 16, e_machine at offset 18 */
    if (elf_u16(elf + 16) != ET_EXEC) {
        mon_puts("Error: not ET_EXEC\r\n");
        return 0;
    }
    if (elf_u16(elf + 18) != EM_68K) {
        mon_puts("Error: not EM_68K\r\n");
        return 0;
    }

    /* Extract header fields */
    e_entry     = elf_u32(elf + 24);
    e_phoff     = elf_u32(elf + 28);
    e_phentsize = elf_u16(elf + 42);
    e_phnum     = elf_u16(elf + 44);

    mon_puts("ELF: entry=");
    mon_puthex(e_entry);
    mon_puts(" phnum=");
    mon_putdec(e_phnum);
    mon_puts("\r\n");

    /* Walk program headers and load PT_LOAD segments */
    for (i = 0; i < e_phnum; i++) {
        const unsigned char *ph = elf + e_phoff + i * e_phentsize;
        unsigned int p_type   = elf_u32(ph + 0);
        unsigned int p_offset = elf_u32(ph + 4);
        unsigned int p_vaddr  = elf_u32(ph + 8);
        unsigned int p_filesz = elf_u32(ph + 16);
        unsigned int p_memsz  = elf_u32(ph + 20);

        if (p_type != PT_LOAD)
            continue;

        mon_puts("  LOAD: vaddr=");
        mon_puthex(p_vaddr);
        mon_puts(" filesz=");
        mon_puthex(p_filesz);
        mon_puts(" memsz=");
        mon_puthex(p_memsz);
        mon_puts("\r\n");

        /* Copy file data */
        if (p_filesz > 0)
            mon_memcpy((void *)p_vaddr, elf + p_offset, p_filesz);

        /* Zero-fill BSS */
        if (p_memsz > p_filesz)
            mon_memset((void *)(p_vaddr + p_filesz), 0,
                       p_memsz - p_filesz);
    }

    return e_entry;
}

/* ---- Exception handler ------------------------------------------------- */

/*
 * ColdFire exception stack frame (format 4):
 *   [SP+0] format/vector word (16-bit)
 *   [SP+2] status register (16-bit)
 *   [SP+4] program counter (32-bit)
 *
 * We receive a pointer to the stacked frame via inline assembly.
 */
void _default_handler(void) __attribute__((interrupt));

void
_default_handler(void)
{
    /*
     * Read the exception frame. On ColdFire, the frame is at the
     * current stack pointer on entry to the handler.
     */
    unsigned int *sp;
    unsigned int pc;
    unsigned short fmtvec, sr;

    __asm__ volatile("move.l %%sp, %0" : "=r"(sp));

    fmtvec = *(unsigned short *)sp;
    sr     = *((unsigned short *)sp + 1);
    pc     = *(unsigned int *)((unsigned char *)sp + 4);

    mon_puts("\r\n*** EXCEPTION ***\r\n");
    mon_puts("Vector: ");
    mon_puthex((fmtvec >> 2) & 0xFF);
    mon_puts("  SR: ");
    mon_puthex(sr);
    mon_puts("  PC: ");
    mon_puthex(pc);
    mon_puts("\r\n");

    /* Halt */
    __asm__ volatile("halt");
    for (;;)
        ;
}

/* ---- TRAP #0 handler: halt --------------------------------------------- */

void _trap0_handler(void) __attribute__((interrupt));

void
_trap0_handler(void)
{
    mon_puts("\r\nTRAP #0: halting\r\n");
    __asm__ volatile("halt");
    for (;;)
        ;
}

/* ---- Monitor entry point ----------------------------------------------- */

void _monitor_start(void) __attribute__((section(".text.entry")));

void
_monitor_start(void)
{
    unsigned int entry;

    /* Hardware init */
    GPU_VID_PROC = 0x00000001;  /* enable display */
    GPU_FB_BASE  = 0x00000000;  /* framebuffer at start of VRAM */
    AUDIO_GLOBAL = 0x00000000;  /* silence */

    /* Banner */
    mon_puts("\r\n");
    mon_puts("Triton Monitor v0.1\r\n");
    mon_puts("Vertex Technologies, 2001\r\n");
    mon_puts("\r\n");

    /* Load ELF from staging area */
    entry = mon_load_elf();
    if (entry == 0) {
        mon_puts("No program to run. Halting.\r\n");
        __asm__ volatile("halt");
        for (;;)
            ;
    }

    /* Jump to loaded program */
    mon_puts("Jumping to ");
    mon_puthex(entry);
    mon_puts("\r\n\r\n");

    ((void (*)(void))entry)();

    /* Should not return */
    mon_puts("\r\nProgram returned. Halting.\r\n");
    __asm__ volatile("halt");
    for (;;)
        ;
}

/* ---- Vector table ------------------------------------------------------ */

/*
 * ColdFire vector table: 256 entries of 4 bytes each (1 KB).
 * Placed at the start of NOR flash (0x01200000) via linker script.
 */

extern void _monitor_start(void);
extern void _default_handler(void);
extern void _trap0_handler(void);

typedef void (*vector_fn)(void);

__attribute__((section(".text.vectors")))
const unsigned int vector_table[256] = {
    /* 0: Initial SSP (top of 8 MB RAM) */
    0x00800000,
    /* 1: Initial PC */
    (unsigned int)_monitor_start,
    /* 2-31: Exception handlers */
    (unsigned int)_default_handler,     /*  2: access error */
    (unsigned int)_default_handler,     /*  3: address error */
    (unsigned int)_default_handler,     /*  4: illegal instruction */
    (unsigned int)_default_handler,     /*  5: zero divide */
    (unsigned int)_default_handler,     /*  6: reserved */
    (unsigned int)_default_handler,     /*  7: reserved */
    (unsigned int)_default_handler,     /*  8: privilege violation */
    (unsigned int)_default_handler,     /*  9: trace */
    (unsigned int)_default_handler,     /* 10: LINE_A */
    (unsigned int)_default_handler,     /* 11: LINE_F */
    (unsigned int)_default_handler,     /* 12: debug interrupt */
    (unsigned int)_default_handler,     /* 13: reserved */
    (unsigned int)_default_handler,     /* 14: format error */
    (unsigned int)_default_handler,     /* 15: uninitialized interrupt */
    (unsigned int)_default_handler,     /* 16: reserved */
    (unsigned int)_default_handler,     /* 17: reserved */
    (unsigned int)_default_handler,     /* 18: reserved */
    (unsigned int)_default_handler,     /* 19: reserved */
    (unsigned int)_default_handler,     /* 20: reserved */
    (unsigned int)_default_handler,     /* 21: reserved */
    (unsigned int)_default_handler,     /* 22: reserved */
    (unsigned int)_default_handler,     /* 23: reserved */
    (unsigned int)_default_handler,     /* 24: spurious interrupt */
    (unsigned int)_default_handler,     /* 25: autovector L1 */
    (unsigned int)_default_handler,     /* 26: autovector L2 */
    (unsigned int)_default_handler,     /* 27: autovector L3 */
    (unsigned int)_default_handler,     /* 28: autovector L4 */
    (unsigned int)_default_handler,     /* 29: autovector L5 */
    (unsigned int)_default_handler,     /* 30: autovector L6 */
    (unsigned int)_default_handler,     /* 31: autovector L7 (NMI) */
    /* 32-47: TRAP #0-#15 */
    (unsigned int)_trap0_handler,       /* 32: TRAP #0 = halt */
    [33 ... 47] = (unsigned int)_default_handler,
    /* 48-255: user interrupts */
    [48 ... 255] = (unsigned int)_default_handler,
};
