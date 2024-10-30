include(../app/vfs/vfs.cmake)
include(../3thrd/clib/c/c.cmake)

qt5_add_resources(RES ../data/icons.qrc)

add_executable(copy-files copy-files.cpp ${C_SRC} ${VFS_SRC}
        ../app/utils.c ../app/utils.h
        ../3thrd/clib/c/glog.c ../3thrd/clib/c/glog.h
        singleton-app-gui.cpp singleton-app-gui.h
        main-window.cc main-window.h
        sandbox-model.cc sandbox-model.h
        sandbox-view.cc sandbox-view.h
        header-view.cc header-view.h
        ${RES}
)
target_include_directories(copy-files PUBLIC ${QT_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS} ../3thrd/clib)
target_link_libraries(copy-files PUBLIC ${QT_LIBRARIES} ${GLIB_LIBRARIES})
target_compile_definitions(copy-files PUBLIC -D __CLIB_H_INSIDE__
        -DPACKAGE_NAME=\"copy-files\")
target_compile_options(copy-files PUBLIC -fPIC)