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
    .globl __cont_capture
    .globl __cont_resume

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

| __cont_capture()
|   Called by IR_CAPTURE inline code after pushing d2-d7.  The jsr return
|   address on the stack becomes the resume entry point (captured in the
|   stack segment; __cont_resume's rts pops it).
|
|   Reads the mark slot via __cont_mark_sp, bump-allocates a buffer,
|   copies the stack segment, then longjmps to the mark's re-entry label
|   with d0 = buffer address.
|
|   On entry:  d2-d7 are on the stack (pushed by IR_CAPTURE inline code)
|              (%sp) = return address to IR_CAPTURE resume point
|   Clobbers:  d0-d7, a0-a1 (callee-saves are in the captured segment)
__cont_capture:
    move.l  %sp, %d4                | d4 = capture_sp
    move.l  %fp, %d5                | d5 = capture_fp

    | Read the 12-byte mark slot pointed to by __cont_mark_sp
    lea     __cont_mark_sp, %a0
    move.l  (%a0), %a1              | a1 = mark slot pointer
    move.l  (%a1), %d2              | d2 = mark's saved_fp
    move.l  4(%a1), %d3             | d3 = mark's saved_sp
    move.l  8(%a1), %d7             | d7 = mark's re-entry PC

    | Stack segment size (mark_sp - capture_sp; stack grows down)
    move.l  %d3, %d6
    sub.l   %d4, %d6               | d6 = size

    | Bump-allocate: 12-byte header + size bytes of stack data
    lea     __cont_arena_ptr, %a0
    move.l  (%a0), %d1             | d1 = buf (start of buffer)

    | Write header [capture_sp, capture_fp, size]
    movea.l %d1, %a1
    move.l  %d4, (%a1)            | buf[0] = capture_sp
    move.l  %d5, 4(%a1)           | buf[4] = capture_fp
    move.l  %d6, 8(%a1)           | buf[8] = size

    | Advance arena pointer past this buffer
    move.l  %d1, %d0
    add.l   #12, %d0
    add.l   %d6, %d0
    lea     __cont_arena_ptr, %a0
    move.l  %d0, (%a0)

    | Copy stack data: capture_sp -> buf+12
    movea.l %d1, %a0
    lea     12(%a0), %a0          | a0 = dst (buf + 12)
    movea.l %d4, %a1              | a1 = src (capture_sp)
    move.l  %d6, %d0
    lsr.l   #2, %d0               | d0 = word count
    bra     2f
1:  move.l  (%a1), %d6
    move.l  %d6, (%a0)
    addq.l  #4, %a1
    addq.l  #4, %a0
2:  subq.l  #1, %d0
    bpl     1b

    | Longjmp to mark: restore FP/SP, d0 = buf, jump to re-entry label
    move.l  %d2, %fp
    movea.l %d3, %sp
    move.l  %d1, %d0              | d0 = buffer address (nonzero)
    movea.l %d7, %a0
    jmp     (%a0)                  | jump to IR_MARK's re-entry label

| __cont_resume(buf, value)
|   Restores the captured stack segment and returns into it via rts.
|   The rts pops the jsr __cont_capture return address, which lands at
|   the IR_CAPTURE inline code's resume point.
|
|   On entry:  4(%sp) = buf pointer, 8(%sp) = resume value
__cont_resume:
    move.l  4(%sp), %a1            | a1 = buf
    move.l  8(%sp), %d1            | d1 = resume value

    | Read buffer header
    move.l  (%a1), %d2             | d2 = capture_sp
    move.l  4(%a1), %d3            | d3 = capture_fp
    move.l  8(%a1), %d6            | d6 = size

    | Restore FP and SP to captured state
    move.l  %d3, %fp
    movea.l %d2, %sp

    | Copy stack data back: buf+12 -> capture_sp
    lea     12(%a1), %a1           | a1 = src (buf + 12)
    movea.l %d2, %a0              | a0 = dst (capture_sp)
    lsr.l   #2, %d6               | d6 = word count
    bra     4f
3:  move.l  (%a1), %d0
    move.l  %d0, (%a0)
    addq.l  #4, %a1
    addq.l  #4, %a0
4:  subq.l  #1, %d6
    bpl     3b

    | Return into the restored continuation
    move.l  %d1, %d0              | d0 = resume value
    rts

    .data
    .align 4
    .globl __cont_mark_sp
__cont_mark_sp:
    .long 0
__cont_arena_ptr:
    .long __cont_arena

    .bss
    .align 4
__cont_arena:
    .space 65536
    .space 65536
stack_top:
