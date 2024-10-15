include(c.cmake)

add_library(clibrary-c SHARED ${C_SRC})
target_include_directories(clibrary-c PUBLIC ${CMAKE_SOURCE_DIR})
set_target_properties(clibrary-c PROPERTIES VERSION "${PROJECT_VERSION}")

add_library(clibrary-c-static STATIC ${C_SRC})
target_include_directories(clibrary-c-static PUBLIC ${CMAKE_SOURCE_DIR})
set_target_properties(clibrary-c-static PROPERTIES OUTPUT_NAME "clibrary-c")
set_target_properties(clibrary-c-static PROPERTIES VERSION "${PROJECT_VERSION}")

install(TARGETS clibrary-c DESTINATION /usr/lib)
install(TARGETS clibrary-c-static DESTINATION /usr/lib)
install(FILES ${C_HEADERS} DESTINATION /usr/include/clibrary/c/)
