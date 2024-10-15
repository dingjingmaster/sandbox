
file(GLOB SANDBOX_SRC
        ${CMAKE_SOURCE_DIR}/app loop.h
        ${CMAKE_SOURCE_DIR}/app loop.c

        ${CMAKE_SOURCE_DIR}/app polkit.h
        ${CMAKE_SOURCE_DIR}/app polkit.c

        ${CMAKE_SOURCE_DIR}/app environ.h
        ${CMAKE_SOURCE_DIR}/app environ.c

        ${CMAKE_SOURCE_DIR}/app sandbox.h
        ${CMAKE_SOURCE_DIR}/app sandbox.c

        ${CMAKE_SOURCE_DIR}/app namespace.h
        ${CMAKE_SOURCE_DIR}/app namespace.c

        ${CMAKE_SOURCE_DIR}/app filesystem.h
        ${CMAKE_SOURCE_DIR}/app filesystem.c

#        ${CMAKE_SOURCE_DIR}/app/fs fs.h
#        ${CMAKE_SOURCE_DIR}/app/fs fs.c

#        ${CMAKE_SOURCE_DIR}/app/fs fs/device.h
#        ${CMAKE_SOURCE_DIR}/app/fs fs/device.c
#
#        ${CMAKE_SOURCE_DIR}/app/fs fs/volume.h
#        ${CMAKE_SOURCE_DIR}/app/fs fs/volume.c
)

include(proto/proto.cmake)
include(${CMAKE_SOURCE_DIR}/3thrd/clib/c/c.cmake)

add_executable(sandbox main.c ${SANDBOX_SRC} ${PROTO_SRC_C} ${C_SRC})

target_link_libraries(sandbox PUBLIC -lpthread
#        ${FUSE_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${E2FS_LIBRARIES}
        ${CLIB_LIBRARIES}
        ${DISK_LIBRARIES}
        ${MOUNT_LIBRARIES}
        ${PROTO_LIBRARIES}
)

target_include_directories(sandbox PUBLIC
#        ${FUSE_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${E2FS_INCLUDE_DIRS}
        ${CLIB_INCLUDE_DIRS}
        ${DISK_INCLUDE_DIRS}
        ${MOUNT_INCLUDE_DIRS}
        ${PROTO_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/3thrd/clib
)

target_compile_definitions(sandbox PUBLIC
        -D_GNU_SOURCE
        -D__CLIB_H_INSIDE__
        -DPACKAGE_NAME=\"sandbox\"
        -DTERMINATOR=\"sandbox-mate-terminal\"
        -DFILE_MANAGER=\"sandbox-nemo\"
)

install(TARGETS sandbox DESTINATION /usr/bin)
