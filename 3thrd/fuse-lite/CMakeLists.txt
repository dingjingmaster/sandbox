include(libfuse-lite.cmake)

add_library(libfuse-lite STATIC ${FUSE_LITE_SRC})
target_include_directories(libfuse-lite PUBLIC
        ${CMAKE_SOURCE_DIR}/3thrd/
        ${CMAKE_SOURCE_DIR}/3thrd/fuse-lite
)
target_compile_definitions(libfuse-lite PRIVATE
        -DFUSE_USE_VERSION=26
        -DHAVE_CONFIG_H
)
set_target_properties(libfuse-lite PROPERTIES OUTPUT_NAME "libfuse-lite")
