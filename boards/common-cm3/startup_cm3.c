/*
 * startup_cm3.c — m2s-cfs generic ARMv7-M (Cortex-M3) start-up: vector table, reset handler, fault report.
 *
 * Not Microchip code; only ARMv7-M architecture registers (ARM DDI 0403) are used. The vector table holds
 * the 16 core exception entries; a board that uses device interrupts adds them. SVCall, PendSV and SysTick
 * go straight to the FreeRTOS ARM_CM3 port handlers, which the port checks at scheduler start
 * (configCHECK_HANDLER_INSTALLATION, through VTOR).
 *
 * Reset_Handler:
 *  1. quiesce what a boot loader (U-Boot on the SmartFusion2 kit) may have left running: SysTick off, every
 *     NVIC interrupt disabled and its pending bit cleared, PendSV/SysTick pending cleared. After a real
 *     reset this changes nothing;
 *  2. VTOR = this vector table. Valid only where the table is VTOR-addressable (below 0x40000000); a board
 *     whose image runs from elsewhere (SmartFusion2 DDR) must copy the table first — board step;
 *  3. copy .data from its load address, clear .bss;
 *  4. enable interrupts (a boot loader may leave PRIMASK set; FreeRTOS masks with BASEPRI until the
 *     scheduler starts), then main().
 * No C runtime start files (-nostartfiles): newlib's crt0 is not used, and no C++ constructors are needed.
 *
 * Faults (HardFault, MemManage, BusFault, UsageFault, NMI, DebugMon, reserved vectors) print one
 * "M2S FAULT" line with the exception number, the fault status registers and the stacked PC, LR and xPSR,
 * then halt.
 */
#include <stdint.h>

#include "m2s_cm3_diag.h"

/* Linker script symbols */
extern uint32_t _sidata[]; /* load address of .data */
extern uint32_t _sdata[];
extern uint32_t _edata[];
extern uint32_t _sbss[];
extern uint32_t _ebss[];
extern uint32_t _estack[]; /* initial main stack pointer */

extern int main(void);

/* FreeRTOS ARM_CM3 port handlers */
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

/* ARMv7-M system registers */
#define M2S_REG(addr)        (*(volatile uint32_t *)(addr))
#define M2S_SYST_CSR         M2S_REG(0xE000E010UL)
#define M2S_ICTR             M2S_REG(0xE000E004UL) /* number of NVIC interrupt lines / 32, minus 1 */
#define M2S_NVIC_ICER(n)     M2S_REG(0xE000E180UL + 4UL * (n))
#define M2S_NVIC_ICPR(n)     M2S_REG(0xE000E280UL + 4UL * (n))
#define M2S_SCB_ICSR         M2S_REG(0xE000ED04UL)
#define M2S_SCB_VTOR         M2S_REG(0xE000ED08UL)
#define M2S_SCB_CFSR         M2S_REG(0xE000ED28UL)
#define M2S_SCB_HFSR         M2S_REG(0xE000ED2CUL)
#define M2S_SCB_MMFAR        M2S_REG(0xE000ED34UL)
#define M2S_SCB_BFAR         M2S_REG(0xE000ED38UL)
#define M2S_ICSR_PENDSVCLR   (1UL << 27)
#define M2S_ICSR_PENDSTCLR   (1UL << 25)

void Reset_Handler(void);
void M2S_FaultEntry(void);
void M2S_FaultReport(const uint32_t *Frame, uint32_t ExcReturn);

void NMI_Handler(void) __attribute__((weak, alias("M2S_FaultEntry")));
void HardFault_Handler(void) __attribute__((weak, alias("M2S_FaultEntry")));
void MemManage_Handler(void) __attribute__((weak, alias("M2S_FaultEntry")));
void BusFault_Handler(void) __attribute__((weak, alias("M2S_FaultEntry")));
void UsageFault_Handler(void) __attribute__((weak, alias("M2S_FaultEntry")));
void DebugMon_Handler(void) __attribute__((weak, alias("M2S_FaultEntry")));

