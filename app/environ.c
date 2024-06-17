//
// Created by dingjing on 24-6-17.
//

#include "environ.h"

static cchar** gsEnviron = NULL;

void environ_init(void)
{
    int i = 0;
    gsEnviron = c_get_environ();
    c_assert(NULL != gsEnviron);
    C_LOG_INFO("environ value:");
    for (i = 0; gsEnviron[i]; ++i) {
        C_LOG_WRITE_FILE(C_LOG_LEVEL_INFO, gsEnviron[i]);
    }
}
