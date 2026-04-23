/* shim_ada.c : C entry shim for bare-metal Ada test */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */

typedef unsigned int uint32_t;

/* Ada exports result symbols directly via pragma Export */
extern uint32_t result_fib;
extern uint32_t result_gcd;
extern uint32_t result_sum;
extern uint32_t result_bits;
extern uint32_t result_sqrt_i;

extern void _ada_test_ada(void);

void _start(void) __attribute__((section(".text.entry")));

void
_start(void)
{
    _ada_test_ada();

    __asm__ volatile("trap #0");
    for (;;)
        ;
}
