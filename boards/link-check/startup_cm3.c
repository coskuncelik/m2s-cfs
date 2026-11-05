/*
 * startup_cm3.c — m2s-cfs generic ARMv7-M (Cortex-M3) start-up: vector table and reset handler.
 *
 * Not Microchip code. The vector table holds the 16 core exception entries only; a real board adds its
 * device interrupts. SVCall, PendSV and SysTick go straight to the FreeRTOS ARM_CM3 port handlers, which
 * the port checks at scheduler start (configCHECK_HANDLER_INSTALLATION).
 *
 * Reset: copy .data from its load address, clear .bss, then main(). No C runtime start files
 * (-nostartfiles): newlib's crt0 is not used, and no C++ constructors are needed.
 */
#include <stdint.h>

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

void Reset_Handler(void);
void M2S_DefaultHandler(void);

/* Address of the last fault handler entered, for a debugger. */
volatile uint32_t M2S_FaultIpsr;

void M2S_DefaultHandler(void)
{
    uint32_t ipsr;

    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    M2S_FaultIpsr = ipsr; /* exception number */
    __asm volatile("cpsid i");
    for (;;)
    {
    }
}

void NMI_Handler(void) __attribute__((weak, alias("M2S_DefaultHandler")));
void HardFault_Handler(void) __attribute__((weak, alias("M2S_DefaultHandler")));
void MemManage_Handler(void) __attribute__((weak, alias("M2S_DefaultHandler")));
void BusFault_Handler(void) __attribute__((weak, alias("M2S_DefaultHandler")));
void UsageFault_Handler(void) __attribute__((weak, alias("M2S_DefaultHandler")));
void DebugMon_Handler(void) __attribute__((weak, alias("M2S_DefaultHandler")));

typedef void (*M2S_Vector_t)(void);

__attribute__((section(".isr_vector"), used)) const M2S_Vector_t M2S_VectorTable[16] = {
    (M2S_Vector_t)(uintptr_t)_estack, /* 0: initial MSP */
    Reset_Handler,                    /* 1 */
    NMI_Handler,                      /* 2 */
    HardFault_Handler,                /* 3 */
    MemManage_Handler,                /* 4 */
    BusFault_Handler,                 /* 5 */
    UsageFault_Handler,               /* 6 */
    0,                                /* 7-10: reserved */
    0,
    0,
    0,
    vPortSVCHandler,                  /* 11: SVCall */
    DebugMon_Handler,                 /* 12 */
    0,                                /* 13: reserved */
    xPortPendSVHandler,               /* 14: PendSV */
    xPortSysTickHandler,              /* 15: SysTick */
};

void Reset_Handler(void)
{
    uint32_t *src = _sidata;
    uint32_t *dst;

    for (dst = _sdata; dst < _edata; ++dst)
    {
        *dst = *src++;
    }
    for (dst = _sbss; dst < _ebss; ++dst)
    {
        *dst = 0U;
    }

    (void)main();

    M2S_DefaultHandler(); /* main() never returns on this target */
}
