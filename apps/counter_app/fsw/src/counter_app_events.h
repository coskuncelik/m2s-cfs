/*
 * counter_app_events.h — m2s-cfs counter_app: event IDs (cFE Event Services).
 */
#ifndef COUNTER_APP_EVENTS_H
#define COUNTER_APP_EVENTS_H

#define COUNTER_APP_INIT_INF_EID      1 /* information: app initialized */
#define COUNTER_APP_FILE_TEST_INF_EID 2 /* information: start-up file test passed */
#define COUNTER_APP_FILE_TEST_ERR_EID 3 /* error: a file test step went wrong */
#define COUNTER_APP_CF_READ_INF_EID   4 /* information: startup script read from /cf */

#endif /* COUNTER_APP_EVENTS_H */
