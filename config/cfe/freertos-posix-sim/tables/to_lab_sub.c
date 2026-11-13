/*
 * to_lab_sub.c — m2s-cfs mission override of NASA to_lab's subscription table (cFE picks
 * <mission config>/tables/to_lab_sub.c in place of the app's default; step 12b, D42).
 *
 * Telemetry forwarded over UDP once to_lab's OUTPUT_ENABLE command has set the destination: counter_app
 * housekeeping, the ci_lab and to_lab housekeeping, and EVS event messages (long and short format).
 */
#include "cfe_tbl_filedef.h"     /* CFE_TBL_FILEDEF */
#include "cfe_sb_api_typedefs.h" /* CFE_SB_MSGID_WRAP_VALUE */
#include "to_lab_tbl.h"
#include "cfe_msgids.h"
#include "to_lab_msgids.h"
#include "ci_lab_msgids.h"       /* on the include path: to_lab's CMakeLists adds it when ci_lab is built */
#include "counter_app_msgids.h"  /* on the include path through global_build_options.cmake */

TO_LAB_Subs_t TO_LAB_Subs = {.Subs = {
                                 {CFE_SB_MSGID_WRAP_VALUE(COUNTER_APP_HK_TLM_MID), {0, 0}, 4},
                                 {CFE_SB_MSGID_WRAP_VALUE(CI_LAB_HK_TLM_MID), {0, 0}, 4},
                                 {CFE_SB_MSGID_WRAP_VALUE(TO_LAB_HK_TLM_MID), {0, 0}, 4},
                                 {CFE_SB_MSGID_WRAP_VALUE(CFE_EVS_LONG_EVENT_MSG_MID), {0, 0}, 32},
                                 {CFE_SB_MSGID_WRAP_VALUE(CFE_EVS_SHORT_EVENT_MSG_MID), {0, 0}, 32},

                                 /* CFE_SB_MSGID_RESERVED entry to mark the end of valid MsgIds */
                                 {CFE_SB_MSGID_RESERVED, {0, 0}, 0}}};

CFE_TBL_FILEDEF(TO_LAB_Subs, TO_LAB_APP.TO_LAB_Subs, TO Lab Sub Tbl, to_lab_sub.tbl)
