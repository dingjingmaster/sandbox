//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_ATTR_H
#define sandbox_ATTR_H
#include <c/clib.h>

#include "types.h"
#include "inode.h"
#include "layout.h"

C_BEGIN_EXTERN_C

#define test_nattr_flag(na, flag)       test_bit(NA_##flag, (na)->state)
#define set_nattr_flag(na, flag)        set_bit(NA_##flag, (na)->state)
#define clear_nattr_flag(na, flag)      clear_bit(NA_##flag, (na)->state)

#define NAttrInitialized(na)             test_nattr_flag(na, Initialized)
#define NAttrSetInitialized(na)           set_nattr_flag(na, Initialized)
#define NAttrClearInitialized(na)       clear_nattr_flag(na, Initialized)

#define NAttrNonResident(na)             test_nattr_flag(na, NonResident)
#define NAttrSetNonResident(na)           set_nattr_flag(na, NonResident)
#define NAttrClearNonResident(na)       clear_nattr_flag(na, NonResident)

#define NAttrBeingNonResident(na)       test_nattr_flag(na, BeingNonResident)
#define NAttrSetBeingNonResident(na)    set_nattr_flag(na, BeingNonResident)
#define NAttrClearBeingNonResident(na)  clear_nattr_flag(na, BeingNonResident)

#define NAttrFullyMapped(na)            test_nattr_flag(na, FullyMapped)
#define NAttrSetFullyMapped(na)         set_nattr_flag(na, FullyMapped)
#define NAttrClearFullyMapped(na)       clear_nattr_flag(na, FullyMapped)

#define NAttrDataAppending(na)          test_nattr_flag(na, DataAppending)
#define NAttrSetDataAppending(na)       set_nattr_flag(na, DataAppending)
#define NAttrClearDataAppending(na)     clear_nattr_flag(na, DataAppending)

#define NAttrRunlistDirty(na)           test_nattr_flag(na, RunlistDirty)
#define NAttrSetRunlistDirty(na)        set_nattr_flag(na, RunlistDirty)
#define NAttrClearRunlistDirty(na)      clear_nattr_flag(na, RunlistDirty)

#define NAttrComprClosing(na)           test_nattr_flag(na, ComprClosing)
#define NAttrSetComprClosing(na)        set_nattr_flag(na, ComprClosing)
#define NAttrClearComprClosing(na)      clear_nattr_flag(na, ComprClosing)

#define GenNAttrIno(func_name, flag)                    \
extern int NAttr##func_name(FSAttr *na);             \
extern void NAttrSet##func_name(FSAttr *na);         \
extern void NAttrClear##func_name(FSAttr *na);

GenNAttrIno(Compressed, FILE_ATTR_COMPRESSED)
GenNAttrIno(Encrypted,  FILE_ATTR_ENCRYPTED)
GenNAttrIno(Sparse,     FILE_ATTR_SPARSE_FILE)
#undef GenNAttrIno


extern FSChar AT_UNNAMED[];
extern FSChar STREAM_SDS[];
extern FSChar TXF_DATA[10];

typedef enum
{
    LCN_HOLE                = -1,   /* Keep this as highest value or die! */
    LCN_RL_NOT_MAPPED       = -2,
    LCN_ENOENT              = -3,
    LCN_EINVAL              = -4,
    LCN_EIO                 = -5,
} FSLcnSpecialValues;

/* ways of processing holes when expanding */
typedef enum
{
    HOLES_NO,
    HOLES_OK,
    HOLES_DELAY,
    HOLES_NONRES
} HoleType;

typedef enum
{
    NA_Initialized,         /* 1: structure is initialized. */
    NA_NonResident,         /* 1: Attribute is not resident. */
    NA_BeingNonResident,    /* 1: Attribute is being made not resident. */
    NA_FullyMapped,         /* 1: Attribute has been fully mapped */
    NA_DataAppending,       /* 1: Attribute is being appended to */
    NA_ComprClosing,        /* 1: Compressed attribute is being closed */
    NA_RunlistDirty,        /* 1: Runlist has been updated */
} FSAttrStateBits;


