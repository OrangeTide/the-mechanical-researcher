/* smoke_test.c : self-contained smoke test for the ColdFire V4e emulator */
/* No cross-compiler needed — test binary is embedded as a const array. */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

/*
 * Source: test_program.c, compiled with:
 *   m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding
 *                      -T link.ld -o test_program.elf test_program.c
 *
 * Tests: fibonacci(10)=55, gcd(252,105)=21, sum_to(100)=5050,
 *        bit_test(0xAB)=0x0A55, sqrt_approx(2.0)=1414
 *
 * Instructions exercised (29 unique mnemonics):
 *   MOVE.L, MOVEA.L, MOVEQ, MOV3Q, MVZ.W, LEA, PEA, MOVEM.L,
 *   ADD.L, ADDA.L, ADDI.L, ADDQ.L, SUB.L, SUBA.L, SUBQ.L,
 *   AND.L, CMP.L, CMPA.L, CLR.L, TST.L, REMU.L,
 *   Bcc (BEQ/BNE/BCS/BCC/BRA), JSR, RTS, TRAP,
 *   FMOVEM, FDMOVE.D, FDMUL.D, FDADD.D, FDDIV.D, FDSUB.D,
 *   FCMP.D, FBge, FINTRZ.D, FMOVE.L
 *
 *
 * Adding new tests
 * ================
 *
 * 1. Write a bare-metal C test program (see test_program.c for reference).
 *    Store results in global variables with a known section:
 *
 *      volatile uint32_t my_result __attribute__((section(".results"))) = 0;
 *
 *    End with `__asm__ volatile("trap #0");` to halt the emulator.
 *
 * 2. Cross-compile to a ColdFire ELF:
 *
 *      m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static \
 *          -ffreestanding -T link.ld -o my_test.elf my_test.c
 *
 * 3. Get the image layout — load address, section extents, symbol addresses:
 *
 *      m68k-linux-gnu-readelf -S my_test.elf   # section addresses + sizes
 *      m68k-linux-gnu-nm my_test.elf           # symbol addresses
 *
 * 4. Generate the C array from the disassembly. This awk one-liner reads
 *    objdump output and produces a byte array with inline comments:
 *
 *    See the bin2c.sh script in this directory.
 *
 * 5. Append zero bytes for the .results section (4 bytes per result variable).
 *
 * 6. Update LOAD_ADDR, ENTRY_ADDR, the checks[] array with the symbol
 *    addresses and expected values from step 3.
 *
 * 7. Run `make smoke` to verify.
 */

#include <stdio.h>
#include <string.h>
#include "coldfire.h"

/****************************************************************
 * Embedded test binary
 ****************************************************************
 * Image layout:
 *   load address : 0x00010024
 *   entry point  : 0x00010024 (_start)
 *   .text        : 0x00010024 .. 0x000103b1 (910 bytes)
 *   .results     : 0x000103b2 .. 0x000103c5 (20 bytes, zero-init)
 *
 * Result symbol addresses:
 *   result_sqrt_i : 0x000103b2
 *   result_bits   : 0x000103b6
 *   result_sum    : 0x000103ba
 *   result_gcd    : 0x000103be
 *   result_fib    : 0x000103c2
 ****************************************************************/

#define LOAD_ADDR  0x00010024
#define ENTRY_ADDR 0x00010024

