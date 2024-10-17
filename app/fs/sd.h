//
// Created by dingjing on 10/16/24.
//

#ifndef sandbox_SD_H
#define sandbox_SD_H

#include "../../3thrd/fs/types.h"

void init_system_file_sd(int sys_file_no, u8 **sd_val, int *sd_val_len);
void init_root_sd(u8 **sd_val, int *sd_val_len);
void init_secure_sds(char *sd_val);

#endif // sandbox_SD_H
