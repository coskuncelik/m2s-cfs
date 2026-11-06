/*
 * cfe_psp_support.c — m2s-cfs freertos-arm-cm3 PSP: restart, panic, caches, IDs.
 *
 * Restart: a system reset through the ARMv7-M Application Interrupt and Reset Control Register; static
 * memory does not survive it, so the next start is a power-on reset. Panic: one "M2S PANIC" line straight
 * to the board console (the OSAL console task may not be running), then the CPU stops with interrupts off
 * and the error code kept for a debugger.
 */
#include <stdint.h>

#include "common_types.h"
#include "osapi.h"
#include "cfe_psp.h"
#include "target_config.h"
#include "m2s_cm3_diag.h"

#define CFE_PSP_CPU_ID        (GLOBAL_CONFIGDATA.Default_CpuId)
#define CFE_PSP_CPU_NAME      (GLOBAL_CONFIGDATA.Default_CpuName)
#define CFE_PSP_SPACECRAFT_ID (GLOBAL_CONFIGDATA.Default_SpacecraftId)

/* ARMv7-M System Control Block: AIRCR (ARM DDI 0403, B3.2.6). */
#define CFE_PSP_SCB_AIRCR             (*(volatile uint32_t *)0xE000ED0CUL)
#define CFE_PSP_SCB_AIRCR_VECTKEY     (0x05FAUL << 16)
#define CFE_PSP_SCB_AIRCR_PRIGROUP    (0x7UL << 8)
#define CFE_PSP_SCB_AIRCR_SYSRESETREQ (1UL << 2)

/* Error code of the last panic, for a debugger. */
volatile int32 CFE_PSP_PanicCode;

static void CFE_PSP_SystemReset(void)
{
    __asm volatile("dsb" ::: "memory");
    CFE_PSP_SCB_AIRCR = CFE_PSP_SCB_AIRCR_VECTKEY | (CFE_PSP_SCB_AIRCR & CFE_PSP_SCB_AIRCR_PRIGROUP) |
                        CFE_PSP_SCB_AIRCR_SYSRESETREQ;
    __asm volatile("dsb" ::: "memory");
    for (;;)
    {
        /* wait for the reset */
    }
}

void CFE_PSP_Restart(uint32 resetType)
{
    OS_printf("CFE_PSP: restart requested (type %u); system reset, the next start is a power-on reset\n",
              (unsigned int)resetType);
    OS_TaskDelay(100); /* let the console task print */
    CFE_PSP_SystemReset();
}

void CFE_PSP_Panic(int32 ErrorCode)
{
    __asm volatile("cpsid i" ::: "memory");
    CFE_PSP_PanicCode = ErrorCode;
    M2S_Diag_Write("\nM2S PANIC code ");
    M2S_Diag_WriteHex((uint32_t)ErrorCode);
    M2S_Diag_Write("\n");
    M2S_Halt();
}

void CFE_PSP_FlushCaches(uint32 type, void *address, uint32 size)
{
    (void)type;
    (void)address;
    (void)size; /* the Cortex-M3 has no data cache */
}

uint32 CFE_PSP_GetProcessorId(void)
{
    return CFE_PSP_CPU_ID;
}

uint32 CFE_PSP_GetSpacecraftId(void)
{
    return CFE_PSP_SPACECRAFT_ID;
}

const char *CFE_PSP_GetProcessorName(void)
{
    return CFE_PSP_CPU_NAME;
}
