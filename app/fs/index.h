//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_INDEX_H
#define sandbox_INDEX_H
#include <c/clib.h>

#include "mft.h"
#include "attr.h"
#include "inode.h"
#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C

#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

/* allows us to warn about unused results of certain function calls */
#ifndef __attribute_warn_unused_result__
# if __GNUC_PREREQ (3,4)
#  define __attribute_warn_unused_result__ \
    __attribute__ ((__warn_unused_result__))
# else
#  define __attribute_warn_unused_result__ /* empty */
# endif
#endif

#define  VCN_INDEX_ROOT_PARENT  ((VCN)-2)

#define  MAX_PARENT_VCN         32


struct _FSIndexContext
{
    FSInode *ni;
    FSChar *name;
    u32 name_len;
    INDEX_ENTRY *entry;
    void *data;
    u16 data_len;
    COLLATE collate;
    bool is_in_root;
    INDEX_ROOT *ir;
    FSAttrSearchCtx *actx;
    INDEX_BLOCK *ib;
    FSAttr *ia_na;
    int parent_pos[MAX_PARENT_VCN];  /* parent entries' positions */
    VCN parent_vcn[MAX_PARENT_VCN]; /* entry's parent nodes */
    int pindex;          /* maximum it's the number of the parent nodes  */
    bool ib_dirty;
    bool bad_index;
    u32 block_size;
    u8 vcn_size_bits;
};

extern FSIndexContext *fs_index_ctx_get(FSInode *ni, FSChar *name, u32 name_len);
extern void fs_index_ctx_put(FSIndexContext *ictx);
extern void fs_index_ctx_reinit(FSIndexContext *ictx);
extern int fs_index_block_inconsistent(const INDEX_BLOCK *ib, u32 block_size, u64 inum, VCN vcn);
extern int fs_index_entry_inconsistent(const INDEX_ENTRY *ie, COLLATION_RULES collation_rule, u64 inum);
extern int fs_index_lookup(const void *key, const int key_len, FSIndexContext* ictx) __attribute_warn_unused_result__;
extern INDEX_ENTRY *fs_index_next(INDEX_ENTRY *ie, FSIndexContext* ictx);
extern int fs_index_add_filename(FSInode *ni, FILE_NAME_ATTR *fn, MFT_REF mref);
extern int fs_index_remove(FSInode *dir_ni, FSInode *ni, const void *key, const int keylen);
extern INDEX_ROOT* fs_index_root_get(FSInode *ni, ATTR_RECORD *attr);
extern VCN fs_ie_get_vcn(INDEX_ENTRY *ie);
extern void fs_index_entry_mark_dirty(FSIndexContext *ictx);
extern char* fs_ie_filename_get(INDEX_ENTRY *ie);
extern void fs_ie_filename_dump(INDEX_ENTRY *ie);
extern void fs_ih_filename_dump(INDEX_HEADER *ih);

/* the following was added by JPA for use in security.c */
extern int fs_ie_add(FSIndexContext *icx, INDEX_ENTRY *ie);
extern int fs_index_rm(FSIndexContext *icx);




C_END_EXTERN_C

#endif // sandbox_INDEX_H
