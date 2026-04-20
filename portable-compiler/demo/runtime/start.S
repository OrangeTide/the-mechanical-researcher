| start.S - minimal m68k/ColdFire runtime for Linux user-mode.
|
| Provides _start (the ELF entry point), and thin wrappers for the two
| syscalls the test programs need: write(2) and exit(2). qemu-m68k uses
| the standard Linux/m68k syscall convention:
|
|   d0 = syscall number
|   d1..d5 = args (we only need d1, d2, d3)
|   trap #0 to invoke
|   return value in d0
|
| _start drops argc/argv (tests take no arguments), calls main, then
| passes main's return value to _exit.

    .text
    .globl _start
    .globl write
    .globl exit
    .globl _exit

_start:
    lea     stack_top, %sp      | switch to our own stack (qemu-m68k's
    sub.l   #16, %sp            | default stack is a single page; recursive
                                | tests overflow it within a few frames)
    jsr     main
    move.l  %d0, %d1            | exit status
    move.l  #1, %d0             | __NR_exit
    trap    #0
    | unreachable
1:  bra     1b

| int write(int fd, const char *buf, int n);
write:
    move.l  4(%sp), %d1         | fd
    move.l  8(%sp), %d2         | buf
    move.l  12(%sp), %d3        | n
    move.l  #4, %d0              | __NR_write
    trap    #0
    rts

| void exit(int status);  (alias _exit)
exit:
_exit:
    move.l  4(%sp), %d1
    move.l  #1, %d0              | __NR_exit
    trap    #0
1:  bra     1b

    .bss
    .align 4
    .space 65536
stack_top:
