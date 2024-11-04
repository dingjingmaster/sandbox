//
// Created by dingjing on 11/4/24.
//

#ifndef sandbox_CGROUP_H
#define sandbox_CGROUP_H
#include <glib.h>
#include <glib-object.h>
#include "../3thrd/clib/c/clib.h"

G_BEGIN_DECLS

#define SB_TYPE_CGROUP                              (sb_cgroup_get_type())
#define SB_CGROUP(obj)                              (G_TYPE_CHECK_INSTANCE_CAST((obj), SB_TYPE_CGROUP, SbCgroup))
#define SB_IS_CGROUP(obj)                           (G_TYPE_CHECK_INSTANCE_TYPE((obj), SB_TYPE_CGROUP))

typedef struct _SbCgroup                            SbCgroup;

/**
 * @brief 监控沙盒进程创建以及给沙盒创建进程的产生的流量打标记
 */

GType         sb_cgroup_get_type                    (void);
SbCgroup*     sb_cgroup_new                         (void);
bool          sb_cgroup_run                         (SbCgroup* cgroup);

G_END_DECLS

#endif // sandbox_CGROUP_H
