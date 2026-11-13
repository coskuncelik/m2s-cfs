# m2s-cfs: options for both cFE build levels (the host-side mission build and the target build).
# counter_app's public headers (message IDs) for other apps' tables, e.g. the sch_lab table override in
# tables/sch_lab_table.c (step 11c, D39).
include("${CMAKE_CURRENT_LIST_DIR}/m2s_paths.cmake")
include_directories("${M2S_ROOT_DIR}/apps/counter_app/fsw/inc")
