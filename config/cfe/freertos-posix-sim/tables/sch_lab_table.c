/*
 * sch_lab_table.c — m2s-cfs mission override of NASA sch_lab's schedule table (cFE picks
 * <mission config>/tables/sch_lab_table.c in place of the app's default; step 11c, D39).
 *
 * counter_app's housekeeping request once per second (TickRate 10 Hz, every 10 ticks); on the simulator also
 * ci_lab's READ_UPLINK at 10 Hz (command latency <= 100 ms) and the ci_lab and to_lab housekeeping requests
 * (step 12b, D42). The cFE core housekeeping requests of the default table are not sent.
 */
#include "cfe_tbl_filedef.h"     /* CFE_TBL_FILEDEF */
#include "sch_lab_tbl.h"
#include "cfe_sb_api_typedefs.h" /* CFE_SB_MSGID_WRAP_VALUE */
#include "counter_app_msgids.h"  /* on the include path through global_build_options.cmake */
#include "ci_lab_msgids.h"       /* sch_lab's CMakeLists adds the ci_lab and to_lab include paths */
#include "to_lab_msgids.h"

SCH_LAB_ScheduleTable_t SCH_LAB_ScheduleTable = {
    .TickRate = 10,
    .Config   = {
        {CFE_SB_MSGID_WRAP_VALUE(COUNTER_APP_SEND_HK_MID), 10, 0}, /* 1 Hz */
        {CFE_SB_MSGID_WRAP_VALUE(CI_LAB_READ_UPLINK_MID), 1, 0},   /* 10 Hz */
        {CFE_SB_MSGID_WRAP_VALUE(CI_LAB_SEND_HK_MID), 10, 0},      /* 1 Hz */
        {CFE_SB_MSGID_WRAP_VALUE(TO_LAB_SEND_HK_MID), 10, 0},      /* 1 Hz */
    }};

CFE_TBL_FILEDEF(SCH_LAB_ScheduleTable, SCH_LAB_APP.ScheduleTable, Schedule Lab MsgID Table, sch_lab_table.tbl)