static const uint8_t test_image[] = {

    /* _start */
    0x51,0x8f,                          /* subql #8,%sp */
    0xf2,0x17,0xf0,0x20,                /* fmovemd %fp2,%sp@ */
    0x2f,0x03,                          /* movel %d3,%sp@- */
    0x2f,0x02,                          /* movel %d2,%sp@- */
    0x48,0x78,0x00,0x0a,                /* pea a <_start-0x1001a> */
    0x4e,0xb9,0x00,0x01,0x01,0x04,      /* jsr 10104 <fibonacci> */
    0x75,0xfc,0x00,0xfc,                /* mvzw #252,%d2 */
    0x58,0x8f,                          /* addql #4,%sp */
    0x72,0x69,                          /* moveq #105,%d1 */
    0x23,0xc0,0x00,0x01,0x03,0xc2,      /* movel %d0,103c2 <result_fib> */
    0x4c,0x41,0x20,0x03,                /* remul %d1,%d3,%d2 */
    0x20,0x01,                          /* movel %d1,%d0 */
    0x24,0x00,                          /* movel %d0,%d2 */
    0x22,0x03,                          /* movel %d3,%d1 */
    0x4a,0x83,                          /* tstl %d3 */
    0x66,0xf2,                          /* bnes 10046 <_start+0x22> */
    0x42,0xa7,                          /* clrl %sp@- */
    0x2f,0x3c,0x3f,0xf0,0x00,0x00,      /* movel #1072693248,%sp@- */
    0xf2,0x1f,0x54,0x44,                /* fdmoved %sp@+,%fp0 */
    0x73,0xfc,0x13,0xba,                /* mvzw #5050,%d1 */
    0x77,0xfc,0x0a,0x55,                /* mvzw #2645,%d3 */
    0x23,0xc0,0x00,0x01,0x03,0xbe,      /* movel %d0,103be <result_gcd> */
    0x70,0x14,                          /* moveq #20,%d0 */
    0x23,0xc1,0x00,0x01,0x03,0xba,      /* movel %d1,103ba <result_sum> */
    0x23,0xc3,0x00,0x01,0x03,0xb6,      /* movel %d3,103b6 <result_bits> */
    0x53,0x80,                          /* subql #1,%d0 */
    0x42,0xa7,                          /* clrl %sp@- */
    0x2f,0x3c,0x40,0x00,0x00,0x00,      /* movel #1073741824,%sp@- */
    0xf2,0x1f,0x54,0xc4,                /* fdmoved %sp@+,%fp1 */
    0xf2,0x00,0x00,0xe4,                /* fddivd %fp0,%fp1 */
    0xf2,0x00,0x04,0x66,                /* fdaddd %fp1,%fp0 */
    0x42,0xa7,                          /* clrl %sp@- */
    0x2f,0x3c,0x3f,0xe0,0x00,0x00,      /* movel #1071644672,%sp@- */
    0xf2,0x1f,0x54,0xc4,                /* fdmoved %sp@+,%fp1 */
    0xf2,0x00,0x04,0x67,                /* fdmuld %fp1,%fp0 */
    0x4a,0x80,                          /* tstl %d0 */
    0x66,0xd6,                          /* bnes 1007c <_start+0x58> */
    0x42,0xa7,                          /* clrl %sp@- */
    0x2f,0x3c,0x40,0x8f,0x40,0x00,      /* movel #1083129856,%sp@- */
    0xf2,0x1f,0x54,0xc4,                /* fdmoved %sp@+,%fp1 */
    0x42,0xa7,                          /* clrl %sp@- */
    0x2f,0x3c,0x41,0xe0,0x00,0x00,      /* movel #1105199104,%sp@- */
    0xf2,0x1f,0x55,0x44,                /* fdmoved %sp@+,%fp2 */
    0xf2,0x00,0x04,0x67,                /* fdmuld %fp1,%fp0 */
    0xf2,0x00,0x08,0x38,                /* fcmpd %fp2,%fp0 */
    0xf2,0x93,0x00,0x14,                /* fbge 100dc <_start+0xb8> */
    0xf2,0x00,0x00,0x03,                /* fintrzd %fp0,%fp0 */
    0xf2,0x00,0x60,0x00,                /* fmovel %fp0,%d0 */
    0x23,0xc0,0x00,0x01,0x03,0xb2,      /* movel %d0,103b2 <result_sqrt_i> */
    0x4e,0x40,                          /* trap #0 */
    0x60,0xfe,                          /* bras 100da <_start+0xb6> */
    0x42,0xa7,                          /* clrl %sp@- */
    0x2f,0x3c,0x41,0xe0,0x00,0x00,      /* movel #1105199104,%sp@- */
    0xf2,0x1f,0x54,0xc4,                /* fdmoved %sp@+,%fp1 */
    0xf2,0x00,0x04,0x6c,                /* fdsubd %fp1,%fp0 */
    0xf2,0x00,0x00,0x03,                /* fintrzd %fp0,%fp0 */
    0xf2,0x00,0x60,0x00,                /* fmovel %fp0,%d0 */
    0x06,0x80,0x80,0x00,0x00,0x00,      /* addil #-2147483648,%d0 */
    0x23,0xc0,0x00,0x01,0x03,0xb2,      /* movel %d0,103b2 <result_sqrt_i> */
    0x4e,0x40,                          /* trap #0 */
    0x60,0xd6,                          /* bras 100da <_start+0xb6> */

    /* fibonacci */
    0x4f,0xef,0xff,0x8c,                /* lea %sp@(-116),%sp */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0x20,0x6f,0x00,0x78,                /* moveal %sp@(120),%a0 */
    0x48,0xd7,0x7c,0xfc,                /* moveml %d2-%d7/%a2-%fp,%sp@ */
    0xb0,0x88,                          /* cmpl %a0,%d0 */
    0x67,0x00,0x01,0xe6,                /* beqw 102fc <fibonacci+0x1f8> */
    0x20,0x08,                          /* movel %a0,%d0 */
    0x72,0xfe,                          /* moveq #-2,%d1 */
    0x53,0x80,                          /* subql #1,%d0 */
    0xc2,0x80,                          /* andl %d0,%d1 */
    0x24,0x08,                          /* movel %a0,%d2 */
    0x2f,0x48,0x00,0x3c,                /* movel %a0,%sp@(60) */
    0x42,0xaf,0x00,0x40,                /* clrl %sp@(64) */
    0x20,0x40,                          /* moveal %d0,%a0 */
    0x94,0x81,                          /* subl %d1,%d2 */
    0x2f,0x42,0x00,0x64,                /* movel %d2,%sp@(100) */
    0x22,0x2f,0x00,0x64,                /* movel %sp@(100),%d1 */
    0xb2,0xaf,0x00,0x3c,                /* cmpl %sp@(60),%d1 */
    0x67,0x00,0x01,0xba,                /* beqw 102f6 <fibonacci+0x1f2> */
    0x55,0xaf,0x00,0x3c,                /* subql #2,%sp@(60) */
    0x70,0xfe,                          /* moveq #-2,%d0 */
    0xc0,0xaf,0x00,0x3c,                /* andl %sp@(60),%d0 */
    0x22,0x08,                          /* movel %a0,%d1 */
    0x42,0xaf,0x00,0x44,                /* clrl %sp@(68) */
    0x92,0x80,                          /* subl %d0,%d1 */
    0x2f,0x41,0x00,0x68,                /* movel %d1,%sp@(104) */
    0x22,0x08,                          /* movel %a0,%d1 */
    0x20,0x41,                          /* moveal %d1,%a0 */
    0x53,0x88,                          /* subql #1,%a0 */
    0xb2,0xaf,0x00,0x68,                /* cmpl %sp@(104),%d1 */
    0x67,0x00,0x02,0x2a,                /* beqw 1038a <fibonacci+0x286> */
    0x55,0x81,                          /* subql #2,%d1 */
    0x70,0xfe,                          /* moveq #-2,%d0 */
    0xc0,0x81,                          /* andl %d1,%d0 */
    0x24,0x08,                          /* movel %a0,%d2 */
    0x2f,0x41,0x00,0x58,                /* movel %d1,%sp@(88) */
    0x42,0xaf,0x00,0x48,                /* clrl %sp@(72) */
    0x22,0x08,                          /* movel %a0,%d1 */
    0x94,0x80,                          /* subl %d0,%d2 */
    0x2f,0x42,0x00,0x6c,                /* movel %d2,%sp@(108) */
    0x22,0x41,                          /* moveal %d1,%a1 */
    0x53,0x89,                          /* subql #1,%a1 */
    0xb2,0xaf,0x00,0x6c,                /* cmpl %sp@(108),%d1 */
    0x67,0x00,0x01,0xec,                /* beqw 10370 <fibonacci+0x26c> */
    0x55,0x81,                          /* subql #2,%d1 */
    0x70,0xfe,                          /* moveq #-2,%d0 */
    0xc0,0x81,                          /* andl %d1,%d0 */
    0x24,0x09,                          /* movel %a1,%d2 */
    0x42,0xaf,0x00,0x4c,                /* clrl %sp@(76) */
    0x2f,0x41,0x00,0x5c,                /* movel %d1,%sp@(92) */
    0x94,0x80,                          /* subl %d0,%d2 */
    0x2f,0x42,0x00,0x70,                /* movel %d2,%sp@(112) */
    0x28,0x09,                          /* movel %a1,%d4 */
    0x53,0x84,                          /* subql #1,%d4 */
    0xb3,0xef,0x00,0x70,                /* cmpal %sp@(112),%a1 */
    0x67,0x00,0x01,0xb0,                /* beqw 10356 <fibonacci+0x252> */
    0x55,0x89,                          /* subql #2,%a1 */
    0x72,0xfe,                          /* moveq #-2,%d1 */
    0x20,0x09,                          /* movel %a1,%d0 */
    0x24,0x04,                          /* movel %d4,%d2 */
    0xc0,0x81,                          /* andl %d1,%d0 */
    0x42,0x81,                          /* clrl %d1 */
    0x2a,0x41,                          /* moveal %d1,%a5 */
    0x2f,0x49,0x00,0x60,                /* movel %a1,%sp@(96) */
    0x94,0x80,                          /* subl %d0,%d2 */
    0x2f,0x42,0x00,0x50,                /* movel %d2,%sp@(80) */
    0x26,0x04,                          /* movel %d4,%d3 */
    0x53,0x83,                          /* subql #1,%d3 */
    0xb8,0xaf,0x00,0x50,                /* cmpl %sp@(80),%d4 */
    0x67,0x00,0x01,0x74,                /* beqw 1033e <fibonacci+0x23a> */
    0x55,0x84,                          /* subql #2,%d4 */
    0x70,0xfe,                          /* moveq #-2,%d0 */
    0x42,0x86,                          /* clrl %d6 */
    0xc0,0x84,                          /* andl %d4,%d0 */
    0x22,0x43,                          /* moveal %d3,%a1 */
    0x93,0xc0,                          /* subal %d0,%a1 */
    0x2a,0x03,                          /* movel %d3,%d5 */
    0x53,0x85,                          /* subql #1,%d5 */
    0xb3,0xc3,                          /* cmpal %d3,%a1 */
    0x67,0x00,0x01,0x28,                /* beqw 10308 <fibonacci+0x204> */
    0x2e,0x03,                          /* movel %d3,%d7 */
    0x70,0xfe,                          /* moveq #-2,%d0 */
    0x55,0x83,                          /* subql #2,%d3 */
    0xc0,0x83,                          /* andl %d3,%d0 */
    0x57,0x87,                          /* subql #3,%d7 */
    0x24,0x07,                          /* movel %d7,%d2 */
    0x99,0xcc,                          /* subal %a4,%a4 */
    0x94,0x80,                          /* subl %d0,%d2 */
    0x2f,0x42,0x00,0x54,                /* movel %d2,%sp@(84) */
    0x24,0x05,                          /* movel %d5,%d2 */
    0x53,0x82,                          /* subql #1,%d2 */
    0xbe,0xaf,0x00,0x54,                /* cmpl %sp@(84),%d7 */
    0x67,0x00,0x01,0x18,                /* beqw 10318 <fibonacci+0x214> */
    0x70,0xfe,                          /* moveq #-2,%d0 */
    0x95,0xca,                          /* subal %a2,%a2 */
    0x2c,0x42,                          /* moveal %d2,%fp */
    0xc0,0x87,                          /* andl %d7,%d0 */
    0x9d,0xc0,                          /* subal %d0,%fp */
    0x20,0x42,                          /* moveal %d2,%a0 */
    0x53,0x88,                          /* subql #1,%a0 */
    0xbd,0xc2,                          /* cmpal %d2,%fp */
    0x67,0x00,0x01,0x14,                /* beqw 10328 <fibonacci+0x224> */
    0x22,0x08,                          /* movel %a0,%d1 */
    0x97,0xcb,                          /* subal %a3,%a3 */
    0x2f,0x48,0x00,0x2c,                /* movel %a0,%sp@(44) */
    0x20,0x41,                          /* moveal %d1,%a0 */
    0x48,0x68,0xff,0xff,                /* pea %a0@(-1) */
    0x2f,0x41,0x00,0x3c,                /* movel %d1,%sp@(60) */
    0x2f,0x49,0x00,0x38,                /* movel %a1,%sp@(56) */
    0x4e,0xba,0xfe,0xd6,                /* jsr %pc@(10104 <fibonacci>) */
    0x58,0x8f,                          /* addql #4,%sp */
    0xd7,0xc0,                          /* addal %d0,%a3 */
    0x22,0x2f,0x00,0x38,                /* movel %sp@(56),%d1 */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0x55,0x81,                          /* subql #2,%d1 */
    0x22,0x6f,0x00,0x34,                /* moveal %sp@(52),%a1 */
    0xb0,0x81,                          /* cmpl %d1,%d0 */
    0x65,0xda,                          /* bcss 1021e <fibonacci+0x11a> */
    0x20,0x6f,0x00,0x2c,                /* moveal %sp@(44),%a0 */
    0x20,0x02,                          /* movel %d2,%d0 */
    0x72,0xfe,                          /* moveq #-2,%d1 */
    0x57,0x80,                          /* subql #3,%d0 */
    0xc0,0x81,                          /* andl %d1,%d0 */
    0x45,0xf0,0xa8,0xfe,                /* lea %a0@(-2,%a2:l),%a2 */
    0x55,0x82,                          /* subql #2,%d2 */
    0x95,0xc0,                          /* subal %d0,%a2 */
    0xd5,0xcb,                          /* addal %a3,%a2 */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x82,                          /* cmpl %d2,%d0 */
    0x66,0xac,                          /* bnes 1020c <fibonacci+0x108> */
    0x47,0xea,0x00,0x01,                /* lea %a2@(1),%a3 */
    0x55,0x85,                          /* subql #2,%d5 */
    0xd9,0xcb,                          /* addal %a3,%a4 */
    0x55,0x87,                          /* subql #2,%d7 */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x85,                          /* cmpl %d5,%d0 */
    0x66,0x86,                          /* bnes 101f6 <fibonacci+0xf2> */
    0x24,0x0c,                          /* movel %a4,%d2 */
    0x52,0x82,                          /* addql #1,%d2 */
    0xdc,0x82,                          /* addl %d2,%d6 */
    0xa3,0x41,                          /* mov3ql #1,%d1 */
    0xb2,0x83,                          /* cmpl %d3,%d1 */
    0x65,0x00,0xff,0x5c,                /* bcsw 101d8 <fibonacci+0xd4> */
    0x2a,0x06,                          /* movel %d6,%d5 */
    0xda,0x83,                          /* addl %d3,%d5 */
    0xdb,0xc5,                          /* addal %d5,%a5 */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x84,                          /* cmpl %d4,%d0 */
    0x65,0x00,0xff,0x36,                /* bcsw 101c0 <fibonacci+0xbc> */
    0x20,0x0d,                          /* movel %a5,%d0 */
    0xd0,0x84,                          /* addl %d4,%d0 */
    0x22,0x6f,0x00,0x60,                /* moveal %sp@(96),%a1 */
    0xd1,0xaf,0x00,0x4c,                /* addl %d0,%sp@(76) */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x89,                          /* cmpl %a1,%d0 */
    0x65,0x00,0xfe,0xfe,                /* bcsw 1019c <fibonacci+0x98> */
    0x20,0x2f,0x00,0x4c,                /* movel %sp@(76),%d0 */
    0xd0,0x89,                          /* addl %a1,%d0 */
    0x22,0x2f,0x00,0x5c,                /* movel %sp@(92),%d1 */
    0xd1,0xaf,0x00,0x48,                /* addl %d0,%sp@(72) */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x81,                          /* cmpl %d1,%d0 */
    0x65,0x00,0xfe,0xc6,                /* bcsw 1017a <fibonacci+0x76> */
    0x20,0x41,                          /* moveal %d1,%a0 */
    0xa3,0x42,                          /* mov3ql #1,%d2 */
    0x22,0x2f,0x00,0x58,                /* movel %sp@(88),%d1 */
    0x20,0x2f,0x00,0x48,                /* movel %sp@(72),%d0 */
    0xd0,0x88,                          /* addl %a0,%d0 */
    0xd1,0xaf,0x00,0x44,                /* addl %d0,%sp@(68) */
    0xb4,0x81,                          /* cmpl %d1,%d2 */
    0x65,0x00,0xfe,0x8a,                /* bcsw 10156 <fibonacci+0x52> */
    0x20,0x2f,0x00,0x44,                /* movel %sp@(68),%d0 */
    0xd0,0x81,                          /* addl %d1,%d0 */
    0xd1,0xaf,0x00,0x40,                /* addl %d0,%sp@(64) */
    0xa3,0x42,                          /* mov3ql #1,%d2 */
    0xb4,0xaf,0x00,0x3c,                /* cmpl %sp@(60),%d2 */
    0x64,0x00,0x00,0xbe,                /* bccw 1039e <fibonacci+0x29a> */
    0x22,0x2f,0x00,0x64,                /* movel %sp@(100),%d1 */
    0x20,0x2f,0x00,0x3c,                /* movel %sp@(60),%d0 */
    0x53,0x80,                          /* subql #1,%d0 */
    0x20,0x40,                          /* moveal %d0,%a0 */
    0xb2,0xaf,0x00,0x3c,                /* cmpl %sp@(60),%d1 */
    0x66,0x00,0xfe,0x4a,                /* bnew 1013e <fibonacci+0x3a> */
    0x20,0x6f,0x00,0x40,                /* moveal %sp@(64),%a0 */
    0xd1,0xc0,                          /* addal %d0,%a0 */
    0x4c,0xd7,0x7c,0xfc,                /* moveml %sp@,%d2-%d7/%a2-%fp */
    0x20,0x08,                          /* movel %a0,%d0 */
    0x4f,0xef,0x00,0x74,                /* lea %sp@(116),%sp */
    0x4e,0x75,                          /* rts */
    0xda,0x86,                          /* addl %d6,%d5 */
    0xdb,0xc5,                          /* addal %d5,%a5 */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x84,                          /* cmpl %d4,%d0 */
    0x65,0x00,0xfe,0xae,                /* bcsw 101c0 <fibonacci+0xbc> */
    0x60,0x00,0xff,0x76,                /* braw 1028c <fibonacci+0x188> */
    0xd4,0x8c,                          /* addl %a4,%d2 */
    0xdc,0x82,                          /* addl %d2,%d6 */
    0xa3,0x41,                          /* mov3ql #1,%d1 */
    0xb2,0x83,                          /* cmpl %d3,%d1 */
    0x65,0x00,0xfe,0xb6,                /* bcsw 101d8 <fibonacci+0xd4> */
    0x60,0x00,0xff,0x58,                /* braw 1027e <fibonacci+0x17a> */
    0x26,0x48,                          /* moveal %a0,%a3 */
    0xd7,0xca,                          /* addal %a2,%a3 */
    0x55,0x85,                          /* subql #2,%d5 */
    0xd9,0xcb,                          /* addal %a3,%a4 */
    0x55,0x87,                          /* subql #2,%d7 */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x85,                          /* cmpl %d5,%d0 */
    0x66,0x00,0xfe,0xbe,                /* bnew 101f6 <fibonacci+0xf2> */
    0x60,0x00,0xff,0x34,                /* braw 10270 <fibonacci+0x16c> */
    0x20,0x03,                          /* movel %d3,%d0 */
    0xd0,0x8d,                          /* addl %a5,%d0 */
    0x22,0x6f,0x00,0x60,                /* moveal %sp@(96),%a1 */
    0xd1,0xaf,0x00,0x4c,                /* addl %d0,%sp@(76) */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x89,                          /* cmpl %a1,%d0 */
    0x65,0x00,0xfe,0x4c,                /* bcsw 1019c <fibonacci+0x98> */
    0x60,0x00,0xff,0x4c,                /* braw 102a0 <fibonacci+0x19c> */
    0x20,0x2f,0x00,0x4c,                /* movel %sp@(76),%d0 */
    0xd0,0x84,                          /* addl %d4,%d0 */
    0x22,0x2f,0x00,0x5c,                /* movel %sp@(92),%d1 */
    0xd1,0xaf,0x00,0x48,                /* addl %d0,%sp@(72) */
    0xa3,0x40,                          /* mov3ql #1,%d0 */
    0xb0,0x81,                          /* cmpl %d1,%d0 */
    0x65,0x00,0xfe,0x10,                /* bcsw 1017a <fibonacci+0x76> */
    0x60,0x00,0xff,0x48,                /* braw 102b6 <fibonacci+0x1b2> */
    0x22,0x2f,0x00,0x58,                /* movel %sp@(88),%d1 */
    0xa3,0x42,                          /* mov3ql #1,%d2 */
    0x20,0x2f,0x00,0x48,                /* movel %sp@(72),%d0 */
    0xd0,0x89,                          /* addl %a1,%d0 */
    0xd1,0xaf,0x00,0x44,                /* addl %d0,%sp@(68) */
    0xb4,0x81,                          /* cmpl %d1,%d2 */
    0x65,0x00,0xfd,0xd2,                /* bcsw 10156 <fibonacci+0x52> */
    0x60,0x00,0xff,0x46,                /* braw 102ce <fibonacci+0x1ca> */
    0x20,0x2f,0x00,0x44,                /* movel %sp@(68),%d0 */
    0xd0,0x88,                          /* addl %a0,%d0 */
    0xd1,0xaf,0x00,0x40,                /* addl %d0,%sp@(64) */
    0xa3,0x42,                          /* mov3ql #1,%d2 */
    0xb4,0xaf,0x00,0x3c,                /* cmpl %sp@(60),%d2 */
    0x65,0x00,0xff,0x46,                /* bcsw 102e2 <fibonacci+0x1de> */
    0x4c,0xd7,0x7c,0xfc,                /* moveml %sp@,%d2-%d7/%a2-%fp */
    0x20,0x6f,0x00,0x3c,                /* moveal %sp@(60),%a0 */
    0xd1,0xef,0x00,0x40,                /* addal %sp@(64),%a0 */
    0x20,0x08,                          /* movel %a0,%d0 */
    0x4f,0xef,0x00,0x74,                /* lea %sp@(116),%sp */
    0x4e,0x75,                          /* rts */

    /* .results — 20 bytes, zero-initialized */
    0x00,0x00,0x00,0x00,                /* result_sqrt_i @ 0x103b2 */
    0x00,0x00,0x00,0x00,                /* result_bits   @ 0x103b6 */
    0x00,0x00,0x00,0x00,                /* result_sum    @ 0x103ba */
    0x00,0x00,0x00,0x00,                /* result_gcd    @ 0x103be */
    0x00,0x00,0x00,0x00,                /* result_fib    @ 0x103c2 */
};

