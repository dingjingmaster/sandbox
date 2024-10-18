file(GLOB FUSE_LITE_SRC
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_i.h
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_kern_chan.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_loop.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_lowlevel.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_misc.h
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_opt.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_session.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fuse_signals.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/fusermount.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/helper.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/mount.c
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite/mount_util.c
)