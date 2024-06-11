//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_RUNLIST_H
#define sandbox_RUNLIST_H
#include <c/clib.h>

#include "attr.h"
#include "types.h"
#include "volume.h"

C_BEGIN_EXTERN_C

struct _RunlistElement
{
    /* In memory vcn to lcn mapping structure element. */
        VCN vcn;        /* vcn = Starting virtual cluster number. */
        LCN lcn;        /* lcn = Starting logical cluster number. */
        s64 length;     /* Run length in clusters. */
};

extern RunlistElement* fs_rl_extend(FSAttr *na, RunlistElement *rl, int more_entries);
extern LCN fs_rl_vcn_to_lcn(const RunlistElement *rl, const VCN vcn);
extern s64 fs_rl_pread(const FSVolume *vol, const RunlistElement *rl, const s64 pos, s64 count, void *b);
extern s64 fs_rl_pwrite(const FSVolume *vol, const RunlistElement *rl, s64 ofs, const s64 pos, s64 count, void *b);
extern RunlistElement* fs_runlists_merge(RunlistElement *drl, RunlistElement *srl);
extern RunlistElement* fs_mapping_pairs_decompress(const FSVolume *vol, const ATTR_RECORD *attr, RunlistElement *old_rl);
extern int fs_get_nr_significant_bytes(const s64 n);
extern int fs_get_size_for_mapping_pairs(const FSVolume *vol, const RunlistElement *rl, const VCN start_vcn, int max_size);
extern int fs_write_significant_bytes(u8 *dst, const u8 *dst_max, const s64 n);
extern int fs_mapping_pairs_build(const FSVolume *vol, u8 *dst, const int dst_len, const RunlistElement *rl, const VCN start_vcn, RunlistElement const **stop_rl);
extern int fs_rl_truncate(Runlist **arl, const VCN start_vcn);
extern int fs_rl_sparse(Runlist *rl);
extern s64 fs_rl_get_compressed_size(FSVolume *vol, Runlist *rl);


C_END_EXTERN_C

#endif // sandbox_RUNLIST_H
