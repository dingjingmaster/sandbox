EXECUTE_PROCESS(COMMAND glib-compile-resources --target=nemo-resource.h --generate-header ./nemo.gresource.xml WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/data/nemo-gresources)
EXECUTE_PROCESS(COMMAND glib-compile-resources --target=nemo-resource.c --generate-source ./nemo.gresource.xml WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/data/nemo-gresources)

install(FILES sandbox.service DESTINATION /etc/systemd/system/)

install(FILES sandbox-terminator.desktop DESTINATION /usr/share/applications/)
install(FILES sandbox-file-manager.desktop DESTINATION /usr/share/applications/)

install(FILES org.nemo.gschema.xml DESTINATION /usr/share/glib-2.0/schemas/)
install(FILES org.mate.terminal.gschema.xml DESTINATION /usr/share/glib-2.0/schemas/)
install(FILES org.mate.interface.gschema.xml DESTINATION /usr/share/glib-2.0/schemas/)

