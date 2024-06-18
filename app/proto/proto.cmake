FILE(GLOB PROTO_FILES
        ${CMAKE_SOURCE_DIR}/app/proto/extend-field.proto
        ${CMAKE_SOURCE_DIR}/app/proto/command-line.proto
)

FILE(GLOB PROTO_SRC_CXX
        ${CMAKE_SOURCE_DIR}/app/proto/extend-field.pb.h
        ${CMAKE_SOURCE_DIR}/app/proto/extend-field.pb.cc

        ${CMAKE_SOURCE_DIR}/app/proto/command-line.pb.h
        ${CMAKE_SOURCE_DIR}/app/proto/command-line.pb.cc
)

FILE(GLOB PROTO_SRC_C
        ${CMAKE_SOURCE_DIR}/app/proto/extend-field.pb-c.h
        ${CMAKE_SOURCE_DIR}/app/proto/extend-field.pb-c.c

        ${CMAKE_SOURCE_DIR}/app/proto/command-line.pb-c.h
        ${CMAKE_SOURCE_DIR}/app/proto/command-line.pb-c.c
)

FILE(GLOB PROTO_SRC ${PROTO_SRC_C})

execute_process(COMMAND protoc-c --c_out=${CMAKE_SOURCE_DIR}/app/proto -I${CMAKE_SOURCE_DIR}/app/proto/ ${PROTO_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/app/proto/)