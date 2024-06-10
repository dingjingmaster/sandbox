//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_INODE_H
#define sandbox_INODE_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C


#define test_nino_flag(ni, flag)                test_bit(NI_##flag, (ni)->state)
#define set_nino_flag(ni, flag)                 set_bit(NI_##flag, (ni)->state)
#define clear_nino_flag(ni, flag)               clear_bit(NI_##flag, (ni)->state)

#define test_and_set_nino_flag(ni, flag)        test_and_set_bit(NI_##flag, (ni)->state)
#define test_and_clear_nino_flag(ni, flag)      test_and_clear_bit(NI_##flag, (ni)->state)

#define NInoDirty(ni)                           test_nino_flag(ni, Dirty)
#define NInoSetDirty(ni)                        set_nino_flag(ni, Dirty)
#define NInoClearDirty(ni)                      clear_nino_flag(ni, Dirty)
#define NInoTestAndSetDirty(ni)                 test_and_set_nino_flag(ni, Dirty)
#define NInoTestAndClearDirty(ni)               test_and_clear_nino_flag(ni, Dirty)

#define NInoAttrList(ni)                        test_nino_flag(ni, AttrList)
#define NInoSetAttrList(ni)                     set_nino_flag(ni, AttrList)
#define NInoClearAttrList(ni)                   clear_nino_flag(ni, AttrList)


#define test_nino_al_flag(ni, flag)             test_nino_flag(ni, AttrList##flag)
#define set_nino_al_flag(ni, flag)              set_nino_flag(ni, AttrList##flag)
#define clear_nino_al_flag(ni, flag)            clear_nino_flag(ni, AttrList##flag)

#define test_and_set_nino_al_flag(ni, flag)     test_and_set_nino_flag(ni, AttrList##flag)
#define test_and_clear_nino_al_flag(ni, flag)   test_and_clear_nino_flag(ni, AttrList##flag)

#define NInoAttrListDirty(ni)                   test_nino_al_flag(ni, Dirty)
#define NInoAttrListSetDirty(ni)                set_nino_al_flag(ni, Dirty)
#define NInoAttrListClearDirty(ni)              clear_nino_al_flag(ni, Dirty)
#define NInoAttrListTestAndSetDirty(ni)         test_and_set_nino_al_flag(ni, Dirty)
#define NInoAttrListTestAndClearDirty(ni)       test_and_clear_nino_al_flag(ni, Dirty)

#define NInoFileNameDirty(ni)                   test_nino_flag(ni, FileNameDirty)
#define NInoFileNameSetDirty(ni)                set_nino_flag(ni, FileNameDirty)
#define NInoFileNameClearDirty(ni)              clear_nino_flag(ni, FileNameDirty)
#define NInoFileNameTestAndSetDirty(ni)         test_and_set_nino_flag(ni, FileNameDirty)
#define NInoFileNameTestAndClearDirty(ni)       test_and_clear_nino_flag(ni, FileNameDirty)


typedef enum
{
    FSI_DIRTY,              /* 1: Mft record needs to be written to disk. */

    /* The NI_AttrList* tests only make sense for base inodes. */
    FSI_ATTR_LIST,          /* 1: Mft record contains an attribute list. */
    FSI_ATTR_LIST_DIRTY,    /* 1: Attribute list needs to be written to the mft record and then to disk. */
    FSI_FILE_NAME_DIRTY,    /* 1: FILE_NAME attributes need to be updated in the index. */
    FSI_V3_EXTENSIONS,      /* 1: JPA v3.x extensions present. */
    FSI_TIMES_SET,          /* 1: Use times which were set */
    FSI_KNOWN_SIZE,         /* 1: Set if sizes are meaningful */
} FSInodeStateBits;

typedef enum
{
    FS_UPDATE_ATIME = 1 << 0,
    FS_UPDATE_MTIME = 1 << 1,
    FS_UPDATE_CTIME = 1 << 2,
} FSTimeUpdateFlags;

struct _FSInode
{
    cuint64                     mftNo;             /* Inode / mft record number. */
    MFT_RECORD *mrec;       /* The actual mft record of the inode. */
    FSVolume*                   vol;       /* Pointer to the ntfs volume of this inode. */
    unsigned long state;    /* FS specific flags describing this inode. See ntfs_inode_state_bits above. */
    FILE_ATTR_FLAGS flags;  /* Flags describing the file. (Copy from STANDARD_INFORMATION) */

    cuint32 attr_list_size;     /* Length of attribute list value in bytes. */
    cuint8 *attr_list;          /* Attribute list value itself. */

    /* Below fields are always valid. */
    cint32 nr_extents;         /* For a base mft record, the number of attached extent inodes (0 if none), for extent records this is -1. */
    union {         /* This union is only used if nr_extents != 0. */
        FSInode** extent_nis;/* For nr_extents > 0, array of the ntfs inodes of the extent mft records belonging to this base inode which have been loaded. */
        FSInode* base_ni;    /* For nr_extents == -1, the ntfs inode of the base mft record. */
    };

    /* Below fields are valid only for base inode. */
    cint64 data_size;          /* Data size of unnamed DATA attribute(or INDEX_ROOT for directories) */
    cint64 allocated_size;     /* Allocated size stored in the filename index. (NOTE: Equal to allocated size of the unnamed data attribute for normal or encrypted files and to compressed size of the unnamed data attribute for sparse or compressed files.) */

    cint64 creation_time;
    cint64 last_data_change_time;
    cint64 last_mft_change_time;
    cint64 last_access_time; /* NTFS 3.x extensions added by JPA. only if NI_v3_Extensions is set in state */
    cint32  owner_id;
    cint32  security_id;
    cint64  quota_charged;
    cint64  usn;
};

extern ntfs_inode *ntfs_inode_base(ntfs_inode *ni);

extern ntfs_inode *ntfs_inode_allocate(ntfs_volume *vol);

extern ntfs_inode *ntfs_inode_open(ntfs_volume *vol, const MFT_REF mref);

extern int ntfs_inode_close(ntfs_inode *ni);
extern int ntfs_inode_close_in_dir(ntfs_inode *ni, ntfs_inode *dir_ni);

#if CACHE_NIDATA_SIZE

struct CACHED_GENERIC;

extern int ntfs_inode_real_close(ntfs_inode *ni);
extern void ntfs_inode_invalidate(ntfs_volume *vol, const MFT_REF mref);
extern void ntfs_inode_nidata_free(const struct CACHED_GENERIC *cached);
extern int ntfs_inode_nidata_hash(const struct CACHED_GENERIC *item);

#endif


extern ntfs_inode *ntfs_extent_inode_open(ntfs_inode *base_ni,
                const leMFT_REF mref);

extern int ntfs_inode_attach_all_extents(ntfs_inode *ni);

extern void ntfs_inode_mark_dirty(ntfs_inode *ni);

extern void ntfs_inode_update_times(ntfs_inode *ni, ntfs_time_update_flags mask);

extern int ntfs_inode_sync(ntfs_inode *ni);

extern int ntfs_inode_add_attrlist(ntfs_inode *ni);

extern int ntfs_inode_free_space(ntfs_inode *ni, int size);

extern int ntfs_inode_badclus_bad(u64 mft_no, ATTR_RECORD *a);

extern int ntfs_inode_get_times(ntfs_inode *ni, char *value, size_t size);

extern int ntfs_inode_set_times(ntfs_inode *ni, const char *value,
                        size_t size, int flags);


C_END_EXTERN_C

#endif // sandbox_INODE_H
