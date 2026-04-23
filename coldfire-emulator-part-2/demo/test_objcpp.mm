/* test_objcpp.mm : bare-metal ColdFire V4e test — Objective-C++ */
/* Cross-compile: m68k-linux-gnu-g++ -mcpu=5475 -O2 -nostdlib -static
 *   -ffreestanding -fno-exceptions -fno-rtti -x objective-c++ -T link.ld */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

/* No ObjC/C++ runtime features — pure C subset compiled as Objective-C++. */

typedef unsigned int uint32_t;

volatile uint32_t result_fib    __attribute__((section(".results"))) = 0;
volatile uint32_t result_gcd    __attribute__((section(".results"))) = 0;
volatile uint32_t result_sum    __attribute__((section(".results"))) = 0;
volatile uint32_t result_bits   __attribute__((section(".results"))) = 0;
volatile uint32_t result_sqrt_i __attribute__((section(".results"))) = 0;

static uint32_t fibonacci(uint32_t n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

static uint32_t gcd(uint32_t a, uint32_t b)
{
    while (b != 0) {
        uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static uint32_t sum_to(uint32_t n)
{
    uint32_t s = 0;
    for (uint32_t i = 1; i <= n; i++)
        s += i;
    return s;
}

static uint32_t bit_test(uint32_t x)
{
    uint32_t a = x << 4;
    uint32_t b = x >> 2;
    uint32_t c = a ^ b;
    uint32_t d = c & 0xFF00;
    uint32_t e = d | 0x0055;
    return e;
}

static uint32_t sqrt_approx(double x)
{
    double guess = x / 2.0;
    for (int i = 0; i < 20; i++)
        guess = (guess + x / guess) / 2.0;
    return static_cast<uint32_t>(guess * 1000.0);
}

extern "C" void _start(void) __attribute__((section(".text.entry")));

extern "C" void _start(void)
{
    result_fib    = fibonacci(10);
    result_gcd    = gcd(252, 105);
    result_sum    = sum_to(100);
    result_bits   = bit_test(0xAB);
    result_sqrt_i = sqrt_approx(2.0);

    asm volatile("trap #0");
    for (;;)
        ;
}
