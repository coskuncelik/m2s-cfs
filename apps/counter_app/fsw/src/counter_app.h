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

#define COUNTER_APP_WORKER_NAME     "COUNTER_WORKER"
#define COUNTER_APP_WORKER_STACK    4096
#define COUNTER_APP_WORKER_PRIORITY 49 /* just above the main task (50 in the startup script) */

/*
 * Counter and PeriodMs are shared by the main task and the worker task: access them under DataMutex. The other
 * fields belong to the main task (the worker only reads TickSem and DataMutex).
 */
typedef struct
{
    uint32              RunStatus;
    uint32              Counter;             /* shared */
    uint32              PeriodMs;            /* shared: period in use */
    uint32              MinPeriodMs;         /* SET_PERIOD limits, from the table */
    uint32              MaxPeriodMs;
    uint8               CommandCounter;
    uint8               CommandErrorCounter;
    uint8               FileTestFailedStep;  /* 0: passed */
    bool                HkSentOnce;
    int32               LibCallStatus;
    CFE_SB_PipeId_t     CommandPipe;
    CFE_TBL_Handle_t    TblHandle;
    osal_id_t           DataMutex;           /* guards the shared fields */
    osal_id_t           TickSem;             /* given by the timer callback, taken by the worker */
    osal_id_t           TimerId;             /* OS_OBJECT_ID_UNDEFINED until the timer exists */
    CFE_ES_TaskId_t     WorkerId;
    COUNTER_APP_HkTlm_t HkTlm;
} COUNTER_APP_Data_t;

extern COUNTER_APP_Data_t COUNTER_APP_Data;

/* counter_app.c */
void COUNTER_APP_SetPeriod(uint32 PeriodMs);

/* counter_app_cmds.c */
CFE_Status_t COUNTER_APP_InitBusAndTable(void);
void         COUNTER_APP_ManageTable(void);
void         COUNTER_APP_ProcessMessage(const CFE_SB_Buffer_t *SBBufPtr);

#endif /* COUNTER_APP_H */
