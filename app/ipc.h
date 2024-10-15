//
// Created by dingjing on 10/15/24.
//

#ifndef sandbox_IPC_H
#define sandbox_IPC_H

typedef enum
{
    IPC_TYPE_NONE = 0,
    IPC_TYPE_OPEN_FM,
    IPC_TYPE_OPEN_TERMINATOR,
    IPC_TYPE_QUIT,
} IpcType;


typedef struct __attribute__((packed)) _IpcMessage
{
    unsigned int        type;                       // 处理类型：IpcServerType、IpcClientType
    unsigned long       dataLen;
    char                data[];
} IpcMessage;

#endif // sandbox_IPC_H
