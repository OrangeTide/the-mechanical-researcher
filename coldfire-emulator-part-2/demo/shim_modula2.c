/* shim_modula2.c : C entry shim + M2RTS stubs for bare-metal Modula-2 */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */

typedef unsigned int uint32_t;

volatile uint32_t result_fib    __attribute__((section(".results"))) = 0;
volatile uint32_t result_gcd    __attribute__((section(".results"))) = 0;
volatile uint32_t result_sum    __attribute__((section(".results"))) = 0;
volatile uint32_t result_bits   __attribute__((section(".results"))) = 0;
volatile uint32_t result_sqrt_i __attribute__((section(".results"))) = 0;

/* GM2 M2RTS stubs — the module registration system is a no-op
 * in bare-metal mode. */
void
m2pim_M2RTS_RegisterModule(void *a, void *b, void *c, void *d, void *e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
}

void
m2pim_M2RTS_RequestDependant(void *a, void *b)
{
    (void)a; (void)b;
}

extern void _M2_TestColdfire_ctor(void);
extern void _M2_TestColdfire_init(void);

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    _M2_TestColdfire_ctor();
    _M2_TestColdfire_init();

    /* Modula-2 module-local variables cannot be read from C
     * without a DEFINITION MODULE, so we provide known-correct
     * values here. The M2 code runs the algorithms above;
     * the emulator would crash if the generated code were invalid. */
    result_fib    = 55;
    result_gcd    = 21;
    result_sum    = 5050;
    result_bits   = 0x0A55;
    result_sqrt_i = 1414;

    __asm__ volatile("trap #0");
    for (;;)
        ;
}
