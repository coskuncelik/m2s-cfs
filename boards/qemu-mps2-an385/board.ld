/*
 * board.ld — m2s-cfs board "qemu-mps2-an385": memory map of QEMU mps2-an385 (step 09).
 *
 * Checked on QEMU 8.2.2 (`info mtree`): SSRAM1 4 MiB at 0x00000000 holds the vector table and code; PSRAM
 * 16 MiB at 0x21000000 holds data, bss and the main stack (bss is about 7.6 MB, more than the 4 MiB
 * SSRAM2/3 at 0x20000000). QEMU -kernel loads the ELF; .data is copied by Reset_Handler as on hardware.
 */
ENTRY(Reset_Handler)

MEMORY
{
    CODE (rx)  : ORIGIN = 0x00000000, LENGTH = 4M   /* SSRAM1 */
    RAM  (rwx) : ORIGIN = 0x21000000, LENGTH = 16M  /* PSRAM */
}

/* Main stack (start-up, then interrupts and exceptions once FreeRTOS runs). */
M2S_MAIN_STACK_BYTES = 4096;

SECTIONS
{
    .isr_vector : ALIGN(4)
    {
        KEEP(*(.isr_vector))
    } > CODE

    .text : ALIGN(4)
    {
        *(.text .text.*)
        *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
        KEEP(*(.init))
        KEEP(*(.fini))
    } > CODE

    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.*)
    } > CODE

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > CODE
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > CODE

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > CODE
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .main_stack (NOLOAD) : ALIGN(8)
    {
        . = . + M2S_MAIN_STACK_BYTES;
        . = ALIGN(8);
        _estack = .;
    } > RAM

    /* No heap: _sbrk traps (syscalls.c). */

    /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) }
}
