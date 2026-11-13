/*
 * counter_app_cmds.c — counter_app Software Bus and table handling: command pipe, commands (NOOP,
 * RESET_COUNTERS, SET_PERIOD), housekeeping telemetry on request, and the parameter table (default period and
 * SET_PERIOD limits), whose default contents are loaded from memory.
 */
#include "counter_app.h"
#include "counter_app_events.h"
#include "counter_app_msgids.h"

/* Default table contents, loaded with CFE_TBL_SRC_ADDRESS (cFE copies them into its own buffer). */
static const COUNTER_APP_ParamTable_t COUNTER_APP_DefaultParams = COUNTER_APP_TBL_DEFAULTS;

#define COUNTER_APP_TBL_INVALID (-1) /* validation result: contents rejected */

/* Table validation (cFE calls it for every load): limits inside the absolute bounds, default inside the limits. */
static int32 COUNTER_APP_ValidateParams(void *TblData)
{
    const COUNTER_APP_ParamTable_t *t = TblData;

    if (t->MinPeriodMs < COUNTER_APP_TBL_MIN_PERIOD_MS || t->MaxPeriodMs > COUNTER_APP_TBL_MAX_PERIOD_MS ||
        t->MinPeriodMs > t->MaxPeriodMs || t->DefaultPeriodMs < t->MinPeriodMs || t->DefaultPeriodMs > t->MaxPeriodMs)
    {
        CFE_EVS_SendEvent(COUNTER_APP_TBL_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Table validation error: default %lu ms, limits %lu..%lu ms (bounds %d..%d)",
                          (unsigned long)t->DefaultPeriodMs, (unsigned long)t->MinPeriodMs,
                          (unsigned long)t->MaxPeriodMs, COUNTER_APP_TBL_MIN_PERIOD_MS, COUNTER_APP_TBL_MAX_PERIOD_MS);
        return COUNTER_APP_TBL_INVALID;
    }
    return CFE_SUCCESS;
}

/* Copies the table's values into the app data (the period starts again from the table's default). */
static CFE_Status_t COUNTER_APP_ReadParams(const char *what)
{
    void        *addr = NULL;
    CFE_Status_t status = CFE_TBL_GetAddress(&addr, COUNTER_APP_Data.TblHandle);

    if (status == CFE_SUCCESS || status == CFE_TBL_INFO_UPDATED)
    {
        const COUNTER_APP_ParamTable_t *t = addr;
        uint32                          period = t->DefaultPeriodMs;

        COUNTER_APP_Data.MinPeriodMs = t->MinPeriodMs;
        COUNTER_APP_Data.MaxPeriodMs = t->MaxPeriodMs;
        (void)CFE_TBL_ReleaseAddress(COUNTER_APP_Data.TblHandle);
        COUNTER_APP_SetPeriod(period);
        CFE_EVS_SendEvent(COUNTER_APP_TBL_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "Table %s: period %lu ms, limits %lu..%lu ms", what, (unsigned long)period,
                          (unsigned long)COUNTER_APP_Data.MinPeriodMs, (unsigned long)COUNTER_APP_Data.MaxPeriodMs);
        status = CFE_SUCCESS;
    }
    return status;
}

CFE_Status_t COUNTER_APP_InitBusAndTable(void)
{
    CFE_Status_t status;

    CFE_MSG_Init(CFE_MSG_PTR(COUNTER_APP_Data.HkTlm.TelemetryHeader), CFE_SB_ValueToMsgId(COUNTER_APP_HK_TLM_MID),
                 sizeof(COUNTER_APP_Data.HkTlm));

    status = CFE_SB_CreatePipe(&COUNTER_APP_Data.CommandPipe, COUNTER_APP_PIPE_DEPTH, COUNTER_APP_PIPE_NAME);
    if (status == CFE_SUCCESS)
    {
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(COUNTER_APP_CMD_MID), COUNTER_APP_Data.CommandPipe);
    }
    if (status == CFE_SUCCESS)
    {
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(COUNTER_APP_SEND_HK_MID), COUNTER_APP_Data.CommandPipe);
    }
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(COUNTER_APP_SB_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Software Bus pipe or subscription error, status 0x%08lX", (unsigned long)(uint32)status);
        return status;
    }

    status = CFE_TBL_Register(&COUNTER_APP_Data.TblHandle, COUNTER_APP_TBL_NAME, sizeof(COUNTER_APP_ParamTable_t),
                              CFE_TBL_OPT_DEFAULT, COUNTER_APP_ValidateParams);
    if (status == CFE_SUCCESS)
    {
        status = CFE_TBL_Load(COUNTER_APP_Data.TblHandle, CFE_TBL_SRC_ADDRESS, &COUNTER_APP_DefaultParams);
    }
    if (status == CFE_SUCCESS)
    {
        status = COUNTER_APP_ReadParams("loaded");
    }
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(COUNTER_APP_TBL_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Table register or load error, status 0x%08lX", (unsigned long)(uint32)status);
    }
    return status;
}

/* Performs pending table actions (validation, activation of a new load); picks up new contents. */
void COUNTER_APP_ManageTable(void)
{
    if (CFE_TBL_Manage(COUNTER_APP_Data.TblHandle) == CFE_TBL_INFO_UPDATED)
    {
        (void)COUNTER_APP_ReadParams("updated");
    }
}

