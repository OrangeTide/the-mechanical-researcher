/* test_d.d : bare-metal ColdFire V4e test — D (via GDC) */
/* Cross-compile: m68k-linux-gnu-gdc-9 -mcpu=5475 -O2 -fno-druntime
 *   -nostdlib -static -I. -T link.ld */
/* Requires object.d stub in include path for GDC 9. */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

extern(C):

__gshared uint result_fib = 0;
__gshared uint result_gcd = 0;
__gshared uint result_sum = 0;
__gshared uint result_bits = 0;
__gshared uint result_sqrt_i = 0;

uint fibonacci(uint n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

uint gcd_func(uint a, uint b)
{
    while (b != 0) {
        uint t = a % b;
        a = b;
        b = t;
    }
    return a;
}

uint sum_to(uint n)
{
    uint s = 0;
    uint i;
    for (i = 1; i <= n; i++)
        s += i;
    return s;
}

uint bit_test(uint x)
{
    uint a = x << 4, b = x >> 2;
    return ((a ^ b) & 0xFF00) | 0x0055;
}

/* Integer-only Newton's method: sqrt(xi) * 1000.
 * No FPU — D with -fno-druntime cannot do float-to-int conversion
 * without the D runtime's _d_arraybounds and TypeInfo support. */
uint sqrt_approx_int(uint xi)
{
    if (xi == 0)
        return 0;
    uint x1000 = xi * 1000;
    uint guess = x1000 / 2;
    int j;
    for (j = 0; j < 20; j++) {
        if (guess == 0)
            break;
        guess = (guess + x1000 * 1000 / guess) / 2;
    }
    return guess;
}

void _start()
{
    result_fib    = fibonacci(10);
    result_gcd    = gcd_func(252, 105);
    result_sum    = sum_to(100);
    result_bits   = bit_test(0xAB);
    result_sqrt_i = sqrt_approx_int(2);

    asm { "trap #0"; }
    while (true) {}
}
