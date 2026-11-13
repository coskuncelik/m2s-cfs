/*
 * counter_app_tbl.h — counter_app parameter table: default period and the limits a SET_PERIOD command must
 * respect. The default contents are loaded from memory at start-up (no table file is needed).
 */
#ifndef COUNTER_APP_TBL_H
#define COUNTER_APP_TBL_H

#include "common_types.h"

#define COUNTER_APP_TBL_NAME "Params"

/* Bounds the validation function enforces on any table contents. */
#define COUNTER_APP_TBL_MIN_PERIOD_MS 100
#define COUNTER_APP_TBL_MAX_PERIOD_MS 10000

typedef struct
{
    uint32 DefaultPeriodMs; /* period used after start-up */
    uint32 MinPeriodMs;     /* smallest period SET_PERIOD accepts */
    uint32 MaxPeriodMs;     /* largest period SET_PERIOD accepts */
} COUNTER_APP_ParamTable_t;

/* Default contents. */
#define COUNTER_APP_TBL_DEFAULTS {1000, 500, 5000}

#endif /* COUNTER_APP_TBL_H */
