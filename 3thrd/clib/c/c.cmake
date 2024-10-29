file(GLOB C_SRC
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/log.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/log.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/str.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/str.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/uri.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/uri.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/date.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/date.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/poll.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/poll.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/test.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/test.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/list.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/list.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/uuid.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/uuid.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hash.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hash.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hook.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hook.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/rcbox.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/rcbox.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/timer.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/timer.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/slist.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/slist.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/array.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/array.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/bytes.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/bytes.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/quark.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/quark.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/queue.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/queue.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/error.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/error.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/utils.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/utils.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/global.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/global.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/source.h
		${CMAKE_SOURCE_DIR}/3thrd/clib/c/source.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/thread.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/thread.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/atomic.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/atomic.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/base64.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/base64.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/macros.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/macros.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/option.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/option.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/wakeup.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/wakeup.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/charset.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/charset.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/convert.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/convert.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/cstring.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/cstring.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/unicode.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/unicode.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/time-zone.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/time-zone.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hash-table.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hash-table.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/file-utils.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/file-utils.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/host-utils.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/host-utils.c

        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/mapped-file.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/mapped-file.c
)

file(GLOB C_HEADERS
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/log.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/str.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/uri.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/clib.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/date.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hash.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/poll.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/test.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/list.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/uuid.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hook.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/error.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/timer.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/utils.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/array.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/bytes.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/slist.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/rcbox.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/quark.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/option.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/source.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/thread.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/atomic.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/base64.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/macros.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/wakeup.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/convert.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/cstring.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/unicode.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/charset.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/time-zone.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/host-utils.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/hash-table.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/file-utils.h
        ${CMAKE_SOURCE_DIR}/3thrd/clib/c/mapped-file.h
)
