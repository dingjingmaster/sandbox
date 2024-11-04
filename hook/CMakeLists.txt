add_library(hook-connect SHARED hook-connect.c)
target_link_libraries(hook-connect PUBLIC -ldl)
set_target_properties(hook-connect PROPERTIES
        OUTPUT_NAME "hook-connect"
        PREFIX ""
        SUFFIX ".so"
)