
#
# cfs-m2s-freertos additions (appended to the stock file above)
#

# /cf image. The PSP unpacks the embedded archive cf.tar into /cf at start-up. cFE's embed rule
# (cpu1_EMBED_FILELIST in targets.cmake) only looks in this directory, so the archive is written here
# (ignored by git). It is a deterministic POSIX ustar archive (fixed order, owner, mode and time): an
# unchanged startup script gives a byte-identical cf.tar, and core-cpu1 is not relinked. Editing
# cpu1_cfe_es_startup.scr re-runs this configure step on the next "make". To add files to /cf (e.g.
# table images), copy them into M2S_CF_STAGE below as well.
find_program(M2S_TAR_EXECUTABLE tar)
if(NOT M2S_TAR_EXECUTABLE)
    message(FATAL_ERROR "GNU tar is needed to build the /cf image (sample_defs/cf.tar)")
endif()
set(M2S_CF_STAGE "${CMAKE_BINARY_DIR}/cf-image")
file(REMOVE_RECURSE "${M2S_CF_STAGE}")
file(MAKE_DIRECTORY "${M2S_CF_STAGE}")
configure_file("${MISSION_DEFS}/cpu1_cfe_es_startup.scr" "${M2S_CF_STAGE}/cfe_es_startup.scr" COPYONLY)
execute_process(
    COMMAND "${M2S_TAR_EXECUTABLE}" --format=ustar --sort=name --owner=0 --group=0 --numeric-owner
            --mtime=@0 --mode=u+rwX,go+rX,go-w -C "${M2S_CF_STAGE}" -cf "${CMAKE_BINARY_DIR}/cf.tar" .
    RESULT_VARIABLE M2S_TAR_RESULT)
if(NOT M2S_TAR_RESULT EQUAL 0)
    message(FATAL_ERROR "building the /cf image failed (GNU tar 1.28 or later is needed for --sort)")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_BINARY_DIR}/cf.tar" "${MISSION_DEFS}/cf.tar")

# cFE's install rule copies tables/staging even when no app has tables. cFE's prepare() recreates the
# tables directory after this file is read, so the directory is made at the end of the configure step.
if(CMAKE_VERSION VERSION_LESS 3.19)
    message(FATAL_ERROR "CMake 3.19 or later is needed (cmake_language DEFER)")
endif()
cmake_language(DEFER CALL file MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tables/staging")
