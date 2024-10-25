
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-3-7.
//

#ifndef CLIBRARY_GLOG_H
#define CLIBRARY_GLOG_H
#include <glib.h>
#include "clib.h"

#ifndef C_LOG_TAG
#define C_LOG_TAG "glog"
#endif

#ifndef C_LOG_SIZE
#define C_LOG_SIZE     (200 * 2 << 10)         // 200mb
#endif

#ifndef C_LOG_DIR
#define C_LOG_DIR      "/tmp/"
#endif

#ifdef DEBUG
#define C_LOG_LEVEL    C_LOG_LEVEL_VERB
#else
#define C_LOG_LEVEL    C_LOG_LEVEL_INFO
#endif

G_BEGIN_DECLS

// 调试使用
//#undef GLIB_VERSION_2_50

#ifdef GLIB_VERSION_2_72
#define C_GLIB_ENABLE_DEBUG g_log_set_debug_enabled(true);
#else
#define C_GLIB_ENABLE_DEBUG
#endif

#ifdef GLIB_VERSION_2_50

#define C_GLOG_INIT_IF_NOT_INIT \
{ \
    if (C_UNLIKELY(!c_log_is_inited())) { \
        c_log_init (C_LOG_LEVEL, C_LOG_SIZE, C_LOG_DIR, C_LOG_TAG, "log", false); \
        C_GLIB_ENABLE_DEBUG \
        g_log_set_writer_func(c_glog_handler, NULL, NULL); \
    } \
};

#define C_GLOG_VERB(...) \
{ \
    C_GLOG_INIT_IF_NOT_INIT \
    g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
                    "FILE", __FILE__, \
                    "LINE", __LINE__, \
                    "FUNC", __FUNCTION__, \
                    "MESSAGE", __VA_ARGS__); \
}

#define C_GLOG_DEBUG(...) \
{ \
    C_GLOG_INIT_IF_NOT_INIT \
    g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
                    "FILE", __FILE__, \
                    "LINE", __LINE__, \
                    "FUNC", __FUNCTION__, \
                    "MESSAGE", __VA_ARGS__);          \
}

#define C_GLOG_INFO(...) \
{ \
    C_GLOG_INIT_IF_NOT_INIT \
    g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, \
                    "FILE", __FILE__, \
                    "LINE", __LINE__, \
                    "FUNC", __FUNCTION__, \
                    "MESSAGE", __VA_ARGS__); \
}

#define C_GLOG_WARNING(...) \
{ \
    C_GLOG_INIT_IF_NOT_INIT \
    g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, \
                    "FILE", __FILE__, \
                    "LINE", __LINE__, \
                    "FUNC", __FUNCTION__, \
                    "MESSAGE", __VA_ARGS__); \
}

#define C_GLOG_CRIT(...) \
{ \
    C_GLOG_INIT_IF_NOT_INIT \
    g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, \
                    "FILE", __FILE__, \
                    "LINE", __LINE__, \
                    "FUNC", __FUNCTION__, \
                    "MESSAGE", __VA_ARGS__); \
}

#define C_GLOG_ERROR(...) \
{ \
    C_GLOG_INIT_IF_NOT_INIT \
    g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, \
                    "FILE", __FILE__, \
                    "LINE", __LINE__, \
                    "FUNC", __FUNCTION__, \
                    "MESSAGE", __VA_ARGS__); \
}

GLogWriterOutput c_glog_handler (GLogLevelFlags level, const GLogField* fields, gsize nFields, gpointer udata);

#else

#define C_GLOG_INIT_IF_NOT_INIT \
{ \
    if (C_UNLIKELY(!c_log_is_inited())) { \
        c_log_init (C_LOG_TYPE_FILE, C_LOG_LEVEL, C_LOG_SIZE, C_LOG_DIR, C_LOG_TAG, "log", false); \
        C_GLIB_ENABLE_DEBUG \
        g_log_set_handler(NULL, G_LOG_LEVEL_MASK, c_glog_handler, NULL); \
    } \
};

#define C_GLOG_VERB(...) \
    { \
        C_GLOG_INIT_IF_NOT_INIT \
        g_debug(__VA_ARGS__); \
    }

#define C_GLOG_DEBUG(...) \
    { \
        C_GLOG_INIT_IF_NOT_INIT \
        g_debug(__VA_ARGS__); \
    }

#define C_GLOG_INFO(...) \
    { \
        C_GLOG_INIT_IF_NOT_INIT \
        g_info(__VA_ARGS__); \
    }

#define C_GLOG_WARNING(...) \
    { \
        C_GLOG_INIT_IF_NOT_INIT \
        g_warning(__VA_ARGS__); \
    }

#define C_GLOG_CRIT(...) \
    { \
        C_GLOG_INIT_IF_NOT_INIT \
        g_critical(__VA_ARGS__); \
    }

#define C_GLOG_ERROR(...) \
    { \
        C_GLOG_INIT_IF_NOT_INIT \
        g_error(__VA_ARGS__); \
    }

void c_glog_handler (const char* logDomain, GLogLevelFlags logLevel, const char* msg, gpointer udata);
#endif

G_END_DECLS

#endif //CLIBRARY_GLOG_H
