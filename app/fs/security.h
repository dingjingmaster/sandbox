//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_SECURITY_H
#define sandbox_SECURITY_H
#include <c/clib.h>

#include "dir.h"
#include "types.h"
#include "inode.h"
#include "layout.h"

C_BEGIN_EXTERN_C

#ifndef POSIXACLS
#define POSIXACLS 0
#endif

/**
 * item in the mapping list
 */

struct MAPPING
{
    struct MAPPING * next;
    int xid; /* linux id : uid or gid */
    SID * sid; /* Windows id : usid or gsid */
    int grcnt; /* group count (for users only) */
    gid_t * groups; /* groups which the user is member of */
};

/*
 *              Entry in the permissions cache
 *      Note : this cache is not organized as a generic cache
 */

struct CACHED_PERMISSIONS
{
    uid_t uid;
    gid_t gid;
    le32 inh_fileid;
    le32 inh_dirid;
#if POSIXACLS
        struct POSIX_SECURITY *pxdesc;
        unsigned int pxdescsize:16;
#endif
    unsigned int mode : 12;
    unsigned int valid : 1;
};

/*
 *      Entry in the permissions cache for directories with no security_id
 */

struct CACHED_PERMISSIONS_LEGACY
{
    struct CACHED_PERMISSIONS_LEGACY * next;
    struct CACHED_PERMISSIONS_LEGACY * previous;
    void * variable;
    size_t varsize;
    union ALIGNMENT payload[0];
    /* above fields must match "struct CACHED_GENERIC" */
    u64 mft_no;
    struct CACHED_PERMISSIONS perm;
};

/*
 *      Entry in the securid cache
 */

struct CACHED_SECURID
{
    struct CACHED_SECURID * next;
    struct CACHED_SECURID * previous;
    void * variable;
    size_t varsize;
    union ALIGNMENT payload[0];
    /* above fields must match "struct CACHED_GENERIC" */
    uid_t uid;
    gid_t gid;
    unsigned int dmode;
    le32 securid;
};

/*
 *      Header of the security cache
 *      (has no cache structure by itself)
 */

struct CACHED_PERMISSIONS_HEADER
{
    unsigned int last;
    /* statistics for permissions */
    unsigned long p_writes;
    unsigned long p_reads;
    unsigned long p_hits;
};

/*
 *      The whole permissions cache
 */

struct PERMISSIONS_CACHE
{
    struct CACHED_PERMISSIONS_HEADER head;
    struct CACHED_PERMISSIONS * cachetable[1]; /* array of variable size */
};

/*
 *      Security flags values
 */

enum
{
    SECURITY_DEFAULT, /* rely on fuse for permissions checking */
    SECURITY_RAW, /* force same ownership/permissions on files */
    SECURITY_ACL, /* enable Posix ACLs (when compiled in) */
    SECURITY_ADDSECURIDS, /* upgrade old security descriptors */
    SECURITY_STATICGRPS, /* use static groups for access control */
    SECURITY_WANTED /* a security related option was present */
};

/*
 *      Security context, needed by most security functions
 */

enum { MAPUSERS, MAPGROUPS, MAPCOUNT };

struct SECURITY_CONTEXT
{
    FSVolume * vol;
    struct MAPPING * mapping[MAPCOUNT];
    struct PERMISSIONS_CACHE ** pseccache;
    uid_t uid; /* uid of user requesting (not the mounter) */
    gid_t gid; /* gid of user requesting (not the mounter) */
    pid_t tid; /* thread id of thread requesting */
    mode_t umask; /* umask of requesting thread */
};

#if POSIXACLS

/*
 *                     Posix ACL structures
 */

struct POSIX_ACE {
        u16 tag;
        u16 perms;
        s32 id;
} __attribute__((__packed__));

struct POSIX_ACL {
        u8 version;
        u8 flags;
        u16 filler;
        struct POSIX_ACE ace[0];
} __attribute__((__packed__));

struct POSIX_SECURITY {
        mode_t mode;
        int acccnt;
        int defcnt;
        int firstdef;
        u16 tagsset;
        u16 filler;
        struct POSIX_ACL acl;
} ;

/*
 *              Posix tags, cpu-endian 16 bits
 */

enum {
        POSIX_ACL_USER_OBJ =    1,
        POSIX_ACL_USER =        2,
        POSIX_ACL_GROUP_OBJ =   4,
        POSIX_ACL_GROUP =       8,
        POSIX_ACL_MASK =        16,
        POSIX_ACL_OTHER =       32,
        POSIX_ACL_SPECIAL =     64  /* internal use only */
} ;

#define POSIX_ACL_EXTENSIONS (POSIX_ACL_USER | POSIX_ACL_GROUP | POSIX_ACL_MASK)

/*
 *              Posix permissions, cpu-endian 16 bits
 */

enum {
        POSIX_PERM_X =          1,
        POSIX_PERM_W =          2,
        POSIX_PERM_R =          4,
        POSIX_PERM_DENIAL =     64 /* internal use only */
} ;

#define POSIX_VERSION 2

#endif

extern bool fs_guid_is_zero(const GUID * guid);
extern char* fs_guid_to_mbs(const GUID * guid, char * guid_str);

extern int fs_sid_to_mbs_size(const SID * sid);
extern char* fs_sid_to_mbs(const SID * sid, char * sid_str, size_t sid_str_size);
extern void fs_generate_guid(GUID * guid);
extern int fs_sd_add_everyone(FSInode * ni);

