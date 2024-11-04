//
// Created by dingjing on 11/4/24.
//
#include "../app/cgroup.h"


int main(int argc, char* argv[])
{
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    SbCgroup* cgroup = sb_cgroup_new();

    sb_cgroup_run(cgroup);

    g_main_loop_run(loop);

    return 0;
}