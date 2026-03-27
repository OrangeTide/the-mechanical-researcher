/*
 * hello_link.ld -- Linker script for Triton user programs
 *
 * Programs are loaded by the monitor ROM from an ELF staged at 0x00001000.
 * The monitor copies PT_LOAD segments to their p_vaddr and jumps to e_entry.
 * Code starts at 0x00010000 to leave room for the ELF staging area.
 */

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00010000, LENGTH = 0x7F0000
}

SECTIONS
{
    .text.entry : {
        *(.text.entry)
    } > RAM

    .text : {
        *(.text .text.*)
    } > RAM

    .rodata : {
        *(.rodata .rodata.*)
    } > RAM

    .data : {
        *(.data .data.*)
    } > RAM

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Stack below VRAM */
    __stack_top = 0x00800000;

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}

ENTRY(_start)
