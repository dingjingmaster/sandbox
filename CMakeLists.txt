cmake_minimum_required(VERSION 3.28)
project(sandbox C)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

set(DEBUG true)
#set(DEBUG false)
set(CMAKE_C_STANDARD 11)

add_definitions(
        -w -g
        -D FUSE_USE_VERSION=31
        -D C_LOG_TAG=\\"sandbox\\"
        -D PACKAGE_VERSION=\\"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}\\"
)

if (DEBUG)
    add_definitions(-D DEBUG)
endif ()

find_package (PkgConfig)

pkg_check_modules(FUSE REQUIRED fuse3)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(CLIB REQUIRED clibrary-c-static)

add_subdirectory(app)