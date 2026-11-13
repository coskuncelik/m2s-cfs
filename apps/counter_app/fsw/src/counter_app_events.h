/*
 * counter_app_events.h — m2s-cfs counter_app: event IDs (cFE Event Services).
 */
#ifndef COUNTER_APP_EVENTS_H
#define COUNTER_APP_EVENTS_H

#define COUNTER_APP_INIT_INF_EID      1 /* information: app initialized */
#define COUNTER_APP_FILE_TEST_INF_EID 2 /* information: start-up file test passed */
#define COUNTER_APP_FILE_TEST_ERR_EID 3 /* error: a file test step went wrong */
#define COUNTER_APP_CF_READ_INF_EID   4 /* information: startup script read from /cf */
#define COUNTER_APP_LIB_CALL_INF_EID  5 /* information: sample_lib function returned success */
#define COUNTER_APP_LIB_CALL_ERR_EID  6 /* error: sample_lib function returned an error status */
#define COUNTER_APP_HK_FIRST_INF_EID  7 /* information: first housekeeping packet sent */
#define COUNTER_APP_TBL_INF_EID       8 /* information: parameter table loaded or updated */
#define COUNTER_APP_TBL_ERR_EID       9 /* error: parameter table registration, load or validation */
#define COUNTER_APP_NOOP_INF_EID      10 /* information: NOOP command */
#define COUNTER_APP_RESET_INF_EID     11 /* information: RESET_COUNTERS command */
#define COUNTER_APP_PERIOD_INF_EID    12 /* information: SET_PERIOD command accepted */
#define COUNTER_APP_PERIOD_ERR_EID    13 /* error: SET_PERIOD outside the table's limits */
#define COUNTER_APP_CC_ERR_EID        14 /* error: unknown command code */
#define COUNTER_APP_LEN_ERR_EID       15 /* error: command length does not match its code */
#define COUNTER_APP_MID_ERR_EID       16 /* error: message ID not handled by this app */
#define COUNTER_APP_SB_ERR_EID        17 /* error: Software Bus pipe or subscription */
#define COUNTER_APP_WORKER_INF_EID    18 /* information: worker task and timer started */
#define COUNTER_APP_WORKER_ERR_EID    19 /* error: worker task, timer, semaphore or mutex */

#endif /* COUNTER_APP_EVENTS_H */
