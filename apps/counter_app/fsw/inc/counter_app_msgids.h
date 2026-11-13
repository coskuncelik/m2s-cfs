/*
 * counter_app_msgids.h — counter_app Software Bus message IDs (topic IDs 0x90/0x91; the stock lab apps and
 * sample_app use 0x80-0x86).
 */
#ifndef COUNTER_APP_MSGIDS_H
#define COUNTER_APP_MSGIDS_H

#include "cfe_core_api_base_msgids.h"

#define COUNTER_APP_CMD_TOPICID     0x90 /* commands */
#define COUNTER_APP_SEND_HK_TOPICID 0x91 /* housekeeping request */
#define COUNTER_APP_HK_TLM_TOPICID  0x90 /* housekeeping telemetry */

#define COUNTER_APP_CMD_MID     CFE_PLATFORM_CMD_TOPICID_TO_MIDV(COUNTER_APP_CMD_TOPICID)
#define COUNTER_APP_SEND_HK_MID CFE_PLATFORM_CMD_TOPICID_TO_MIDV(COUNTER_APP_SEND_HK_TOPICID)
#define COUNTER_APP_HK_TLM_MID  CFE_PLATFORM_TLM_TOPICID_TO_MIDV(COUNTER_APP_HK_TLM_TOPICID)

#endif /* COUNTER_APP_MSGIDS_H */
