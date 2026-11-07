# m2s-cfs functional-test mission: the RAM root volume holding /cf must also hold cFE's test log
# /cf/cfe_test.log (about 250 KB); 1 MiB instead of the default 32 KiB (os-freertos.h).
add_definitions(-DOS_FREERTOS_RAMFS_ROOT_BYTES=1048576U)
