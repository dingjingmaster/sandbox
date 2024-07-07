file(GLOB SANDBOX_NEMO_SRC
        nemo-action-config-widget.c
        nemo-main-application.c
        nemo-application.c
        nemo-main.c
        nemo-cinnamon-dbus.c  nemo-cinnamon-dbus.h
        nemo-freedesktop-generated.c  nemo-freedesktop-generated.h
        #        nemo-autorun-software.c
        nemo-mime-actions.c
        nemo-blank-desktop-window.c
        nemo-navigation-action.c
        nemo-bookmark-list.c
        nemo-navigation-state.c
        nemo-bookmarks-window.c
        nemo-notebook.c
        nemo-config-base-widget.c
#        nemo-open-with-main.c
        nemo-connect-server-dialog.c
        nemo-pathbar.c
#        nemo-connect-server-dialog-main.c
        nemo-places-sidebar.c
        nemo-connect-server-dialog-nonmain.c
        nemo-plugin-manager.c
        nemo-connect-server-operation.c
        nemo-previewer.c
        nemo-desktop-application.c
        nemo-progress-info-widget.c
        nemo-desktop-icon-grid-view.c
        nemo-progress-ui-handler.c
        nemo-desktop-icon-view.c
        nemo-properties-window.c
        nemo-desktop-item-properties.c
        nemo-query-editor.c
#        nemo-desktop-main.c
        nemo-script-config-widget.c
        nemo-desktop-manager.c
        nemo-self-check-functions.c
        nemo-desktop-overlay.c
        nemo-statusbar.c
        nemo-desktop-window.c
        nemo-thumbnail-problem-bar.c
#        nemo-empty-view.c
        nemo-toolbar.c
        nemo-error-reporting.c
        nemo-trash-bar.c
        nemo-extension-config-widget.c
        nemo-tree-sidebar.c
#        nemo-extensions-list.c
        nemo-tree-sidebar-model.c
        nemo-file-management-properties.c
        nemo-view.c
        nemo-floating-bar.c
        nemo-view-dnd.c
        nemo-freedesktop-dbus.c
        nemo-view-factory.c
        nemo-icon-view.c
        nemo-window-bookmarks.c
        nemo-icon-view-container.c
        nemo-window.c
        nemo-icon-view-grid-container.c
        nemo-window-manage-views.c
        nemo-image-properties-page.c
        nemo-window-menus.c
        nemo-interesting-folder-bar.c
        nemo-window-pane.c
        nemo-list-model.c
        nemo-window-slot.c
        nemo-list-view.c
        nemo-window-slot-dnd.c
        nemo-location-bar.c
        nemo-x-content-bar.c
        nemo-location-entry.c

#        eel/check-program.c
        eel/eel-debug.c
        eel/eel-gnome-extensions.c
        eel/eel-self-checks.c
        eel/eel-accessibility.c
        eel/eel-editable-label.c
        eel/eel-graphic-effects.c
        eel/eel-stock-dialogs.c
        eel/eel-art-extensions.c
        eel/eel-gdk-extensions.c
        eel/eel-gtk-extensions.c
        eel/eel-string.c
        eel/eel-canvas.c
        eel/eel-glib-extensions.c
        eel/eel-lib-self-check-functions.c
        eel/eel-vfs-extensions.c

        libegg/eggtreemultidnd.c  libegg/eggtreemultidnd.h

        libnemo-extension/nemo-column.c
        libnemo-extension/nemo-location-widget-provider.c
        libnemo-extension/nemo-property-page.c
        libnemo-extension/nemo-column-provider.c
        libnemo-extension/nemo-menu.c
        libnemo-extension/nemo-property-page-provider.c
        libnemo-extension/nemo-desktop-preferences.c
        libnemo-extension/nemo-menu-item.c
        libnemo-extension/nemo-simple-button.c
        libnemo-extension/nemo-file-info.c
        libnemo-extension/nemo-menu-provider.c
        libnemo-extension/nemo-info-provider.c
        libnemo-extension/nemo-name-and-desc-provider.c

        libnemo-private/nemo-action.c
        libnemo-private/nemo-generated.c
        libnemo-private/nemo-file-changes-queue.c
        libnemo-private/nemo-progress-info.c
        libnemo-private/nemo-action-manager.c
        libnemo-private/nemo-file-conflict-dialog.c
        libnemo-private/nemo-progress-info-manager.c
        libnemo-private/nemo-bookmark.c
        libnemo-private/nemo-file-dnd.c
        libnemo-private/nemo-query.c
        libnemo-private/nemo-cell-renderer-disk.c
        libnemo-private/nemo-file-operations.c
        libnemo-private/nemo-recent.c
        libnemo-private/nemo-centered-placement-grid.c
        libnemo-private/nemo-file-queue.c
        libnemo-private/nemo-search-directory.c
        libnemo-private/nemo-clipboard.c
        libnemo-private/nemo-file-undo-manager.c
        libnemo-private/nemo-search-directory-file.c
        libnemo-private/nemo-clipboard-monitor.c
        libnemo-private/nemo-file-undo-operations.c
        libnemo-private/nemo-search-engine-advanced.c
        libnemo-private/nemo-column-chooser.c
        libnemo-private/nemo-file-utilities.c
        libnemo-private/nemo-search-engine.c
        libnemo-private/nemo-column-utilities.c
        libnemo-private/nemo-global-preferences.c
        libnemo-private/nemo-search-engine-tracker.c
        libnemo-private/nemo-dbus-manager.c
        libnemo-private/nemo-icon-canvas-item.c
        libnemo-private/nemo-selection-canvas-item.c
        libnemo-private/nemo-debug.c
        libnemo-private/nemo-icon-container.c
        libnemo-private/nemo-separator-action.c
        libnemo-private/nemo-default-file-icon.c
        libnemo-private/nemo-icon-dnd.c
        libnemo-private/nemo-signaller.c
        libnemo-private/nemo-desktop-directory.c
        libnemo-private/nemo-icon-info.c
        libnemo-private/nemo-thumbnails.c
        libnemo-private/nemo-desktop-directory-file.c
        libnemo-private/nemo-job-queue.c
        libnemo-private/nemo-trash-monitor.c
        libnemo-private/nemo-desktop-icon-file.c
        libnemo-private/nemo-lib-self-check-functions.c
        libnemo-private/nemo-tree-view-drag-dest.c
        libnemo-private/nemo-desktop-link.c
        libnemo-private/nemo-link.c
        libnemo-private/nemo-ui-utilities.c
        libnemo-private/nemo-desktop-link-monitor.c
        libnemo-private/nemo-merged-directory.c
        libnemo-private/nemo-undo.c
        libnemo-private/nemo-desktop-metadata.c
        libnemo-private/nemo-metadata.c
        libnemo-private/nemo-undo-manager.c
        libnemo-private/nemo-desktop-utils.c
        libnemo-private/nemo-mime-application-chooser.c
        libnemo-private/nemo-undo-signal-handlers.c
        libnemo-private/nemo-directory-async.c
        libnemo-private/nemo-module.c
        libnemo-private/nemo-undo-transaction.c
        libnemo-private/nemo-directory.c
        libnemo-private/nemo-monitor.c
        libnemo-private/nemo-vfs-directory.c
        libnemo-private/nemo-dnd.c
        libnemo-private/nemo-placement-grid.c
        libnemo-private/nemo-vfs-file.c
        libnemo-private/nemo-entry.c
        libnemo-private/nemo-places-tree-view.c
        libnemo-private/nemo-widget-action.c
        libnemo-private/nemo-file.c
        libnemo-private/nemo-program-choosing.c
        libnemo-private/nemo-widget-menu-item.c
)


