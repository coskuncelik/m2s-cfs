/*
 * counter_app_msg.h — counter_app command codes, command and housekeeping telemetry formats.
 */
#ifndef COUNTER_APP_MSG_H
#define COUNTER_APP_MSG_H

#include "common_types.h"
#include "cfe_msg_hdr.h"

/* Command codes (function codes) on COUNTER_APP_CMD_MID. */
#define COUNTER_APP_NOOP_CC           0 /* no payload: event and command counter + 1 */
#define COUNTER_APP_RESET_COUNTERS_CC 1 /* no payload: command and error counters to 0 */
#define COUNTER_APP_SET_PERIOD_CC     2 /* payload: period in ms, within the table's limits */

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} COUNTER_APP_NoArgsCmd_t;

typedef struct
{
    uint32 PeriodMs;
} COUNTER_APP_SetPeriod_Payload_t;

typedef struct
{
    CFE_MSG_CommandHeader_t         CommandHeader;
    COUNTER_APP_SetPeriod_Payload_t Payload;
} COUNTER_APP_SetPeriodCmd_t;

/* Housekeeping telemetry, sent on each request on COUNTER_APP_SEND_HK_MID. */
typedef struct
{
    uint32 Counter;             /* counter value last printed */
    uint32 PeriodMs;            /* period in use */
    int32  LibCallStatus;       /* status returned by the sample_lib call at start-up */
    uint8  CommandCounter;      /* commands accepted */
    uint8  CommandErrorCounter; /* commands rejected */
    uint8  FileTestFailedStep;  /* 0: the start-up file test passed; else the first failing step */
    uint8  Spare;
} COUNTER_APP_HkTlm_Payload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t   TelemetryHeader;
    COUNTER_APP_HkTlm_Payload_t Payload;
} COUNTER_APP_HkTlm_t;

#endif /* COUNTER_APP_MSG_H */
