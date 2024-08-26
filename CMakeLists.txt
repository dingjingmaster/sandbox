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

pkg_check_modules(X REQUIRED x11)
#pkg_check_modules(ATK REQUIRED atk)
#pkg_check_modules(XAPP REQUIRED xapp)
pkg_check_modules(E2FS REQUIRED ext2fs)
#pkg_check_modules(FUSE REQUIRED fuse3)
#pkg_check_modules(DCONF REQUIRED dconf)
pkg_check_modules(MOUNT REQUIRED mount)
pkg_check_modules(GDK REQUIRED gdk-3.0)
pkg_check_modules(GTK REQUIRED gtk+-3.0)
pkg_check_modules(DISK REQUIRED udisks2)
pkg_check_modules(GLIB REQUIRED gio-2.0)
pkg_check_modules(VTE REQUIRED vte-2.91)
pkg_check_modules(GAIL REQUIRED gail-3.0)
#pkg_check_modules(XML REQUIRED libxml-2.0)
#pkg_check_modules(GMODULE REQUIRED gmodule-2.0)
pkg_check_modules(PROTO REQUIRED libprotobuf-c)
pkg_check_modules(CLIB REQUIRED clibrary-c-static)
#pkg_check_modules(PKEXEC REQUIRED polkit-gobject-1)
#pkg_check_modules(TRACKER REQUIRED tracker-sparql-3.0)
#pkg_check_modules(CINNAMON_DESKTOP REQUIRED cinnamon-desktop)

add_subdirectory(app)
add_subdirectory(data)
add_subdirectory(3thrd)
