//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_MFT_H
#define sandbox_MFT_H
#include <c/clib.h>

#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C

extern int fs_mft_records_read(const FSVolume * vol, const MFT_REF mref, const s64 count, MFT_RECORD * b);

static __inline__ int fs_mft_record_read(const FSVolume * vol, const MFT_REF mref, MFT_RECORD * b)
{
    int ret;

    C_LOG_INFO("Entering for inode %lld\n", (long long)MREF(mref));
    ret = fs_mft_records_read(vol, mref, 1, b);
    C_LOG_INFO("Leave for inode\n");
    return ret;
}

extern int fs_mft_record_check(const FSVolume * vol, const MFT_REF mref, MFT_RECORD * m);
extern int fs_file_record_read(const FSVolume * vol, const MFT_REF mref, MFT_RECORD ** mrec, ATTR_RECORD ** attr);
extern int fs_mft_records_write(const FSVolume * vol, const MFT_REF mref, const s64 count, MFT_RECORD * b);

static __inline__ int fs_mft_record_write(const FSVolume * vol, const MFT_REF mref, MFT_RECORD * b)
{
    int ret;

    C_LOG_INFO("Entering for inode %lld\n", (long long)MREF(mref));
    ret = fs_mft_records_write(vol, mref, 1, b);
    C_LOG_INFO("\n");
    return ret;
}

static __inline__ u32 fs_mft_record_get_data_size(const MFT_RECORD * m)
{
    if (!m || !fs_is_mft_record(m->magic))
        return 0;
    /* Get the number of used bytes and return it. */
    return le32_to_cpu(m->bytes_in_use);
}

extern int fs_mft_record_layout(const FSVolume * vol, const MFT_REF mref, MFT_RECORD * mrec);
extern int fs_mft_record_format(const FSVolume * vol, const MFT_REF mref);
extern FSInode* fs_mft_record_alloc(FSVolume * vol, FSInode * base_ni);
extern FSInode* fs_mft_rec_alloc(FSVolume * vol, bool mft_data);
extern int fs_mft_record_free(FSVolume * vol, FSInode * ni);
extern int fs_mft_usn_dec(MFT_RECORD * mrec);

C_END_EXTERN_C

#endif // sandbox_MFT_H
