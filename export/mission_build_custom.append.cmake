
#
# cfs-m2s-freertos additions (appended to the stock file above)
#

# /cf image. The PSP unpacks the embedded archive cf.tar into /cf at start-up. cFE's embed rule
# (cpu1_EMBED_FILELIST in targets.cmake) only looks in this directory, so the archive is written here
# (ignored by git). It is a deterministic POSIX ustar archive (fixed order, owner, mode and time): unchanged
# contents give a byte-identical cf.tar, and core-cpu1 is not relinked.
#   - at configure time: the startup script only (cFE's embed rule needs the file to exist);
#   - at build time, before core-cpu1 is built: the startup script plus every table image (*.tbl) that the
#     build has made for cpu1 (target m2s-cf-image, after cFE's table tool; see the end of this file). So a
#     single "make" puts the apps' default tables into /cf, and a changed table is embedded on the next make.
find_program(M2S_TAR_EXECUTABLE tar)
if(NOT M2S_TAR_EXECUTABLE)
    message(FATAL_ERROR "GNU tar is needed to build the /cf image (cf.tar in the mission config folder)")
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

# Build-time /cf image with the table images (see above). The script is the same tar command, run on a stage
# that also holds the staged tables of cpu1 (cFE's table tool writes them to tables/staging/cpu1/cf).
set(M2S_CF_SCRIPT "${CMAKE_BINARY_DIR}/m2s-cf-image.cmake")
file(WRITE "${M2S_CF_SCRIPT}" "
set(stage \"${CMAKE_BINARY_DIR}/cf-image-build\")
file(REMOVE_RECURSE \"\${stage}\")
file(MAKE_DIRECTORY \"\${stage}\")
configure_file(\"${MISSION_DEFS}/cpu1_cfe_es_startup.scr\" \"\${stage}/cfe_es_startup.scr\" COPYONLY)
file(GLOB tables \"${CMAKE_BINARY_DIR}/tables/staging/cpu1/cf/*.tbl\")
foreach(t IN LISTS tables)
    configure_file(\"\${t}\" \"\${stage}/\" COPYONLY)
endforeach()
execute_process(
    COMMAND \"${M2S_TAR_EXECUTABLE}\" --format=ustar --sort=name --owner=0 --group=0 --numeric-owner
            --mtime=@0 --mode=u+rwX,go+rX,go-w -C \"\${stage}\" -cf \"${CMAKE_BINARY_DIR}/cf-build.tar\" .
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR \"building the /cf image failed\")
endif()
execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different
                \"${CMAKE_BINARY_DIR}/cf-build.tar\" \"${MISSION_DEFS}/cf.tar\")
")
add_custom_target(m2s-cf-image
    COMMAND "${CMAKE_COMMAND}" -P "${M2S_CF_SCRIPT}"
    COMMENT "Building the /cf image (startup script and table images)")

# The targets that build core-cpu1 (<system>-all, <system>-install) are created after this file is read, and
# cFE's table tool target (tabletool-execute) in its tools folder: the dependencies are added at the end.
function(m2s_cf_image_dependencies)
    add_dependencies(m2s-cf-image tabletool-execute)
    foreach(sys IN LISTS TGTSYS_LIST)
        add_dependencies(${sys}-all m2s-cf-image)
        add_dependencies(${sys}-install m2s-cf-image)
    endforeach()
endfunction()
cmake_language(DEFER CALL m2s_cf_image_dependencies)