extern le32 fs_security_hash(const SECURITY_DESCRIPTOR_RELATIVE * sd, const u32 len);

int fs_build_mapping(struct SECURITY_CONTEXT * scx, const char * usermap_path, bool allowdef);
int fs_get_owner_mode(struct SECURITY_CONTEXT * scx, FSInode * ni, struct stat *);
int fs_set_mode(struct SECURITY_CONTEXT * scx, FSInode * ni, mode_t mode);
bool fs_allowed_as_owner(struct SECURITY_CONTEXT * scx, FSInode * ni);
int fs_allowed_access(struct SECURITY_CONTEXT * scx, FSInode * ni, int accesstype);
int fs_allowed_create(struct SECURITY_CONTEXT * scx, FSInode * ni, gid_t * pgid, mode_t * pdsetgid);
bool old_fs_allowed_dir_access(struct SECURITY_CONTEXT * scx, const char * path, int accesstype);

#if POSIXACLS
le32 ntfs_alloc_securid(struct SECURITY_CONTEXT *scx,
                uid_t uid, gid_t gid, ntfs_inode *dir_ni,
                mode_t mode, BOOL isdir);
#else
le32 fs_alloc_securid(struct SECURITY_CONTEXT * scx, uid_t uid, gid_t gid, mode_t mode, bool isdir);
#endif
int fs_set_owner(struct SECURITY_CONTEXT * scx, FSInode * ni, uid_t uid, gid_t gid);
int fs_set_ownmod(struct SECURITY_CONTEXT * scx, FSInode * ni, uid_t uid, gid_t gid, mode_t mode);
#if POSIXACLS
int fs_set_owner_mode(struct SECURITY_CONTEXT *scx, ntfs_inode *ni, uid_t uid, gid_t gid, mode_t mode, struct POSIX_SECURITY *pxdesc);
#else
int fs_set_owner_mode(struct SECURITY_CONTEXT * scx, FSInode * ni, uid_t uid, gid_t gid, mode_t mode);
#endif
le32 fs_inherited_id(struct SECURITY_CONTEXT * scx, FSInode * dir_ni, bool fordir);
int fs_open_secure(FSVolume * vol);
int fs_close_secure(FSVolume * vol);
void fs_destroy_security_context(struct SECURITY_CONTEXT * scx);

#if POSIXACLS

int ntfs_set_inherited_posix(struct SECURITY_CONTEXT *scx,
                ntfs_inode *ni, uid_t uid, gid_t gid,
                ntfs_inode *dir_ni, mode_t mode);
int ntfs_get_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
                        const char *name, char *value, size_t size);
int ntfs_set_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
                        const char *name, const char *value, size_t size,
                        int flags);
int ntfs_remove_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
                        const char *name);
#endif

int fs_get_ntfs_acl(struct SECURITY_CONTEXT * scx, FSInode * ni, char * value, size_t size);
int fs_set_ntfs_acl(struct SECURITY_CONTEXT * scx, FSInode * ni, const char * value, size_t size, int flags);
int fs_get_ntfs_attrib(FSInode * ni, char * value, size_t size);
int fs_set_ntfs_attrib(FSInode * ni, const char * value, size_t size, int flags);

/**
 * Security API for direct access to security descriptors
 * based on Win32 API
 */

#define MAGIC_API 0x09042009

struct SECURITY_API
{
    u32 magic;
    struct SECURITY_CONTEXT security;
    struct PERMISSIONS_CACHE * seccache;
};

/*
 *  The following constants are used in interfacing external programs.
 *  They are not to be stored on disk and must be defined in their
 *  native cpu representation.
 *  When disk representation (le) is needed, use SE_DACL_PRESENT, etc.
 */
enum
{
    OWNER_SECURITY_INFORMATION = 1,
    GROUP_SECURITY_INFORMATION = 2,
    DACL_SECURITY_INFORMATION = 4,
    SACL_SECURITY_INFORMATION = 8
};

int fs_get_file_security(struct SECURITY_API * scapi, const char * path, u32 selection, char * buf, u32 buflen, u32 * psize);
int fs_set_file_security(struct SECURITY_API * scapi, const char * path, u32 selection, const char * attr);
int fs_get_file_attributes(struct SECURITY_API * scapi, const char * path);
bool fs_set_file_attributes(struct SECURITY_API * scapi, const char * path, s32 attrib);
bool fs_read_directory(struct SECURITY_API * scapi, const char * path, FSFillDir callback, void * context);
int fs_read_sds(struct SECURITY_API * scapi, char * buf, u32 size, u32 offset);
INDEX_ENTRY* fs_read_sii(struct SECURITY_API * scapi, INDEX_ENTRY * entry);
INDEX_ENTRY* fs_read_sdh(struct SECURITY_API * scapi, INDEX_ENTRY * entry);
struct SECURITY_API* fs_initialize_file_security(const char * device, unsigned long flags);
bool fs_leave_file_security(struct SECURITY_API * scx);

int fs_get_usid(struct SECURITY_API * scapi, uid_t uid, char * buf);
int fs_get_gsid(struct SECURITY_API * scapi, gid_t gid, char * buf);
int fs_get_user(struct SECURITY_API * scapi, const SID * usid);
int fs_get_group(struct SECURITY_API * scapi, const SID * gsid);


C_END_EXTERN_C

#endif // sandbox_SECURITY_H