static void COUNTER_APP_SendHk(void)
{
    COUNTER_APP_HkTlm_Payload_t *p = &COUNTER_APP_Data.HkTlm.Payload;

    (void)OS_MutSemTake(COUNTER_APP_Data.DataMutex);
    p->Counter  = COUNTER_APP_Data.Counter;
    p->PeriodMs = COUNTER_APP_Data.PeriodMs;
    (void)OS_MutSemGive(COUNTER_APP_Data.DataMutex);
    p->LibCallStatus       = COUNTER_APP_Data.LibCallStatus;
    p->CommandCounter      = COUNTER_APP_Data.CommandCounter;
    p->CommandErrorCounter = COUNTER_APP_Data.CommandErrorCounter;
    p->FileTestFailedStep  = COUNTER_APP_Data.FileTestFailedStep;
    p->Spare               = 0;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(COUNTER_APP_Data.HkTlm.TelemetryHeader));
    (void)CFE_SB_TransmitMsg(CFE_MSG_PTR(COUNTER_APP_Data.HkTlm.TelemetryHeader), true);

    if (!COUNTER_APP_Data.HkSentOnce)
    {
        COUNTER_APP_Data.HkSentOnce = true;
        CFE_EVS_SendEvent(COUNTER_APP_HK_FIRST_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "First housekeeping packet sent (MID 0x%04lX)", (unsigned long)COUNTER_APP_HK_TLM_MID);
    }
}

/* true if the command has the length its code needs; else an error event and the error counter + 1. */
static bool COUNTER_APP_LengthOk(const CFE_MSG_Message_t *MsgPtr, size_t expected, CFE_MSG_FcnCode_t cc)
{
    CFE_MSG_Size_t actual = 0;

    (void)CFE_MSG_GetSize(MsgPtr, &actual);
    if (actual != expected)
    {
        CFE_EVS_SendEvent(COUNTER_APP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Command length error: code %u, length %lu, expected %lu", (unsigned int)cc,
                          (unsigned long)actual, (unsigned long)expected);
        ++COUNTER_APP_Data.CommandErrorCounter;
        return false;
    }
    return true;
}

static void COUNTER_APP_ProcessCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t cc = 0;

    (void)CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &cc);
    switch (cc)
    {
        case COUNTER_APP_NOOP_CC:
            if (COUNTER_APP_LengthOk(&SBBufPtr->Msg, sizeof(COUNTER_APP_NoArgsCmd_t), cc))
            {
                ++COUNTER_APP_Data.CommandCounter;
                CFE_EVS_SendEvent(COUNTER_APP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION, "NOOP command received");
            }
            break;

        case COUNTER_APP_RESET_COUNTERS_CC:
            if (COUNTER_APP_LengthOk(&SBBufPtr->Msg, sizeof(COUNTER_APP_NoArgsCmd_t), cc))
            {
                COUNTER_APP_Data.CommandCounter      = 0;
                COUNTER_APP_Data.CommandErrorCounter = 0;
                CFE_EVS_SendEvent(COUNTER_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "Command counters reset");
            }
            break;

        case COUNTER_APP_SET_PERIOD_CC:
            if (COUNTER_APP_LengthOk(&SBBufPtr->Msg, sizeof(COUNTER_APP_SetPeriodCmd_t), cc))
            {
                uint32 ms = ((const COUNTER_APP_SetPeriodCmd_t *)SBBufPtr)->Payload.PeriodMs;

                if (ms < COUNTER_APP_Data.MinPeriodMs || ms > COUNTER_APP_Data.MaxPeriodMs)
                {
                    ++COUNTER_APP_Data.CommandErrorCounter;
                    CFE_EVS_SendEvent(COUNTER_APP_PERIOD_ERR_EID, CFE_EVS_EventType_ERROR,
                                      "SET_PERIOD %lu ms rejected: limits %lu..%lu ms", (unsigned long)ms,
                                      (unsigned long)COUNTER_APP_Data.MinPeriodMs,
                                      (unsigned long)COUNTER_APP_Data.MaxPeriodMs);
                }
                else
                {
                    ++COUNTER_APP_Data.CommandCounter;
                    COUNTER_APP_SetPeriod(ms);
                    CFE_EVS_SendEvent(COUNTER_APP_PERIOD_INF_EID, CFE_EVS_EventType_INFORMATION,
                                      "Period set to %lu ms", (unsigned long)ms);
                }
            }
            break;

        default:
            ++COUNTER_APP_Data.CommandErrorCounter;
            CFE_EVS_SendEvent(COUNTER_APP_CC_ERR_EID, CFE_EVS_EventType_ERROR, "Unknown command code %u",
                              (unsigned int)cc);
            break;
    }
}

void COUNTER_APP_ProcessMessage(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t mid = CFE_SB_INVALID_MSG_ID;

    (void)CFE_MSG_GetMsgId(&SBBufPtr->Msg, &mid);
    if (CFE_SB_MsgIdToValue(mid) == COUNTER_APP_CMD_MID)
    {
        COUNTER_APP_ProcessCommand(SBBufPtr);
    }
    else if (CFE_SB_MsgIdToValue(mid) == COUNTER_APP_SEND_HK_MID)
    {
        COUNTER_APP_SendHk();
    }
    else
    {
        CFE_EVS_SendEvent(COUNTER_APP_MID_ERR_EID, CFE_EVS_EventType_ERROR, "Unexpected message ID 0x%04lX",
                          (unsigned long)CFE_SB_MsgIdToValue(mid));
    }
}