struct _FSAttrSearchCtx
{
    MFT_RECORD *mrec;
    ATTR_RECORD *attr;
    bool isFirst;
    FSInode *ntfsIno;
    ATTR_LIST_ENTRY *alEntry;
    FSInode *baseNtfsIno;
    MFT_RECORD *baseMrec;
    ATTR_RECORD *baseAttr;
};

struct _FSAttr
{
    RunlistElement *rl;
    FSInode *ni;
    ATTR_TYPES type;
    ATTR_FLAGS dataFlags;
    FSChar* name;
    u32 nameLen;
    unsigned long state;
    s64 allocatedSize;
    s64 dataSize;
    s64 initializedSize;
    s64 compressedSize;
    u32 compressionBlockSize;
    u8 compressionBlockSizeBits;
    u8 compressionBlockClusters;
    s8 unusedRuns; /* pre-reserved entries available */
};

typedef union
{
    u8 _default;    /* Unnamed u8 to serve as default when just using a_val without specifying any of the below. */
    STANDARD_INFORMATION std_inf;
    ATTR_LIST_ENTRY al_entry;
    FILE_NAME_ATTR filename;
    OBJECT_ID_ATTR obj_id;
    SECURITY_DESCRIPTOR_ATTR sec_desc;
    VOLUME_NAME vol_name;
    VOLUME_INFORMATION vol_inf;
    DATA_ATTR data;
    INDEX_ROOT index_root;
    INDEX_BLOCK index_blk;
    BITMAP_ATTR bmp;
    REPARSE_POINT reparse;
    EA_INFORMATION ea_inf;
    EA_ATTR ea;
    PROPERTY_SET property_set;
    LOGGED_UTILITY_STREAM logged_util_stream;
    EFS_ATTR_HEADER efs;
} AttrVal;

extern void fs_attr_reinit_search_ctx(FSAttrSearchCtx *ctx);
extern FSAttrSearchCtx *fs_attr_get_search_ctx(FSInode* ni, MFT_RECORD *mrec);
extern void fs_attr_put_search_ctx(FSAttrSearchCtx *ctx);

extern int fs_attr_lookup(const ATTR_TYPES type, const FSChar* name, const u32 name_len, const IGNORE_CASE_BOOL ic, const VCN lowest_vcn, const u8 *val, const u32 val_len, FSAttrSearchCtx *ctx);

extern int fs_attr_position(const ATTR_TYPES type, FSAttrSearchCtx *ctx);

extern ATTR_DEF *fs_attr_find_in_attrdef(const FSVolume *vol, const ATTR_TYPES type);

static __inline__ int fs_attrs_walk(FSAttrSearchCtx *ctx)
{
    return fs_attr_lookup(AT_UNUSED, NULL, 0, CASE_SENSITIVE, 0, NULL, 0, ctx);
}

extern void fs_attr_init(FSAttr *na, const bool non_resident, const ATTR_FLAGS data_flags, const bool encrypted, const bool sparse, const s64 allocated_size, const s64 data_size, const s64 initialized_size, const s64 compressed_size, const u8 compression_unit);

extern FSAttr* fs_attr_open(FSInode *ni, const ATTR_TYPES type, FSChar* name, u32 name_len);
extern void fs_attr_close(FSAttr *na);

extern s64 fs_attr_pread(FSAttr *na, const s64 pos, s64 count, void *b);
extern s64 fs_attr_pwrite(FSAttr *na, const s64 pos, s64 count, const void *b);
extern int fs_attr_pclose(FSAttr *na);

extern void* fs_attr_readall(FSInode *ni, const ATTR_TYPES type, FSChar *name, u32 name_len, s64 *data_size);
extern s64 fs_attr_mst_pread(FSAttr *na, const s64 pos, const s64 bk_cnt, const u32 bk_size, void *dst);
extern s64 fs_attr_mst_pwrite(FSAttr *na, const s64 pos, s64 bk_cnt, const u32 bk_size, void *src);

