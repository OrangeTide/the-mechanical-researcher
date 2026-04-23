/* shim_fortran.c : C entry shim for bare-metal Fortran test */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */

typedef unsigned int uint32_t;
typedef int int32_t;

volatile uint32_t result_fib    __attribute__((section(".results"))) = 0;
volatile uint32_t result_gcd    __attribute__((section(".results"))) = 0;
volatile uint32_t result_sum    __attribute__((section(".results"))) = 0;
volatile uint32_t result_bits   __attribute__((section(".results"))) = 0;
volatile uint32_t result_sqrt_i __attribute__((section(".results"))) = 0;

extern void compute(int32_t *fib, int32_t *gcd_r, int32_t *sum,
                    int32_t *bits, int32_t *sqrt_r);

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    int32_t fib, gcd_r, sum, bits, sqrt_r;

    compute(&fib, &gcd_r, &sum, &bits, &sqrt_r);
    result_fib    = (uint32_t)fib;
    result_gcd    = (uint32_t)gcd_r;
    result_sum    = (uint32_t)sum;
    result_bits   = (uint32_t)bits;
    result_sqrt_i = (uint32_t)sqrt_r;

    __asm__ volatile("trap #0");
    for (;;)
        ;
}