add_executable(sandbox-nemo nemo-main.c ${SANDBOX_NEMO_SRC})
target_link_libraries(sandbox-nemo PUBLIC -lm
        ${X_LIBRARIES}
        ${XML_LIBRARIES}
        ${ATK_LIBRARIES}
        ${VTE_LIBRARIES}
        ${GDK_LIBRARIES}
        ${XAPP_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${GAIL_LIBRARIES}
        ${DCONF_LIBRARIES}
        ${TRACKER_LIBRARIES}
        ${GMODULE_LIBRARIES}
        ${CINNAMON_DESKTOP_LIBRARIES}
)
target_include_directories(sandbox-nemo PUBLIC
        ${X_INCLUDE_DIRS}
        ${XML_INCLUDE_DIRS}
        ${ATK_INCLUDE_DIRS}
        ${VTE_INCLUDE_DIRS}
        ${GDK_INCLUDE_DIRS}
        ${XAPP_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${GAIL_INCLUDE_DIRS}
        ${DCONF_INCLUDE_DIRS}
        ${TRACKER_INCLUDE_DIRS}
        ${GMODULE_INCLUDE_DIRS}
        ${CINNAMON_DESKTOP_INCLUDE_DIRS}
)
target_compile_definitions(sandbox-nemo PUBLIC
        -DLIBEXECDIR=\"/usr/local/libexec\"
        -DNEMO_DATADIR=\"/usr/local/share/nemo\"
        -DNEMO_EXTENSIONDIR=\"/usr/local/lib/nemo/extensions-3.0\"
)
