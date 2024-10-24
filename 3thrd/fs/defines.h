//
// Created by dingjing on 10/23/24.
//

#ifndef sandbox_DEFINES_H
#define sandbox_DEFINES_H

#define SANDBOX_OEM                                 "NTFS    "
// #define SANDBOX_MAGIC                               0x5346544e
// #define SANDBOX_OEM_S64                             0x202020205346544eUL
#define SANDBOX_VERSION                             1

// Boot
#define SANDBOX_EFS_HEADER_SIZE                     10240               // 尾部 10KB

#define SANDBOX_MSG_OFFSET                          3
#define SANDBOX_ORDER_OFFSET                        14
#define SANDBOX_EFS_HEADER_OFFSET                   18  // 18 + 256 -- remain 238

#endif // sandbox_DEFINES_H
