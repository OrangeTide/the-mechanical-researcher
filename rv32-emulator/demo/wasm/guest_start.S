/* guest_start.S : entry point for the browser demo's guest program */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

    .section .text.entry, "ax", @progbits
    .globl _start
    .type _start, @function
_start:
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop
    la      sp, __stack_top

    /* Clear bss: the page loads a flat image, so only the bytes that are
     * present in the file have been written into guest memory. */
    la      t0, __bss_start
    la      t1, __bss_end
1:  bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b

2:  call    main
    li      a7, 93
    ecall
3:  j       3b
    .size _start, . - _start
