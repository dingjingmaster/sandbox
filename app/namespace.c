//
// Created by dingjing on 24-6-18.
//

#include "namespace.h"

bool namespace_check_availed()
{
    // test user namespaces available in the kernel
    struct stat s1;
    struct stat s2;
    struct stat s3;
    if (stat("/proc/self/ns/user", &s1) == 0
        && stat("/proc/self/uid_map", &s2) == 0
        && stat("/proc/self/gid_map", &s3) == 0) {
        return true;
    }

    return false;
}

bool namespace_enter()
{
    return 0;
}
