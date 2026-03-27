/*
 * monitor_link.ld -- Linker script for Triton monitor ROM
 *
 * The monitor ROM lives in NOR flash at 0x01200000. The vector table
 * must be the first thing in the binary (ColdFire fetches reset vectors
 * from VBR + 0x00 and VBR + 0x04).
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x01200000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x00000000, LENGTH = 8M
}

SECTIONS
{
    /* Vector table: must be first, at exactly 0x01200000 */
    .vectors : {
        *(.text.vectors)
    } > FLASH

    /* Monitor entry point */
    .text.entry : {
        *(.text.entry)
    } > FLASH

    /* Monitor code */
    .text : {
        *(.text .text.*)
    } > FLASH

    /* Read-only data */
    .rodata : {
        *(.rodata .rodata.*)
    } > FLASH

    /* Initialized data (copied to RAM if needed, but monitor runs from flash) */
    .data : {
        *(.data .data.*)
    } > FLASH

    /* BSS in RAM (monitor can use RAM for variables) */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Stack at top of RAM */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}

ENTRY(_monitor_start)