/****************************************************************
 * Flat memory model — 16 MB
 ****************************************************************/

#define MEM_SIZE (16 * 1024 * 1024)

static uint8_t mem[MEM_SIZE];

static uint32_t
mem_read8(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        return mem[addr];
    return 0;
}

static uint32_t
mem_read16(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 1 < MEM_SIZE)
        return ((uint32_t)mem[addr] << 8) | mem[addr + 1];
    return 0;
}

static uint32_t
mem_read32(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr + 3 < MEM_SIZE)
        return ((uint32_t)mem[addr] << 24) | ((uint32_t)mem[addr + 1] << 16) |
               ((uint32_t)mem[addr + 2] << 8) | mem[addr + 3];
    return 0;
}

static void
mem_write8(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr < MEM_SIZE)
        mem[addr] = val & 0xFF;
}

static void
mem_write16(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 1 < MEM_SIZE) {
        mem[addr]     = (val >> 8) & 0xFF;
        mem[addr + 1] = val & 0xFF;
    }
}

static void
mem_write32(void *ctx, uint32_t addr, uint32_t val)
{
    (void)ctx;
    if (addr + 3 < MEM_SIZE) {
        mem[addr]     = (val >> 24) & 0xFF;
        mem[addr + 1] = (val >> 16) & 0xFF;
        mem[addr + 2] = (val >> 8) & 0xFF;
        mem[addr + 3] = val & 0xFF;
    }
}

