
file(GLOB SANDBOX_SRC
        ${CMAKE_SOURCE_DIR}/app rootfs.h
        ${CMAKE_SOURCE_DIR}/app rootfs.c

        ${CMAKE_SOURCE_DIR}/app environ.h
        ${CMAKE_SOURCE_DIR}/app environ.c

        ${CMAKE_SOURCE_DIR}/app sandbox.h
        ${CMAKE_SOURCE_DIR}/app sandbox.c

        ${CMAKE_SOURCE_DIR}/app namespace.h
        ${CMAKE_SOURCE_DIR}/app namespace.c

        ${CMAKE_SOURCE_DIR}/app sandbox-fs.h
        ${CMAKE_SOURCE_DIR}/app sandbox-fs.c

        ${CMAKE_SOURCE_DIR}/app utils.h
        ${CMAKE_SOURCE_DIR}/app utils.c

        ${CMAKE_SOURCE_DIR}/app fs/sd.h
        ${CMAKE_SOURCE_DIR}/app fs/sd.c

        ${CMAKE_SOURCE_DIR}/app fs/boot.h
        ${CMAKE_SOURCE_DIR}/app fs/boot.c

        ${CMAKE_SOURCE_DIR}/app fs/utils.h
        ${CMAKE_SOURCE_DIR}/app fs/utils.c

        ${CMAKE_SOURCE_DIR}/app fs/attrdef.h
        ${CMAKE_SOURCE_DIR}/app fs/attrdef.c
)

include(proto/proto.cmake)
include(${CMAKE_SOURCE_DIR}/3thrd/clib/c/c.cmake)
include(${CMAKE_SOURCE_DIR}/3thrd/fs/libsandbox-fs.cmake)

add_executable(sandbox main.c ${SANDBOX_SRC} ${PROTO_SRC_C} ${C_SRC} ${SANDBOX_FS_SRC})

target_link_libraries(sandbox PUBLIC -lpthread -ldl
        ${GLIB_LIBRARIES}
        ${E2FS_LIBRARIES}
        ${CLIB_LIBRARIES}
        ${DISK_LIBRARIES}
        ${FUSE_LIBRARIES}
        ${MOUNT_LIBRARIES}
)

target_include_directories(sandbox PUBLIC
        ${GLIB_INCLUDE_DIRS}
        ${E2FS_INCLUDE_DIRS}
        ${CLIB_INCLUDE_DIRS}
        ${DISK_INCLUDE_DIRS}
        ${FUSE_INCLUDE_DIRS}
        ${MOUNT_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3thrd/clib
)

target_compile_definitions(sandbox PUBLIC
        -D_GNU_SOURCE
        -DHAVE_CONFIG_H
        -D__CLIB_H_INSIDE__
        -D_FILE_OFFSET_BITS=64
        -DPACKAGE_NAME=\"sandbox\"
        -DTERMINATOR=\"sandbox-mate-terminal\"
        -DFILE_MANAGER=\"sandbox-nemo\"
)

install(TARGETS sandbox DESTINATION /usr/local/andsec/sandbox/bin)
