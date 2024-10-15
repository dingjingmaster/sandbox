//
// Created by dingjing on 10/15/24.
//

#ifndef sandbox_IPC_MESSAGE_H
#define sandbox_IPC_MESSAGE_H
#include "../ipc.h"

#include <glib.h>
#include <c/clib.h>

typedef struct _IpcMessageData          IpcMessageData;

IpcMessageData*     ipc_message_data_new    (void);
void                ipc_message_data_free   (IpcMessageData** data);
void                ipc_message_set_type    (IpcMessageData* data, IpcType ipcType);
void                ipc_message_append_kv   (IpcMessageData* data, const char* key, const char* value);
gsize               ipc_message_pack        (IpcMessageData* data, char** outBuf/*out, need free*/);
bool                ipc_message_unpack      (IpcMessageData* data, const char* buf, gsize bufSize);
IpcType             ipc_message_type        (IpcMessageData* data);
const GList*        ipc_message_get_env_list(IpcMessageData* data);

#endif // sandbox_IPC_MESSAGE_H
