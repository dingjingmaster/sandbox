//
// Created by dingjing on 10/15/24.
//

#include "ipc-message.h"

#include <glib.h>

struct _IpcMessageData
{
    IpcType         ipcType;
    GList*          ipcData;
};

IpcMessageData* ipc_message_data_new(void)
{
    IpcMessageData* data = g_malloc0 (sizeof(IpcMessageData));
    if (NULL == data) {
        return NULL;
    }

    data->ipcData = NULL;

    return data;
}

void ipc_message_data_free(IpcMessageData** data)
{
    g_return_if_fail (data != NULL || *data == NULL);

    if ((*data)->ipcData) {
        g_list_free_full((*data)->ipcData, g_free);
    }

    if (*data) {
        g_free(*data);
        *data = NULL;
    }
}

void ipc_message_set_type(IpcMessageData* data, IpcType ipcType)
{
    g_return_if_fail(data != NULL);

    data->ipcType = ipcType;
}

void ipc_message_append_kv(IpcMessageData* data, const char* key, const char* value)
{
    g_return_if_fail(data != NULL && key != NULL && value != NULL);

    gchar* kv = g_strdup_printf("%s=%s", key, value);

    data->ipcData = g_list_append(data->ipcData, kv);
}

gsize ipc_message_pack(IpcMessageData* data, char** outBuf)
{
    g_return_val_if_fail(data != NULL && outBuf, 0);

    int curSize = 0;
    gsize size = sizeof(IpcMessage);

    GList* node = NULL;
    for (node = data->ipcData; node; node = node->next) {
        size += strlen(node->data);
        size += 2;
    }

    char* buf = g_malloc0(size);
    if (NULL == buf) {
        return 0;
    }

    memset(buf, 0, size);

    IpcMessage* ipcMessage = (IpcMessage*) buf;
    ipcMessage->type = data->ipcType;
    ipcMessage->dataLen = size - sizeof(IpcMessage);
    curSize += sizeof(IpcMessage);

    C_LOG_VERB("IpcMessage packed");

    for (node = data->ipcData; node; node = node->next) {
        int strLen = strlen(node->data);
        memcpy(buf + curSize, node->data, strLen);
        curSize += strLen;
        memcpy(buf + curSize, "{]", 2);
        curSize += 2;
    }

    *outBuf = buf;

    return size;
}

bool ipc_message_unpack(IpcMessageData* data, const char* buf, gsize bufSize)
{
    g_return_val_if_fail(data != NULL && buf != NULL && bufSize >= sizeof(IpcMessage), false);

    IpcMessage* ipcMessage = (IpcMessage*) buf;

    data->ipcType = ipcMessage->type;

    char* dataT = ipcMessage->data;

    char** arr = g_strsplit(dataT, "{]", -1);

    int i = 0;
    for (i = 0; arr[i]; i++) {
        data->ipcData = g_list_append(data->ipcData, g_strdup(arr[i]));
    }

    if (arr)    { g_strfreev(arr); }

    return true;
}

IpcType ipc_message_type(IpcMessageData * data)
{
    g_return_val_if_fail(data != NULL, IPC_TYPE_NONE);

    return data->ipcType;
}

const GList * ipc_message_get_env_list(IpcMessageData * data)
{
    g_return_val_if_fail(data != NULL, NULL);

    return data->ipcData;
}