/****************************************************************
 * Test runner
 ****************************************************************/

struct smoke_check {
    const char *name;
    uint32_t addr;
    uint32_t expected;
};

int
main(void)
{
    cf_cpu cpu;
    int executed, passed, failed, i;

    struct smoke_check checks[] = {
        { "fibonacci(10)", 0x000103c2, 55 },
        { "gcd(252, 105)", 0x000103be, 21 },
        { "sum_to(100)",   0x000103ba, 5050 },
        { "bit_test(0xAB)",0x000103b6, 0x0A55 },
        { "sqrt(2)*1000",  0x000103b2, 1414 },
    };
    int nchecks = sizeof(checks) / sizeof(checks[0]);

    /* Clear memory and load embedded image */
    memset(mem, 0, MEM_SIZE);
    memcpy(mem + LOAD_ADDR, test_image, sizeof(test_image));

    /* Set up vector table */
    mem_write32(NULL, 0x00, MEM_SIZE);          /* SSP = top of RAM */
    mem_write32(NULL, 0x04, ENTRY_ADDR);        /* PC = entry */
    mem_write32(NULL, 32 * 4, 0x00000200);      /* TRAP #0 vector */
    mem_write16(NULL, 0x200, 0x4AC8);           /* HALT instruction */

    /* Init and reset CPU */
    cf_init(&cpu, mem_read8, mem_read16, mem_read32,
            mem_write8, mem_write16, mem_write32, NULL);
    cf_reset(&cpu);

    /* Run */
    executed = cf_run(&cpu, 100000);

    /* Check results */
    passed = 0;
    failed = 0;
    for (i = 0; i < nchecks; i++) {
        uint32_t actual = mem_read32(NULL, checks[i].addr);
        int ok = (actual == checks[i].expected);
        printf("  %-20s got %-10u expected %-10u %s\n",
               checks[i].name, actual, checks[i].expected,
               ok ? "PASS" : "FAIL");
        if (ok)
            passed++;
        else
            failed++;
    }
    printf("%d/%d smoke tests passed (%d instructions)\n",
           passed, nchecks, executed);

    return failed > 0 ? 1 : 0;
}
