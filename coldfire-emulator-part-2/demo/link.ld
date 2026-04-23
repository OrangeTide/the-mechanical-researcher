/* link.ld : linker script for bare-metal ColdFire V4e test programs */
/* Memory starts at 0x10000 (64K), leaving room for vector table at 0x0 */

OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00010000, LENGTH = 0x100000
}

SECTIONS
{
    /* Vector table (0x0–0x3FF) is populated by test harness at runtime */

    /* Code */
    .text : {
        *(.text.entry)
        *(.text .text.*)
    } > RAM

    /* Read-only data */
    .rodata : {
        *(.rodata .rodata.*)
    } > RAM

    /* Initialized data */
    .data : {
        *(.data .data.*)
    } > RAM

    /* Test result slots */
    .results : {
        __results_start = .;
        *(.results)
        __results_end = .;
    } > RAM

    /* Uninitialized data */
    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Stack at top of RAM */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
