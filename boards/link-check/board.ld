/*
 * board.ld — m2s-cfs board "link-check": PLACEHOLDER memory map for the step 08 cross-build.
 *
 * Not a real device map. Both regions are large so the link always succeeds and the map file reports
 * the real section sizes; whether the image fits the SmartFusion2 board is decided with the board's map.
 */
ENTRY(Reset_Handler)

MEMORY
{
    CODE (rx)  : ORIGIN = 0x00000000, LENGTH = 16M
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 64M
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
        /* Global offset table of position-independent objects (OSAL's ut_assert is built with
         * POSITION_INDEPENDENT_CODE): inside .data, so Reset_Handler copies its link-time addresses
         * (outside it, every GOT entry would read 0 at run time). */
        *(.got .got.*)
        *(.got.plt)
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
