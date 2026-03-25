/* test_program.c : bare-metal ColdFire V4e test program */
/* Cross-compile: m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -T link.ld */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

/* Results are stored at known addresses for the test harness to read.
 * TRAP #0 signals completion — the emulator halts on this vector. */

typedef unsigned int uint32_t;
typedef int int32_t;

/* Result slots — placed in .data so the linker gives them fixed addresses */
volatile uint32_t result_fib    __attribute__((section(".results"))) = 0;
volatile uint32_t result_gcd    __attribute__((section(".results"))) = 0;
volatile uint32_t result_sum    __attribute__((section(".results"))) = 0;
volatile uint32_t result_bits   __attribute__((section(".results"))) = 0;
volatile uint32_t result_sqrt_i __attribute__((section(".results"))) = 0;

/****************************************************************
 * Integer tests
 ****************************************************************/

/* Fibonacci — tests recursion, stack, branching, comparison */
static uint32_t
fibonacci(uint32_t n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

/* GCD — tests REMU.L, loops, conditional branch */
static uint32_t
gcd(uint32_t a, uint32_t b)
{
    while (b != 0) {
        uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* Sum 1..n — tests loop, ADDQ, comparison */
static uint32_t
sum_to(uint32_t n)
{
    uint32_t s = 0;
    uint32_t i;

    for (i = 1; i <= n; i++)
        s += i;
    return s;
}

/* Bit manipulation — tests shift, AND, OR, XOR */
static uint32_t
bit_test(uint32_t x)
{
    uint32_t a = x << 4;       /* ASL/LSL */
    uint32_t b = x >> 2;       /* LSR */
    uint32_t c = a ^ b;        /* EOR */
    uint32_t d = c & 0xFF00;   /* AND */
    uint32_t e = d | 0x0055;   /* OR */
    return e;
}

/****************************************************************
 * FPU test
 ****************************************************************/

/* Integer square root via Newton's method using FPU.
 * Returns sqrt(x) * 1000 as integer for easy comparison. */
static uint32_t
sqrt_approx(double x)
{
    double guess = x / 2.0;
    int i;

    for (i = 0; i < 20; i++)
        guess = (guess + x / guess) / 2.0;

    /* Return as fixed-point integer (× 1000) */
    return (uint32_t)(guess * 1000.0);
}

/****************************************************************
 * Entry point
 ****************************************************************/

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    result_fib    = fibonacci(10);       /* expect 55 */
    result_gcd    = gcd(252, 105);       /* expect 21 */
    result_sum    = sum_to(100);         /* expect 5050 */
    result_bits   = bit_test(0xAB);      /* expect 0x0A55 */
    result_sqrt_i = sqrt_approx(2.0);    /* expect 1414 */

    /* Signal completion */
    __asm__ volatile("trap #0");

    /* Should not reach here */
    for (;;)
        ;
}
