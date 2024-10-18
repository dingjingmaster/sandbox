include(${CMAKE_SOURCE_DIR}/3thrd/clib/c/c.cmake)
include(${CMAKE_SOURCE_DIR}/3thrd/fs/libsandbox-fs.cmake)
#include(${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/libfuse-lite.cmake)

add_executable(test-format format.c ${C_SRC} ${SANDBOX_FS_SRC} ${FUSE_LITE_SRC}
        ../app/sandbox-fs.c
        ../app/fs/sd.c
        ../app/fs/boot.c
        ../app/fs/utils.c
        ../app/fs/attrdef.c
)
target_link_libraries(test-format PUBLIC
        ${GLIB_LIBRARIES}
        ${CLIB_LIBRARIES}
        ${FUSE_LIBRARIES}
)

target_include_directories(test-format PUBLIC
        ${GLIB_INCLUDE_DIRS}
        ${CLIB_INCLUDE_DIRS}
        ${FUSE_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3thrd/clib
)

target_compile_definitions(test-format PUBLIC
        -D_GNU_SOURCE
        -DHAVE_CONFIG_H
        -D__CLIB_H_INSIDE__
        -D_FILE_OFFSET_BITS=64
        -DPACKAGE_NAME=\"test-format\"
)

add_executable(test-check check.c ${C_SRC} ${SANDBOX_FS_SRC} ${FUSE_LITE_SRC}
        ../app/fs/sd.c
        ../app/fs/boot.c
        ../app/fs/utils.c
        ../app/fs/attrdef.c
        ../app/sandbox-fs.c
)
target_link_libraries(test-check PUBLIC
        ${GLIB_LIBRARIES}
        ${CLIB_LIBRARIES}
        ${FUSE_LIBRARIES}
)

target_include_directories(test-check PUBLIC
        ${GLIB_INCLUDE_DIRS}
        ${CLIB_INCLUDE_DIRS}
        ${FUSE_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3thrd/clib
)

target_compile_definitions(test-check PUBLIC
        -D_GNU_SOURCE
        -DHAVE_CONFIG_H
        -D__CLIB_H_INSIDE__
        -D_FILE_OFFSET_BITS=64
        -DPACKAGE_NAME=\"test-check\"
)

add_executable(test-resize resize.c ${C_SRC} ${SANDBOX_FS_SRC} ${FUSE_LITE_SRC}
        ../app/fs/sd.c
        ../app/fs/boot.c
        ../app/fs/utils.c
        ../app/fs/attrdef.c
        ../app/sandbox-fs.c
)
target_link_libraries(test-resize PUBLIC
        ${GLIB_LIBRARIES}
        ${CLIB_LIBRARIES}
        ${FUSE_LIBRARIES}
)

target_include_directories(test-resize PUBLIC
        ${GLIB_INCLUDE_DIRS}
        ${CLIB_INCLUDE_DIRS}
        ${FUSE_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3thrd/clib
)

target_compile_definitions(test-resize PUBLIC
        -D_GNU_SOURCE
        -DHAVE_CONFIG_H
        -D__CLIB_H_INSIDE__
        -D_FILE_OFFSET_BITS=64
        -DPACKAGE_NAME=\"test-check\"
)

add_executable(test-mount mount.c ${C_SRC} ${SANDBOX_FS_SRC} # ${FUSE_LITE_SRC}
        ../app/fs/sd.c
        ../app/fs/boot.c
        ../app/fs/utils.c
        ../app/fs/attrdef.c
        ../app/sandbox-fs.c
)
target_link_libraries(test-mount PUBLIC
        ${GLIB_LIBRARIES}
        ${CLIB_LIBRARIES}
        ${FUSE_LIBRARIES}
)

target_include_directories(test-mount PUBLIC
        ${GLIB_INCLUDE_DIRS}
        ${CLIB_INCLUDE_DIRS}
        ${FUSE_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3thrd/clib
)

target_compile_definitions(test-mount PUBLIC
        -D_GNU_SOURCE
        -DHAVE_CONFIG_H
        -D__CLIB_H_INSIDE__
        -D_FILE_OFFSET_BITS=64
        -DPACKAGE_NAME=\"test-check\"
)