extern int fs_attr_map_runlist(FSAttr *na, VCN vcn);
extern int fs_attr_map_whole_runlist(FSAttr *na);

extern LCN fs_attr_vcn_to_lcn(FSAttr *na, const VCN vcn);
extern RunlistElement *fs_attr_find_vcn(FSAttr *na, const VCN vcn);

extern int fs_attr_size_bounds_check(const FSVolume *vol, const ATTR_TYPES type, const s64 size);
extern int fs_attr_can_be_resident(const FSVolume *vol, const ATTR_TYPES type);
int fs_attr_make_non_resident(FSAttr *na, FSAttrSearchCtx *ctx);
int fs_attr_force_non_resident(FSAttr *na);
extern int fs_make_room_for_attr(MFT_RECORD *m, u8 *pos, u32 size);

extern int fs_resident_attr_record_add(FSInode *ni, ATTR_TYPES type, const FSChar *name, u8 name_len, const u8 *val, u32 size, ATTR_FLAGS flags);
extern int fs_non_resident_attr_record_add(FSInode *ni, ATTR_TYPES type, const FSChar *name, u8 name_len, VCN lowest_vcn, int dataruns_size, ATTR_FLAGS flags);
extern int fs_attr_record_rm(FSAttrSearchCtx *ctx);

extern int fs_attr_add(FSInode *ni, ATTR_TYPES type, FSChar *name, u8 name_len, const u8 *val, s64 size);
extern int fs_attr_set_flags(FSInode *ni, ATTR_TYPES type, const FSChar *name, u8 name_len, ATTR_FLAGS flags, ATTR_FLAGS mask);
extern int fs_attr_rm(FSAttr *na);
extern int fs_attr_record_resize(MFT_RECORD *m, ATTR_RECORD *a, u32 new_size);
extern int fs_resident_attr_value_resize(MFT_RECORD *m, ATTR_RECORD *a, const u32 new_size);
extern int fs_attr_record_move_to(FSAttrSearchCtx *ctx, FSInode *ni);
extern int fs_attr_record_move_away(FSAttrSearchCtx *ctx, int extra);
extern int fs_attr_update_mapping_pairs(FSAttr *na, VCN from_vcn);
extern int fs_attr_truncate(FSAttr *na, const s64 newsize);
extern int fs_attr_truncate_solid(FSAttr *na, const s64 newsize);
extern s64 fs_get_attribute_value_length(const ATTR_RECORD *a);
extern s64 fs_get_attribute_value(const FSVolume *vol, const ATTR_RECORD *a, u8 *b);

extern void fs_attr_name_free(char **name);
extern char* fs_attr_name_get(const FSChar *uname, const int uname_len);
extern int fs_attr_exist(FSInode *ni, const ATTR_TYPES type, const FSChar *name, u32 name_len);
extern int fs_attr_remove(FSInode *ni, const ATTR_TYPES type, FSChar *name, u32 name_len);
extern s64 fs_attr_get_free_bits(FSAttr *na);
extern int fs_attr_data_read(FSInode *ni, FSChar *stream_name, int stream_name_len, char *buf, size_t size, off_t offset);
extern int fs_attr_data_write(FSInode *ni, FSChar *stream_name, int stream_name_len, const char *buf, size_t size, off_t offset);
extern int fs_attr_shrink_size(FSInode *ni, FSChar *stream_name, int stream_name_len, off_t offset);
extern int fs_attr_inconsistent(const ATTR_RECORD *a, const MFT_REF mref);


extern int fs_attrlist_need(FSInode* ni);
extern int fs_attrlist_entry_add(FSInode* ni, ATTR_RECORD *attr);
extern int fs_attrlist_entry_rm(FSAttrSearchCtx *ctx);

static __inline__ void fs_attrlist_mark_dirty(FSInode* ni)
{
    if (ni->nr_extents == -1) {
        NInoAttrListSetDirty(ni->base_ni);
    }
    else {
        NInoAttrListSetDirty(ni);
    }
}



C_END_EXTERN_C

#endif // sandbox_ATTR_H
