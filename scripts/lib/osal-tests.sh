#!/usr/bin/env bash
# scripts/lib/osal-tests.sh — the OSAL functional test set and its allowed failures, shared by script 04
# (FreeRTOS simulator) and script 09b (Cortex-M3 on QEMU). Sourced, not run.

# Test programs, as paths relative to the build directory; the CMake target is the file name.
#   tests/       OSAL example/functional tests (04a, 04b, 04c)
#   unit-tests/  OSAL API-exhaustive functional tests (04c)
OSAL_TESTS=(
    tests/osal-core-test tests/queue-test tests/mutex-test
    tests/bin-sem-test tests/bin-sem-timeout-test tests/count-sem-test tests/count-sem-timeout-test
    tests/idmap-api-test tests/sem-speed-test
    tests/timer-test tests/time-base-api-test tests/timer-add-api-test
    tests/file-api-test tests/file-sys-add-fixed-map-api-test
    tests/condvar-test tests/select-test tests/shell-test tests/symbol-api-test tests/network-api-test
    unit-tests/oscore-test/osal_core_UT unit-tests/osfile-test/osal_file_UT
    unit-tests/osfilesys-test/osal_filesys_UT unit-tests/ostimer-test/osal_timer_UT
    unit-tests/osloader-test/osal_loader_UT unit-tests/osnetwork-test/osal_network_UT
)

# Allowed assertions on every target, as "<test name>|<text that identifies one FAIL/TSF/TTF line>". A run
# passes only if its failed assertions are exactly the entries listed for that test (same count, each one
# matched), so a new failure or a missing expected one is still an error.
#   D26: features left out by configuration (osal_config.cmake): loading modules from files
#        (osal_loader_UT loads /utmod/MODULE*.so; the dynamic loader is off).
#   The 5 network-api-test entries of D26 are gone: networking is on in the simulator (host sockets,
#   step 12a, D41). Callers whose configuration has networking off (Cortex-M3, script 09b) add them back
#   with OSAL_KNOWN_FAILS_NO_NETWORK.
OSAL_KNOWN_FAILS_NO_NETWORK=(
    "network-api-test|network-api-test.c:99 - OS_SocketAddrSetPort(&addr, 0xFFFF) (-28)"
    "network-api-test|network-api-test.c:103 - OS_SocketAddrGetPort(&port, &addr) (-28)"
    "network-api-test|network-api-test.c:108 - OS_SocketAddrInit(&addr, OS_SocketDomain_INET) (-28)"
    "network-api-test|network-api-test.c:113 - OS_SocketAddrFromString(&addr, \"invalid\") (-28)"
    "network-api-test|network-api-test.c:119 - OS_SocketAddrToString(addr_string, 1, &addr) (-28)"
)
OSAL_KNOWN_FAILS_CONFIG=(
    "osal_loader_UT|ut_osloader_module_test.c:128 - OS_ModuleLoad(&module_id, module_name, module_file_name, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:141 - OS_ModuleLoad(&module_id2, \"DUPLICATE\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:156 - OS_ModuleLoad(&module_id, \"Good\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:159 - OS_ModuleUnload(module_id) (-16)"
    "osal_loader_UT|ut_osloader_module_test.c:195 - OS_ModuleLoad(&module_id, \"Good\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:232 - OS_ModuleLoad(&module_id, \"Good\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_symtable_test.c:158 - OS_ModuleLoad(&module_id, \"Mod1\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
)

# Prints the number of KNOWN_FAILS entries for test $1 if every FAIL/TSF/TTF line in log $2 matches
# one of them and the counts are equal; prints nothing otherwise.
known_fails_match() {
    local name="$1" out="$2" entry count=0 line ok
    local -a lines=()
    mapfile -t lines < <(grep -E '^\[ *(FAIL|TSF|TTF)\]' "${out}" || true)
    for entry in "${KNOWN_FAILS[@]}"; do
        [[ "${entry%%|*}" == "${name}" ]] && count=$(( count + 1 ))
    done
    (( count > 0 && count == ${#lines[@]} )) || return 0
    for line in "${lines[@]}"; do
        ok=0
        for entry in "${KNOWN_FAILS[@]}"; do
            if [[ "${entry%%|*}" == "${name}" && "${line}" == *"${entry#*|}"* ]]; then ok=1; break; fi
        done
        (( ok )) || return 0
    done
    echo "${count}"
}
