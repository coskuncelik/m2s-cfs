/*
 * counter_app.h — counter_app internal data and functions shared by its source files.
 */
#ifndef COUNTER_APP_H
#define COUNTER_APP_H

#include "cfe.h"
#include "counter_app_msg.h"
#include "counter_app_tbl.h"

#define COUNTER_APP_PIPE_DEPTH 8
#define COUNTER_APP_PIPE_NAME  "COUNTER_CMD_PIPE"

typedef struct
{
    uint32              RunStatus;
    uint32              Counter;
    uint32              PeriodMs;            /* period in use */
    uint32              MinPeriodMs;         /* SET_PERIOD limits, from the table */
    uint32              MaxPeriodMs;
    uint8               CommandCounter;
    uint8               CommandErrorCounter;
    uint8               FileTestFailedStep;  /* 0: passed */
    bool                HkSentOnce;
    int32               LibCallStatus;
    CFE_SB_PipeId_t     CommandPipe;
    CFE_TBL_Handle_t    TblHandle;
    COUNTER_APP_HkTlm_t HkTlm;
} COUNTER_APP_Data_t;

extern COUNTER_APP_Data_t COUNTER_APP_Data;

/* counter_app_cmds.c */
CFE_Status_t COUNTER_APP_InitBusAndTable(void);
void         COUNTER_APP_ManageTable(void);
void         COUNTER_APP_ProcessMessage(const CFE_SB_Buffer_t *SBBufPtr);

#endif /* COUNTER_APP_H */
