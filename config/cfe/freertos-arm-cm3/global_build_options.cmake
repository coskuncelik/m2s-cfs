# m2s-cfs: options for both cFE build levels (the host-side mission build and the target build).
# The mission's cfe_<module>_internal_cfg.h files include cFE's stock default_cfe_<module>_internal_cfg.h and
# change single values (step 10, D36); both levels compile code that includes them (e.g. the host-side
# platform-data tool), so the stock config folders are made reachable by name here.
include("${CMAKE_CURRENT_LIST_DIR}/m2s_paths.cmake")
foreach(m2s_module es sb tbl time)
    include_directories("${M2S_ROOT_DIR}/sources/cFS/cfe/modules/${m2s_module}/config")
endforeach()
# counter_app's public headers (message IDs) for other apps' tables, e.g. the sch_lab table override in
# tables/sch_lab_table.c (step 11c, D39).
include_directories("${M2S_ROOT_DIR}/apps/counter_app/fsw/inc")
