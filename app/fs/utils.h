//
// Created by dingjing on 10/17/24.
//

#ifndef sandbox_UTILS_H
#define sandbox_UTILS_H

#include "../../3thrd/config.h"

#include "../../3thrd/fs/types.h"
#include "../../3thrd/fs/layout.h"
#include "../../3thrd/fs/volume.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

extern const char *ntfs_bugs;
extern const char *ntfs_gpl;

int utils_set_locale(void);
int utils_parse_size(const char *value, s64 *size, BOOL scale);
int utils_parse_range(const char *string, s64 *start, s64 *finish, BOOL scale);
int utils_inode_get_name(ntfs_inode *inode, char *buffer, int bufsize);
int utils_attr_get_name(ntfs_volume *vol, ATTR_RECORD *attr, char *buffer, int bufsize);
int utils_cluster_in_use(ntfs_volume *vol, long long lcn);
int utils_mftrec_in_use(ntfs_volume *vol, MFT_REF mref);
int utils_is_metadata(ntfs_inode *inode);
void utils_dump_mem(void *buf, int start, int length, int flags);

ATTR_RECORD * find_attribute(const ATTR_TYPES type, ntfs_attr_search_ctx *ctx);
ATTR_RECORD * find_first_attribute(const ATTR_TYPES type, MFT_RECORD *mft);

int utils_valid_device(const char *name, int force);
ntfs_volume * utils_mount_volume(const char *device, unsigned long flags);

/**
 * defines...
 * if *not in use* then the other flags are ignored?
 */
#define FEMR_IN_USE		(1 << 0)
#define FEMR_NOT_IN_USE		(1 << 1)
#define FEMR_FILE		(1 << 2)		// $DATA
#define FEMR_DIR		(1 << 3)		// $INDEX_ROOT, "$I30"
#define FEMR_METADATA		(1 << 4)
#define FEMR_NOT_METADATA	(1 << 5)
#define FEMR_BASE_RECORD	(1 << 6)
#define FEMR_NOT_BASE_RECORD	(1 << 7)
#define FEMR_ALL_RECORDS	0xFF

/**
 * struct mft_search_ctx
 */
struct mft_search_ctx {
	int flags_search;
	int flags_match;
	ntfs_inode *inode;
	ntfs_volume *vol;
	u64 mft_num;
};

struct mft_search_ctx * mft_get_search_ctx(ntfs_volume *vol);
void mft_put_search_ctx(struct mft_search_ctx *ctx);
int mft_next_record(struct mft_search_ctx *ctx);

// Flags for dump mem
#define DM_DEFAULTS	0
#define DM_NO_ASCII	(1 << 0)
#define DM_NO_DIVIDER	(1 << 1)
#define DM_INDENT	(1 << 2)
#define DM_RED		(1 << 3)
#define DM_GREEN	(1 << 4)
#define DM_BLUE		(1 << 5)
#define DM_BOLD		(1 << 6)

/* MAX_PATH definition was missing in ntfs-3g's headers. */
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

#ifdef HAVE_WINDOWS_H
/*
 *	Macroes to hide the needs to translate formats on older Windows
 */
#define MAX_FMT 1536
char *ntfs_utils_reformat(char *out, int sz, const char *fmt);
char *ntfs_utils_unix_path(const char *in);
#define ntfs_log_redirect(fn,fi,li,le,d,fmt, args...) \
		do { char _b[MAX_FMT]; ntfs_log_redirect(fn,fi,li,le,d, \
		ntfs_utils_reformat(_b,MAX_FMT,fmt), args); } while (0)
#define printf(fmt, args...) \
		do { char _b[MAX_FMT]; \
		printf(ntfs_utils_reformat(_b,MAX_FMT,fmt), args); } while (0)
#define fprintf(str, fmt, args...) \
		do { char _b[MAX_FMT]; \
		fprintf(str, ntfs_utils_reformat(_b,MAX_FMT,fmt), args); } while (0)
#define vfprintf(file, fmt, args) \
		do { char _b[MAX_FMT]; vfprintf(file, \
		ntfs_utils_reformat(_b,MAX_FMT,fmt), args); } while (0)
#endif

/**
 * linux-ntfs's ntfs_mbstoucs has different semantics, so we emulate it with
 * ntfs-3g's.
 */
int ntfs_mbstoucs_libntfscompat(const char *ins,
		ntfschar **outs, int outs_len);

/* This simple utility function was missing from libntfs-3g. */
static __inline__ ntfschar *ntfs_attr_get_name(ATTR_RECORD *attr)
{
	return (ntfschar*)((u8*)attr + le16_to_cpu(attr->name_offset));
}



#endif // sandbox_UTILS_H
