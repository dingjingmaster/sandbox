//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_XATTRS_H
#define sandbox_XATTRS_H
#include <c/clib.h>
#if defined(HAVE_SETXATTR) || defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#else
#include "compat.h" /* may be needed for ENODATA definition */
#endif

#include "types.h"

C_BEGIN_EXTERN_C

#define XATTR_CREATE    1
#define XATTR_REPLACE   2

/**
 * Identification of data mapped to the system name space
 */

enum SYSTEMXATTRS
{
    XATTR_UNMAPPED,
    XATTR_NTFS_ACL,
    XATTR_NTFS_ATTRIB,
    XATTR_NTFS_ATTRIB_BE,
    XATTR_NTFS_EFSINFO,
    XATTR_NTFS_REPARSE_DATA,
    XATTR_NTFS_OBJECT_ID,
    XATTR_NTFS_DOS_NAME,
    XATTR_NTFS_TIMES,
    XATTR_NTFS_TIMES_BE,
    XATTR_NTFS_CRTIME,
    XATTR_NTFS_CRTIME_BE,
    XATTR_NTFS_EA,
    XATTR_POSIX_ACC,
    XATTR_POSIX_DEF
};

struct XATTRMAPPING
{
    struct XATTRMAPPING * next;
    enum SYSTEMXATTRS xattr;
    char name[1]; /* variable length */
};

#ifdef XATTR_MAPPINGS

struct XATTRMAPPING *fs_xattr_build_mapping(FSVolume *vol, const char *path);
void fs_xattr_free_mapping(struct XATTRMAPPING*);

#endif /* XATTR_MAPPINGS */

enum SYSTEMXATTRS fs_xattr_system_type(const char * name, FSVolume * vol);

struct SECURITY_CONTEXT;

int fs_xattr_system_getxattr(struct SECURITY_CONTEXT * scx, enum SYSTEMXATTRS attr, FSInode * ni, FSInode * dir_ni, char * value, size_t size);
int fs_xattr_system_setxattr(struct SECURITY_CONTEXT * scx, enum SYSTEMXATTRS attr, FSInode * ni, FSInode * dir_ni, const char * value, size_t size, int flags);
int fs_xattr_system_removexattr(struct SECURITY_CONTEXT * scx, enum SYSTEMXATTRS attr, FSInode * ni, FSInode * dir_ni);

C_END_EXTERN_C

#endif // sandbox_XATTRS_H
