//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_COMPAT_H
#define sandbox_COMPAT_H
#include <c/clib.h>

#include <errno.h>      /* ENODATA */

#include "types.h"

C_BEGIN_EXTERN_C


#ifndef ENODATA
#define ENODATA ENOENT
#endif

#ifndef ELIBBAD
#define ELIBBAD ENOEXEC
#endif

#ifndef ELIBACC
#define ELIBACC ENOENT
#endif

/* xattr APIs in macOS differs from Linux ones in that they expect the special
 * error code ENOATTR to be returned when an attribute cannot be found. So
 * define NTFS_NOXATTR_ERRNO to the appropriate "no xattr found" errno value for
 * the platform. */
#define FS_NOXATTR_ERRNO ENODATA

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef HAVE_FFS
extern int ffs(int i);
#endif /* HAVE_FFS */

#ifndef HAVE_DAEMON
extern int daemon(int nochdir, int noclose);
#endif /* HAVE_DAEMON */

#ifndef HAVE_STRSEP
extern char *strsep(char **stringp, const char *delim);
#endif /* HAVE_STRSEP */

#ifndef O_BINARY
#define O_BINARY                0               /* unix is binary by default */
#endif

C_END_EXTERN_C

#endif // sandbox_COMPAT_H
