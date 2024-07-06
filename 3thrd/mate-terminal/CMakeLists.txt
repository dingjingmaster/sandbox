file(GLOB SANDBOX_MATE_TERMINAL_SRC
        eggshell.c
        terminal-app.c
        terminal-info-bar.c
        terminal-search-dialog.c
        terminal.c
        terminal-options.c
        terminal-tab-label.c
        profile-editor.c
        terminal-close-button.c
        terminal-profile.c
        terminal-tabs-menu.c
        skey-popup.c
        terminal-debug.c
        terminal-screen.c
        terminal-util.c
        terminal-accels.c
        terminal-encoding.c
        terminal-screen-container.c
        terminal-window.c

        skey/skey.c
        skey/skeyutil.c
        skey/md4.c
        skey/md5.c
        skey/sha1.c
        skey/btoe.c


        terminal-marshal.h  terminal-marshal.c
        terminal-resources.h terminal-resources.c
        terminal-type-builtins.h terminal-type-builtins.c
)


add_executable(sandbox-mate-terminal terminal.c ${SANDBOX_MATE_TERMINAL_SRC})
target_link_libraries(sandbox-mate-terminal PUBLIC
        ${X_LIBRARIES}
        ${ATK_LIBRARIES}
        ${VTE_LIBRARIES}
        ${GDK_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${DCONF_LIBRARIES}
)
target_include_directories(sandbox-mate-terminal PUBLIC
        ${X_INCLUDE_DIRS}
        ${ATK_INCLUDE_DIRS}
        ${VTE_INCLUDE_DIRS}
        ${GDK_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${DCONF_INCLUDE_DIRS}
)
target_compile_definitions(sandbox-mate-terminal PUBLIC
        -DTERM_LOCALEDIR=\"\"
        -DGETTEXT_PACKAGE=\"\"
        -DTERMINAL_RESOURCES_PATH_PREFIX=\"/org/mate/terminal\"
        -DSN_API_NOT_YET_FROZEN
        -DGDK_MULTIHEAD_SAFE
        -DG_DISABLE_SINGLE_INCLUDES
        -DPANGO_DISABLE_SINGLE_INCLUDES
        -DATK_DISABLE_SINGLE_INCLUDES
        -DGDK_DISABLE_SINGLE_INCLUDES
        -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES
        -DGTK_DISABLE_SINGLE_INCLUDES
        -DTERMINAL_COMPILATION
)