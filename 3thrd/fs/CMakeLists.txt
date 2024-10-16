include(libsandbox-fs.cmake)

add_library(libsandbox-fs STATIC ${LIB_NTFS_3G})
target_include_directories(libsandbox-fs PUBLIC
        ${CMAKE_SOURCE_DIR}/3thrd
        ${CMAKE_SOURCE_DIR}/3thrd/fs
)
target_compile_definitions(libsandbox-fs PRIVATE
        -D HAVE_CONFIG_H
)
set_target_properties(libsandbox-fs PROPERTIES OUTPUT_NAME "libsandbox-fs")
