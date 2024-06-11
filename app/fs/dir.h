//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_DIR_H
#define sandbox_DIR_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

#define PATH_SEP '/'

/*
 * We do not have these under DJGPP, so define our version that do not conflict
 * with other S_IFs defined under DJGPP.
 */
#ifdef DJGPP
#ifndef S_IFLNK
#define S_IFLNK  0120000
#endif
#ifndef S_ISLNK
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#endif

extern FSChar NTFS_INDEX_I30[5];
extern FSChar NTFS_INDEX_SII[5];
extern FSChar NTFS_INDEX_SDH[5];
extern FSChar NTFS_INDEX_O[3];
extern FSChar NTFS_INDEX_Q[3];
extern FSChar NTFS_INDEX_R[3];

extern u64 fs_inode_lookup_by_name(FSInode *dir_ni, const FSChar *uname, const int uname_len);
extern u64 fs_inode_lookup_by_mbsname(FSInode *dir_ni, const char *name);
extern void fs_inode_update_mbsname(FSInode *dir_ni, const char *name, u64 inum);

extern FSInode* fs_pathname_to_inode(FSVolume *vol, FSInode *parent, const char *pathname);
extern FSInode* fs_create(FSInode *dir_ni, le32 securid, const FSChar *name, u8 name_len, mode_t type);
extern FSInode* fs_create_device(FSInode *dir_ni, le32 securid, const FSChar *name, u8 name_len, mode_t type, dev_t dev);
extern FSInode* fs_create_symlink(FSInode *dir_ni, le32 securid, const FSChar *name, u8 name_len, const FSChar *target, int target_len);
extern int fs_check_empty_dir(FSInode *ni);
extern int fs_delete(FSVolume *vol, const char *path, FSInode *ni, FSInode *dir_ni, const FSChar *name, u8 name_len);
extern int fs_link(FSInode *ni, FSInode *dir_ni, const FSChar *name, u8 name_len);

#define FS_DT_UNKNOWN         0
#define FS_DT_FIFO            1
#define FS_DT_CHR             2
#define FS_DT_DIR             4
#define FS_DT_BLK             6
#define FS_DT_REG             8
#define FS_DT_LNK             10
#define FS_DT_SOCK            12
#define FS_DT_WHT             14
#define FS_DT_REPARSE         32


typedef int (*fs_filldir_t)(void *dirent, const FSChar *name, const int name_len, const int name_type, const s64 pos, const MFT_REF mref, const unsigned dt_type);

extern int fs_readdir(FSInode *dir_ni, s64 *pos, void *dirent, fs_filldir_t filldir);

FSInode* fs_dir_parent_inode(FSInode *ni);
u32 fs_interix_types(FSInode *ni);

int fs_get_ntfs_dos_name(FSInode *ni, FSInode *dir_ni, char *value, size_t size);
int fs_set_ntfs_dos_name(FSInode *ni, FSInode *dir_ni, const char *value, size_t size, int flags);
int fs_remove_ntfs_dos_name(FSInode *ni, FSInode *dir_ni);
int fs_dir_link_cnt(FSInode *ni);

#if CACHE_INODE_SIZE

struct CACHED_GENERIC;

extern int fs_dir_inode_hash(const struct CACHED_GENERIC *cached);
extern int fs_dir_lookup_hash(const struct CACHED_GENERIC *cached);

#endif

C_END_EXTERN_C

#endif // sandbox_DIR_H