typedef void (*M2S_Vector_t)(void);

__attribute__((section(".isr_vector"), used)) const M2S_Vector_t M2S_VectorTable[16] = {
    (M2S_Vector_t)(uintptr_t)_estack, /* 0: initial MSP */
    Reset_Handler,                    /* 1 */
    NMI_Handler,                      /* 2 */
    HardFault_Handler,                /* 3 */
    MemManage_Handler,                /* 4 */
    BusFault_Handler,                 /* 5 */
    UsageFault_Handler,               /* 6 */
    M2S_FaultEntry,                   /* 7-10: reserved */
    M2S_FaultEntry,
    M2S_FaultEntry,
    M2S_FaultEntry,
    vPortSVCHandler,                  /* 11: SVCall */
    DebugMon_Handler,                 /* 12 */
    M2S_FaultEntry,                   /* 13: reserved */
    xPortPendSVHandler,               /* 14: PendSV */
    xPortSysTickHandler,              /* 15: SysTick */
};

/* Exception entry: pass the stacked frame (MSP or PSP, from EXC_RETURN bit 2) and EXC_RETURN to C. */
__attribute__((naked)) void M2S_FaultEntry(void)
{
    __asm volatile(
        "tst   lr, #4            \n"
        "ite   eq                \n"
        "mrseq r0, msp           \n"
        "mrsne r0, psp           \n"
        "mov   r1, lr            \n"
        "b     M2S_FaultReport   \n");
}

void M2S_FaultReport(const uint32_t *Frame, uint32_t ExcReturn)
{
    uint32_t ipsr;

    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    M2S_Diag_Write("\nM2S FAULT exception ");
    M2S_Diag_WriteDec(ipsr & 0x1FFU);
    M2S_Diag_Write(" CFSR ");
    M2S_Diag_WriteHex(M2S_SCB_CFSR);
    M2S_Diag_Write(" HFSR ");
    M2S_Diag_WriteHex(M2S_SCB_HFSR);
    M2S_Diag_Write(" MMFAR ");
    M2S_Diag_WriteHex(M2S_SCB_MMFAR);
    M2S_Diag_Write(" BFAR ");
    M2S_Diag_WriteHex(M2S_SCB_BFAR);
    M2S_Diag_Write(" PC ");
    M2S_Diag_WriteHex(Frame[6]);
    M2S_Diag_Write(" LR ");
    M2S_Diag_WriteHex(Frame[5]);
    M2S_Diag_Write(" xPSR ");
    M2S_Diag_WriteHex(Frame[7]);
    M2S_Diag_Write(" EXC_RETURN ");
    M2S_Diag_WriteHex(ExcReturn);
    M2S_Diag_Write("\n");
    M2S_Halt();
}

void Reset_Handler(void)
{
    uint32_t *src = _sidata;
    uint32_t *dst;
    uint32_t  n;
    uint32_t  words;

    /* 1. Quiesce what a boot loader may have left running. */
    M2S_SYST_CSR = 0U;
    words        = (M2S_ICTR & 0xFU) + 1U;
    for (n = 0; n < words; ++n)
    {
        M2S_NVIC_ICER(n) = 0xFFFFFFFFUL;
        M2S_NVIC_ICPR(n) = 0xFFFFFFFFUL;
    }
    M2S_SCB_ICSR = M2S_ICSR_PENDSVCLR | M2S_ICSR_PENDSTCLR;

    /* 2. Vector table. */
    M2S_SCB_VTOR = (uint32_t)(uintptr_t)M2S_VectorTable;
    __asm volatile("dsb\n isb" ::: "memory");

    /* 3. C environment. */
    for (dst = _sdata; dst < _edata; ++dst)
    {
        *dst = *src++;
    }
    for (dst = _sbss; dst < _ebss; ++dst)
    {
        *dst = 0U;
    }

    /* 4. Interrupts on, then the application. */
    __asm volatile("cpsie i" ::: "memory");
    (void)main();

    M2S_Diag_Write("\nM2S STOP main() returned\n");
    M2S_Halt();
}
