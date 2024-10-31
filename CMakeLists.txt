cmake_minimum_required(VERSION 3.10)
project(sandbox C CXX)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(DEBUG true)
#set(DEBUG false)
set(CMAKE_C_STANDARD 11)

add_definitions(
        -w -g
        -D QT_NO_KEYWORDS
        -D FUSE_USE_VERSION=31
        -D QT_MESSAGELOGCONTEXT
        -D C_LOG_TAG=\\"sandbox\\"
        -DINSTALL_DIR=\"/usr/local/andsec/\"
        -D SANDBOX_MOUNT_POINT=\\"/usr/local/andsec/sandbox/.sandbox\\"
        -D PACKAGE_VERSION=\\"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}\\"
)

if (DEBUG)
    add_definitions(-D DEBUG)
endif ()

find_package (PkgConfig)
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

pkg_check_modules(X REQUIRED x11)
pkg_check_modules(FUSE REQUIRED fuse)
pkg_check_modules(GDK REQUIRED gdk-3.0)
pkg_check_modules(GTK REQUIRED gtk+-3.0)
pkg_check_modules(GLIB REQUIRED gio-2.0)
pkg_check_modules(VTE REQUIRED vte-2.91)
pkg_check_modules(GAIL REQUIRED gail-3.0)
pkg_check_modules(QT REQUIRED Qt5Core Qt5Widgets Qt5Gui Qt5Network)

add_subdirectory(app)
add_subdirectory(data)
add_subdirectory(test)
add_subdirectory(3thrd)
add_subdirectory(tools)
