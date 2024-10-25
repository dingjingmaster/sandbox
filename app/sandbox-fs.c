//
// Created by dingjing on 10/16/24.
//

#include "sandbox-fs.h"

#include "andsec-types.h"

#include <locale.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../3thrd/config.h"
#endif

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#ifdef ENABLE_UUID
#include <uuid/uuid.h>
#endif


#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
    extern char *optarg;
    extern int optind;
#endif

#ifdef HAVE_LINUX_MAJOR_H
#    include <linux/major.h>
#    ifndef MAJOR
#        define MAJOR(dev)    ((dev) >> 8)
#        define MINOR(dev)    ((dev) & 0xff)
#    endif
#    ifndef IDE_DISK_MAJOR
#        ifndef IDE0_MAJOR
#            define IDE0_MAJOR    3
#            define IDE1_MAJOR    22
#            define IDE2_MAJOR    33
#            define IDE3_MAJOR    34
#            define IDE4_MAJOR    56
#            define IDE5_MAJOR    57
#            define IDE6_MAJOR    88
#            define IDE7_MAJOR    89
#            define IDE8_MAJOR    90
#            define IDE9_MAJOR    91
#        endif
#        define IDE_DISK_MAJOR(M) \
                ((M) == IDE0_MAJOR || (M) == IDE1_MAJOR || \
                (M) == IDE2_MAJOR || (M) == IDE3_MAJOR || \
                (M) == IDE4_MAJOR || (M) == IDE5_MAJOR || \
                (M) == IDE6_MAJOR || (M) == IDE7_MAJOR || \
                (M) == IDE8_MAJOR || (M) == IDE9_MAJOR)
#    endif
#    ifndef SCSI_DISK_MAJOR
#        ifndef SCSI_DISK0_MAJOR
#            define SCSI_DISK0_MAJOR    8
#            define SCSI_DISK1_MAJOR    65
#            define SCSI_DISK7_MAJOR    71
#        endif
#        define SCSI_DISK_MAJOR(M) \
                ((M) == SCSI_DISK0_MAJOR || \
                ((M) >= SCSI_DISK1_MAJOR && \
                (M) <= SCSI_DISK7_MAJOR))
#    endif
#endif

#include <dlfcn.h>
#include <glib.h>
#include <fuse.h>
#include <pwd.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

#include "utils.h"
#include "c/clib.h"
#include "./fs/sd.h"
#include "./fs/boot.h"
#include "./fs/utils.h"
#include "./fs/attrdef.h"
#include "../3thrd/fs/dir.h"
#include "../3thrd/fs/mft.h"
#include "../3thrd/fs/mst.h"
#include "../3thrd/fs/misc.h"
#include "../3thrd/fs/types.h"
#include "../3thrd/fs/param.h"
#include "../3thrd/fs/xattrs.h"
#include "../3thrd/fs/unistr.h"
#include "../3thrd/fs/device.h"
#include "../3thrd/fs/attrib.h"
#include "../3thrd/fs/bitmap.h"
#include "../3thrd/fs/plugin.h"
#include "../3thrd/fs/compat.h"
#include "../3thrd/fs/logging.h"
#include "../3thrd/fs/support.h"
#include "../3thrd/fs/runlist.h"
#include "../3thrd/fs/ntfstime.h"
#include "../3thrd/fs/bootsect.h"
#include "../3thrd/fs/security.h"


#undef byte
#undef uint32_t
#undef uint64_t
#define byte u8
#define uint32_t u32
#define uint64_t u64

#if defined(__sun) && defined (__SVR4)
#undef basename
#define basename(name) name
#endif

typedef enum { WRITE_STANDARD, WRITE_BITMAP, WRITE_LOGFILE } WRITE_TYPE;

#ifdef NO_NTFS_DEVICE_DEFAULT_IO_OPS
#error "No default device io operations!  Cannot build.  \
You need to run ./configure without the --disable-default-device-io-ops \
switch if you want to be able to build the NTFS utilities."
#endif

/* Page size on ia32. Can change to 8192 on Alpha. */
#define NTFS_PAGE_SIZE                      4096

// check --start
#define RETURN_FS_ERRORS_CORRECTED          (1)
#define RETURN_SYSTEM_NEEDS_REBOOT          (2)
#define RETURN_FS_ERRORS_LEFT_UNCORRECTED   (4)
#define RETURN_OPERATIONAL_ERROR            (8)
#define RETURN_USAGE_OR_SYNTAX_ERROR        (16)
#define RETURN_CANCELLED_BY_USER            (32)
/* Where did 64 go? */
#define RETURN_SHARED_LIBRARY_ERROR         (128)

#define DIRTY_NONE		                    (0)
#define DIRTY_INODE		                    (1)
#define DIRTY_ATTRIB		                (2)

#define NTFSCK_PROGBAR		                0x0001

#define NTFS_PROGBAR                        0x0001
#define NTFS_PROGBAR_SUPPRESS               0x0002
#define NTFS_MBYTE                          (1000 * 1000)

/*	ACLS may be checked by kernel (requires a fuse patch) or here */
#define KERNELACLS                          ((HPERMSCONFIG > 6) & (HPERMSCONFIG < 10))
/*	basic permissions may be checked by kernel or here */
#define KERNELPERMS                         (((HPERMSCONFIG - 1) % 6) < 3)
/*	may want to use fuse/kernel cacheing */
#define CACHEING                            (!(HPERMSCONFIG % 3))

static const char ntfs_bad_reparse[] = "unsupported reparse tag 0x%08lx";
#define ntfs_bad_reparse_lth                (sizeof(ntfs_bad_reparse) + 2)
#define set_archive(ni)                     ((ni)->flags |= FILE_ATTR_ARCHIVE)
#define ntfs_real_allowed_access(scx, ni, type)     ntfs_allowed_access(scx, ni, type)

#define CALL_REPARSE_PLUGIN(ni, op_name, ...)	\
(reparse = (REPARSE_POINT*)NULL,			 \
ops = select_reparse_plugin(ctx, ni, &reparse),	 \
(!ops ? -errno						 \
: (ops->op_name ?				 \
ops->op_name(ni, reparse, __VA_ARGS__)  \
: -EOPNOTSUPP))),			 \
free(reparse)


#define STRAPPEND_MAX_INSIZE   8192
#define strappend_is_large(x) ((x) > STRAPPEND_MAX_INSIZE)

struct BITMAP_ALLOCATION
{
    struct BITMAP_ALLOCATION *next;
    LCN    lcn;                 /* first allocated cluster */
    s64    length;              /* count of consecutive clusters */
};

/* Upcase $Info, used since Windows 8 */
struct UPCASEINFO
{
    le32    len;
    le32    filler;
    le64    crc;
    le32    osmajor;
    le32    osminor;
    le32    build;
    le16    packmajor;
    le16    packminor;
};

// resize
enum mirror_source
{
    MIRR_OLD,
    MIRR_NEWMFT,
    MIRR_MFT
};

enum
{
    CLOSE_COMPRESSED = 1,
    CLOSE_ENCRYPTED = 2,
    CLOSE_DMTIME = 4,
    CLOSE_REPARSE = 8
};

typedef enum
{
    FSTYPE_NONE,
    FSTYPE_UNKNOWN,
    FSTYPE_FUSE,
    FSTYPE_FUSEBLK
} fuse_fstype;

typedef enum
{
    ERR_PLUGIN = 1
} single_log_t;

typedef enum
{
    ATIME_ENABLED,
    ATIME_DISABLED,
    ATIME_RELATIVE
} ntfs_atime_t;

enum
{
    FLGOPT_BOGUS        = 1,
    FLGOPT_STRING       = 2,
    FLGOPT_OCTAL        = 4,
    FLGOPT_DECIMAL      = 8,
    FLGOPT_APPEND       = 16,
    FLGOPT_NOSUPPORT    = 32,
    FLGOPT_OPTIONAL     = 64
};

enum
{
    OPT_RO,
    OPT_NOATIME,
    OPT_ATIME,
    OPT_RELATIME,
    OPT_DMTIME,
    OPT_RW,
    OPT_FAKE_RW,
    OPT_FSNAME,
    OPT_NO_DEF_OPTS,
    OPT_DEFAULT_PERMISSIONS,
    OPT_PERMISSIONS,
    OPT_ACL,
    OPT_UMASK,
    OPT_FMASK,
    OPT_DMASK,
    OPT_UID,
    OPT_GID,
    OPT_SHOW_SYS_FILES,
    OPT_HIDE_HID_FILES,
    OPT_HIDE_DOT_FILES,
    OPT_IGNORE_CASE,
    OPT_WINDOWS_NAMES,
    OPT_COMPRESSION,
    OPT_NOCOMPRESSION,
    OPT_SILENT,
    OPT_RECOVER,
    OPT_NORECOVER,
    OPT_REMOVE_HIBERFILE,
    OPT_SYNC,
    OPT_BIG_WRITES,
    OPT_LOCALE,
    OPT_NFCONV,
    OPT_NONFCONV,
    OPT_STREAMS_INTERFACE,
    OPT_USER_XATTR,
    OPT_NOAUTO,
    OPT_DEBUG,
    OPT_NO_DETACH,
    OPT_REMOUNT,
    OPT_BLKSIZE,
    OPT_INHERIT,
    OPT_ADDSECURIDS,
    OPT_STATICGRPS,
    OPT_USERMAPPING,
    OPT_XATTRMAPPING,
    OPT_EFS_RAW,
    OPT_POSIX_NLINK,
    OPT_SPECIAL_FILES,
    OPT_HELP,
    OPT_VERSION,
};

typedef enum
{
    NF_STREAMS_INTERFACE_NONE,	    /* No access to named data streams. */
    NF_STREAMS_INTERFACE_XATTR,	    /* Map named data streams to xattrs. */
    NF_STREAMS_INTERFACE_OPENXATTR,	/* Same, not limited to "user." */
    NF_STREAMS_INTERFACE_WINDOWS,	/* "file:stream" interface. */
} ntfs_fuse_streams_interface;

enum
{
    XATTRNS_NONE,
    XATTRNS_USER,
    XATTRNS_SYSTEM,
    XATTRNS_SECURITY,
    XATTRNS_TRUSTED,
    XATTRNS_OPEN
};

struct bitmap
{
    s64 size;
    u8 *bm;
};

struct llcn_t
{
    s64 lcn;	/* last used LCN for a "special" file/attr type */
    s64 inode;	/* inode using it */
};

struct progress_bar
{
    u64 start;
    u64 stop;
    int resolution;
    int flags;
    float unit;
};

typedef struct {
    ntfs_inode *ni;		     /* inode being processed */
    ntfs_attr_search_ctx *ctx;   /* inode attribute being processed */
    s64 inuse;		     /* num of clusters in use */
    int multi_ref;		     /* num of clusters referenced many times */
    int outsider;		     /* num of clusters outside the volume */
    int show_outsider;	     /* controls showing the above information */
    int flags;
    struct bitmap lcn_bitmap;
} ntfsck_t;

/* runlists which have to be processed later */
struct DELAYED
{
    struct DELAYED *next;
    ATTR_TYPES type;
    MFT_REF mref;
    VCN lowest_vcn;
    int name_len;
    ntfschar *attr_name;
    runlist_element *rl;
    runlist *head_rl;
};

typedef struct
{
    ntfs_volume *vol;
    ntfs_inode *ni;		            /* inode being processed */
    s64 new_volume_size;	        /* in clusters; 0 = --info w/o --size */
    MFT_REF mref;                   /* mft reference */
    MFT_RECORD *mrec;               /* mft record */
    ntfs_attr_search_ctx *ctx;      /* inode attribute being processed */
    u64 relocations;	            /* num of clusters to relocate */
    s64 inuse;		                /* num of clusters in use */
    runlist mftmir_rl;	            /* $MFTMirr AT_DATA's new position */
    s64 mftmir_old;		            /* $MFTMirr AT_DATA's old LCN */
    int dirty_inode;	            /* some inode data got relocated */
    int shrink;		                /* shrink = 1, 缩小 enlarge = 0 */
    s64 badclusters;	            /* num of physically dead clusters */
    VCN mft_highest_vcn;	        /* used for relocating the $MFT */
    runlist_element *new_mft_start; /* new first run for $MFT:$DATA */
    struct DELAYED *delayed_runlists; /* runlists to process later */
    struct progress_bar progress;
    struct bitmap lcn_bitmap;
    /* Temporary statistics until all case is supported */
    struct llcn_t last_mft;
    struct llcn_t last_mftmir;
    struct llcn_t last_multi_mft;
    struct llcn_t last_sparse;
    struct llcn_t last_compressed;
    struct llcn_t last_lcn;
    s64 last_unsupp;	            /* last unsupported cluster */
    enum mirror_source mirr_from;
} ntfs_resize_t;

typedef struct EXPAND {
    ntfs_volume *vol;
    u64 original_sectors;
    u64 new_sectors;
    u64 bitmap_allocated;
    u64 bitmap_size;
    u64 boot_size;
    u64 mft_size;
    LCN mft_lcn;
    s64 byte_increment;
    s64 sector_increment;
    s64 cluster_increment;
    u8 *bitmap;
    u8 *mft_bitmap;
    char *bootsector;
    MFT_RECORD *mrec;
    struct progress_bar *progress;
    struct DELAYED *delayed_runlists; /* runlists to process later */
} expand_t;

typedef struct plugin_list
{
    struct plugin_list *next;
    void *handle;
    const plugin_operations_t *ops;
    le32 tag;
} plugin_list_t;

typedef struct
{
    ntfs_volume *vol;
    unsigned int uid;
    unsigned int gid;
    unsigned int fmask;
    unsigned int dmask;
    ntfs_fuse_streams_interface streams;
    ntfs_atime_t atime;
    s64 dmtime;
    BOOL ro;
    BOOL rw;
    BOOL show_sys_files;
    BOOL hide_hid_files;
    BOOL hide_dot_files;
    BOOL windows_names;
    BOOL ignore_case;
    BOOL compression;
    BOOL acl;
    BOOL silent;
    BOOL recover;
    BOOL hiberfile;
    BOOL sync;
    BOOL big_writes;
    BOOL debug;
    BOOL no_detach;
    BOOL blkdev;
    BOOL mounted;
    BOOL posix_nlink;
    ntfs_volume_special_files special_files;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
    BOOL efs_raw;
#ifdef XATTR_MAPPINGS
    char *xattrmap_path;
#endif /* XATTR_MAPPINGS */
#endif /* HAVE_SETXATTR */
    struct fuse_chan *fc;
    BOOL inherit;
    unsigned int secure_flags;
    single_log_t errors_logged;
    char *usermap_path;
    char *abs_mnt_point;
#ifndef DISABLE_PLUGINS
    plugin_list_t *plugins;
#endif /* DISABLE_PLUGINS */
    struct PERMISSIONS_CACHE *seccache;
    struct SECURITY_CONTEXT security;
    struct open_file *open_files; /* only defined in lowntfs-3g */
    u64 latest_ghost;
} ntfs_fuse_context_t;

typedef struct
{
    fuse_fill_dir_t filler;
    void *buf;
} ntfs_fuse_fill_context_t;

struct DEFOPTION
{
    const char *name;
    int type;
    int flags;
};

/**
 * @brief 沙盒核心结构
 *
 */
struct _SandboxFs
{
    char*                       dev;
    char*                       mountPoint;

    bool                        isMounted;
};

G_LOCK_DEFINE(gsSandbox);
#define SANDBOX_FS_MUTEX_LOCK()                 G_LOCK(gsSandbox)
#define SANDBOX_FS_MUTEX_UNLOCK()               G_UNLOCK(gsSandbox)


static int drop_privs                           (void);
static void ntfs_close                          (void);
static int restore_privs                        (void);
static int ntfs_fuse_init                       (void);
static void mkntfs_cleanup                      (void);
static void create_dev_fuse                     (void);
static ntfs_time mkntfs_time                    (void);
static long mkntfs_get_page_size                (void);
static fuse_fstype get_fuse_fstype              (void);
static fuse_fstype load_fuse_module             (void);
static BOOL mkntfs_initialize_rl_bad            (void);
static BOOL mkntfs_initialize_rl_mft            (void);
static BOOL mkntfs_initialize_rl_boot           (void);
static BOOL mkntfs_initialize_bitmaps           (void);
static BOOL mkntfs_initialize_rl_logfile        (void);
static BOOL mkntfs_create_root_structures       (void);
static BOOL mkntfs_fill_device_with_zeroes      (void);
static void register_internal_reparse_plugins   (void);
static void set_fuse_error                      (int *err);
static int create_backup_boot_sector            (u8 *buff);
static int mft_bitmap_get_bit                   (s64 mft_no);
static void dump_runlist                        (runlist *rl);
static int rl_items                             (runlist *rl);
static VCN get_last_vcn                         (runlist *rl);
static void rl_fixup                            (runlist **rl);
static runlist * allocate_scattered_clusters    (s64 clusters);
static int64_t align_4096                       (int64_t value);
static int64_t align_1024                       (int64_t value);
static ntfs_time stdinfo_time                   (MFT_RECORD *m);
static int initialize_quota                     (MFT_RECORD *m);
static BOOL non_resident_unnamed_data           (MFT_RECORD *m);
static int inode_close                          (ntfs_inode *ni);
static void mknod_dev_fuse                      (const char *dev);
static ntfs_volume* check_volume_dev            (const char* dev);
static s64 nr_clusters_to_bitmap_byte_size      (s64 nr_clusters);
static int ntfs_fuse_is_named_data_stream       (const char *path);
static void update_bootsector                   (ntfs_resize_t *r);
static void prepare_volume_fixup                (ntfs_volume *vol);
static int check_bad_sectors                    (ntfs_volume *vol);
static int copy_boot                            (expand_t *expand);
static void check_volume                        (ntfs_volume *vol);
static int reset_dirty                          (ntfs_volume *vol);
static u8 *get_mft_bitmap                       (expand_t *expand);
static int check_expand_constraints             (expand_t *expand);
static int write_bitmap                         (expand_t *expand);
static BOOL mkntfs_override_vol_params          (ntfs_volume *vol);
static int rebase_all_inodes                    (expand_t *expand);
static int expand_index_sizes                   (expand_t *expand);
static int copy_mftmirr                         (expand_t *expand);
static int write_bootsector                     (expand_t *expand);
static int ntfs_fuse_rmdir                      (const char *path);
static int xattr_namespace                      (const char *name);
static ntfs_inode *get_parent_dir               (const char *path);
static s64 ntfs_get_nr_free_mft_records         (ntfs_volume* vol);
static bool mkntfs_init_sandbox_header          (ntfs_volume* vol);
static bool check_efs_header                    (ntfs_volume* vol);
static void deallocate_scattered_clusters       (const runlist *rl);
static int ntfs_open                            (const char *device);
static ntfs_volume *mount_volume                (const char* volume);
static void dump_run                            (runlist_element *r);
static void apply_umask                         (struct stat *stbuf);
static int expand_to_beginning                  (const char* devPath);
static BOOL bitmap_deallocate                   (LCN lcn, s64 length);
static int verify_mft_preliminary               (ntfs_volume *rawvol);
static int mft_bitmap_load                      (ntfs_volume *rawvol);
static int ntfs_fuse_rm                         (const char *org_path);
static s64 rounded_up_division                  (s64 numer, s64 denom);
static int ntfs_fuse_unlink                     (const char *org_path);
static void setup_logging                       (char *parsed_options);
static void build_resize_constraints            (ntfs_resize_t *resize);
static void truncate_bitmap_data_attr           (ntfs_resize_t *resize);
static int record_mft_in_bitmap                 (ntfs_resize_t *resize);
static void truncate_badclust_bad_attr          (ntfs_resize_t *resize);
static void print_num_of_relocations            (ntfs_resize_t *resize);
static void delayed_updates                     (ntfs_resize_t *resize);
static void relocate_inodes                     (ntfs_resize_t *resize);
static void check_resize_constraints            (ntfs_resize_t *resize);
static bool fs_sandbox_header_check             (EfsFileHeader* header);
static void set_resize_constraints              (ntfs_resize_t *resize);
static void set_disk_usage_constraint           (ntfs_resize_t *resize);
static void truncate_badclust_file              (ntfs_resize_t *resize);
static void truncate_bitmap_file                (ntfs_resize_t *resize);
static void resize_constraints_by_attributes    (ntfs_resize_t *resize);
static int reload_mft                           (ntfs_resize_t *resize);
static int is_mftdata                           (ntfs_resize_t *resize);
static void relocate_attribute                  (ntfs_resize_t *resize);
static int set_fuseblk_options                  (char **parsed_options);
static BOOL check_file_record                   (u8 *buffer, u16 buflen);
static BOOL append_to_bad_blocks                (unsigned long long block);
void close_reparse_plugins                      (ntfs_fuse_context_t *ctx);
static void close_inode_and_context             (ntfs_attr_search_ctx *ctx);
static void lseek_to_cluster                    (ntfs_volume *vol, s64 lcn);
static ntfs_attr *open_badclust_bad_attr        (ntfs_attr_search_ctx *ctx);
static s64 get_data_size                        (expand_t *expand, s64 inum);
static runlist_element *rebase_runlists_meta    (expand_t *expand, s64 inum);
static int rebase_runlists                      (expand_t *expand, s64 inum);
static void *ntfs_init                          (struct fuse_conn_info *conn);
static BOOL ntfs_fuse_fill_security_context     (struct SECURITY_CONTEXT *scx);
static void bitmap_build                        (u8 *buf, LCN lcn, s64 length);
static int ntfs_fuse_mkdir                      (const char *path, mode_t mode);
static int ntfs_fuse_chmod                      (const char *path, mode_t mode);
static void verify_mft_record                   (ntfs_volume *vol, s64 mft_num);
static int bitmap_get_and_set                   (LCN lcn, unsigned long length);
static void rl_split_run                        (runlist **rl, int run, s64 pos);
static void release_bitmap_clusters             (struct bitmap *bm, runlist *rl);
static void bitmap_file_data_fixup              (s64 cluster, struct bitmap *bm);
static s64 vol_size                             (ntfs_volume *v, s64 nr_clusters);
static int ntfs_strinsert                       (char **dest, const char *append);
int ntfs_strappend                              (char **dest, const char *append);
static int build_allocation_bitmap              (ntfs_volume *vol, ntfsck_t *fsck);
static void check_cluster_allocation            (ntfs_volume *vol, ntfsck_t *fsck);
static int check_expand_bad_sectors             (expand_t *expand, ATTR_RECORD *a);
static int walk_attributes                      (ntfs_volume *vol, ntfsck_t *fsck);
static int ntfs_fuse_symlink                    (const char *to, const char *from);
static ntfs_attr_search_ctx *attr_get_search_ctx(ntfs_inode *ni, MFT_RECORD *mrec);
static int ntfs_fuse_truncate                   (const char *org_path, off_t size);
static void build_lcn_usage_bitmap              (ntfs_volume *vol, ntfsck_t *fsck);
static int assert_u32_equal                     (u32 val, u32 ok, const char *name);
static int ntfs_fuse_removexattr                (const char *path, const char *name);
static void collect_relocation_info             (ntfs_resize_t *resize, runlist *rl);
static int replace_attribute_runlist            (ntfs_resize_t *resize, runlist *rl);
static int minimal_record                       (expand_t *expand, MFT_RECORD *mrec);
static BOOL can_expand                          (expand_t *expand, ntfs_volume *vol);
static int setup_lcn_bitmap                     (struct bitmap *bm, s64 nr_clusters);
static void compare_bitmaps                     (ntfs_volume *vol, struct bitmap *a);
static void collect_resize_constraints          (ntfs_resize_t *resize, runlist *rl);
static void realloc_lcn_bitmap                  (ntfs_resize_t *resize, s64 bm_bsize);
static void progress_update                     (struct progress_bar *p, u64 current);
static void rl_insert_at_run                    (runlist **rl, int run, runlist *ins);
static void ntfs_fuse_destroy2                  (void *unused __attribute__((unused)));
static int assert_u32_noteq                     (u32 val, u32 wrong, const char *name);
static int assert_u32_less                      (u32 val1, u32 val2, const char *name);
static int assert_u32_lesseq                    (u32 val1, u32 val2, const char *name);
static int add_attr_object_id                   (MFT_RECORD *m, const GUID *object_id);
static int handle_mftdata                       (ntfs_resize_t *resize, int do_mftdata);
static int really_expand                        (expand_t* expand, const char* devPath);
static int set_bitmap                           (expand_t *expand, runlist_element *rl);
static BOOL mkntfs_open_partition               (ntfs_volume *vol, const char* devName);
static void relocate_attributes                 (ntfs_resize_t *resize, int do_mftdata);
static int ntfs_fuse_chown                      (const char *path, uid_t uid, gid_t gid);
static void set_bitmap_clusters                 (struct bitmap *bm, runlist *rl, u8 bit);
static void print_disk_usage                    (ntfs_volume *vol, s64 nr_used_clusters);
static int initialize_secure                    (char *sds, u32 sds_size, MFT_RECORD *m);
static void rl_set                              (runlist *rl, VCN vcn, LCN lcn, s64 len);
static int ntfs_fuse_mknod                      (const char *path, mode_t mode, dev_t dev);
static void replay_log                          (ntfs_volume *vol __attribute__((unused)));
static int make_room_for_attribute              (MFT_RECORD *m, char *pos, const u32 size);
static int ntfs_fuse_getattr                    (const char *org_path, struct stat *stbuf);
static int ntfs_fuse_listxattr                  (const char *path, char *list, size_t size);
static void expand_attribute_runlist            (ntfs_volume *vol, struct DELAYED *delayed);
static void set_max_free_zone                   (s64 length, s64 end, runlist_element *rle);
static struct fuse_chan *try_fuse_mount         (const char* devName, char *parsed_options);
static int ntfs_fuse_rename_existing_dest       (const char *old_path, const char *new_path);
static int ntfs_fuse_link                       (const char *old_path, const char *new_path);
static int ntfs_fuse_rename                     (const char *old_path, const char *new_path);
static void ntfs_fuse_update_times              (ntfs_inode *ni, ntfs_time_update_flags mask);
static ATTR_RECORD *get_unnamed_attr            (expand_t *expand, ATTR_TYPES type, s64 inum);
static uint64_t crc64                           (uint64_t crc, const byte * data, size_t size);
static BOOL verify_boot_sector                  (struct ntfs_device *dev, ntfs_volume *rawvol);
static void relocate_run                        (ntfs_resize_t *resize, runlist **rl, int run);
static struct fuse *mount_fuse                  (char *parsed_options, const char* mountPoint);
static int ntfs_fuse_utimens                    (const char *path, const struct timespec tv[2]);
static int add_attr_sd                          (MFT_RECORD *m, const u8 *sd, const s64 sd_len);
static int read_all                             (struct ntfs_device *dev, void *buf, int count);
static int write_all                            (struct ntfs_device *dev, void *buf, int count);
static void set_bitmap_range                    (struct bitmap *bm, s64 pos, s64 length, u8 bit);
static s64 mkntfs_bitmap_write                  (struct ntfs_device *dev, s64 offset, s64 length);
static BOOL mkntfs_parse_long                   (const char *string, const char *name, long *num);
static void relocate_clusters                   (ntfs_resize_t *r, runlist *dest_rl, s64 src_lcn);
static int ntfs_fuse_release                    (const char *org_path, struct fuse_file_info *fi);
static int make_room_for_index_entry_in_index_block (INDEX_BLOCK *idx, INDEX_ENTRY *pos, u32 size);
static int ntfs_fuse_readlink                   (const char *org_path, char *buf, size_t buf_size);
static void copy_clusters                       (ntfs_resize_t *resize, s64 dest, s64 src, s64 len);
static int ntfs_fuse_bmap                       (const char *path, size_t blocksize, uint64_t *idx);
static int fix_xattr_prefix                     (const char *name, int namespace, ntfschar **lename);
static int write_mft_record                     (ntfs_volume *v, const MFT_REF mref, MFT_RECORD *buf);
static int ntfs_fuse_open                       (const char *org_path,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		struct fuse_file_info *fi);
#else
		struct fuse_file_info *fi __attribute__((unused)));
#endif

#if defined(__APPLE__) || defined(__DARWIN__)
static int ntfs_fuse_getxattr(const char *path, const char *name, char *value, size_t size, uint32_t position);
#else
static int ntfs_fuse_getxattr(const char *path, const char *name, char *value, size_t size);
#endif

#if defined(__APPLE__) || defined(__DARWIN__)
static int ntfs_fuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t position);
#else
static int ntfs_fuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
#endif

static int ntfs_fuse_trunc(const char *org_path, off_t size,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			BOOL chkwrite);
#else
			BOOL chkwrite __attribute__((unused)));
#endif


static gpointer mount_fs_thread                 (gpointer data);
static void relocate_inode                      (ntfs_resize_t *resize, MFT_REF mref, int do_mftdata);
static ATTR_REC *check_attr_record              (ATTR_REC *attr_rec, MFT_RECORD *mft_rec, u16 buflen);
static int index_obj_id_insert                  (MFT_RECORD *m, const GUID *guid, const leMFT_REF ref);
static BOOL mkntfs_parse_llong                  (const char *string, const char *name, long long *num);
static void replace_later                       (ntfs_resize_t *resize, runlist *rl, runlist *head_rl);
static void progress_init                       (struct progress_bar *p, u64 start, u64 stop, int flags);
static void realloc_bitmap_data_attr            (ntfs_resize_t *resize, runlist **rl, s64 nr_bm_clusters);
static long long mkntfs_write                   (struct ntfs_device *dev, const void *b, long long count);
static int ntfs_fuse_create_file                (const char *path, mode_t mode, struct fuse_file_info *fi);
static int ntfs_fuse_parse_path                 (const char *org_path, char **path, ntfschar **stream_name);
static ntfs_volume *get_volume_data             (expand_t *expand, struct ntfs_device *dev, s32 sector_size);
static runlist *alloc_cluster                   (struct bitmap *bm, s64 items, s64 nr_vol_clusters, int hint);
static int replace_runlist                      (ntfs_attr *na, const runlist_element *reprl, VCN lowest_vcn);
static int ntfs_fuse_safe_rename                (const char *old_path, const char *new_path, const char *tmp);
static int ntfs_fuse_getxattr_windows           (const char *path, const char *name, char *value, size_t size);
static int add_attr_std_info                    (MFT_RECORD *m, const FILE_ATTR_FLAGS flags, le32 security_id);
static int ntfs_fuse_statfs                     (const char *path __attribute__((unused)), struct statvfs *sfs);
static ATTR_RECORD *find_attr                   (MFT_RECORD *mrec, ATTR_TYPES type, ntfschar *name, int namelen);
static int update_runlist                       (expand_t *expand, s64 inum, ATTR_RECORD *a, runlist_element *rl);
BOOL user_xattrs_allowed                        (ntfs_fuse_context_t *ctx __attribute__((unused)), ntfs_inode *ni);
static int rebase_inode                         (expand_t *expand, const runlist_element *prl, s64 inum, s64 jnum);
static int wsl_getattr                          (ntfs_inode *ni, const REPARSE_POINT *reparse, struct stat *stbuf);
static BOOL mkntfs_sync_index_record            (INDEX_ALLOCATION* idx, MFT_RECORD* m, ntfschar* name, u32 name_len);
static int ntfs_fuse_rm_stream                  (const char *path, ntfschar *stream_name, const int stream_name_len);
static int find_free_cluster                    (struct bitmap *bm, runlist_element *rle, s64 nr_vol_clusters, int hint);
static s64 mkntfs_logfile_write                 (struct ntfs_device *dev, s64 offset __attribute__((unused)), s64 length);
static int ntfs_fuse_mknod_common               (const char *org_path, mode_t mode, dev_t dev, struct fuse_file_info *fi);
static ATTR_RECORD *read_and_get_attr           (expand_t *expand, ATTR_TYPES type, s64 inum, ntfschar *name, int namelen);
static void delayed_expand                      (ntfs_volume *vol, struct DELAYED *delayed, struct progress_bar *progress);
static ntfs_inode *ntfs_check_access_xattr      (struct SECURITY_CONTEXT *security, const char *path, int attr, BOOL setting);
static BOOL create_file_volume                  (MFT_RECORD *m, leMFT_REF root_ref, VOLUME_FLAGS fl, const GUID *volume_guid);
static void lookup_data_attr                    (ntfs_volume *vol, MFT_REF mref, const char *aname, ntfs_attr_search_ctx **ctx);
const struct plugin_operations *select_reparse_plugin(ntfs_fuse_context_t *ctx, ntfs_inode *ni, REPARSE_POINT **reparse_wanted);
static int add_attr_vol_info                    (MFT_RECORD *m, const VOLUME_FLAGS flags, const u8 major_ver, const u8 minor_ver);
int register_reparse_plugin                     (ntfs_fuse_context_t *ctx, le32 tag, const plugin_operations_t *ops, void *handle);
static int junction_readlink                    (ntfs_inode *ni, const REPARSE_POINT *reparse __attribute__((unused)), char **pbuf);
int ntfs_fuse_listxattr_common                  (ntfs_inode *ni, ntfs_attr_search_ctx *actx, char *list, size_t size, BOOL prefixing);
static int insert_file_link_in_dir_index        (INDEX_BLOCK *idx, leMFT_REF file_ref, FILE_NAME_ATTR *file_name, u32 file_name_size);
static int add_attr_vol_name                    (MFT_RECORD *m, const char *vol_name, const int vol_name_len __attribute__((unused)));
static int ntfs_fuse_ftruncate                  (const char *org_path, off_t size, struct fuse_file_info *fi __attribute__((unused)));
static int ntfs_index_keys_compare              (u8 *key1, u8 *key2, int key1_length, int key2_length, COLLATION_RULES collation_rule);
static int ntfs_allowed_real_dir_access         (struct SECURITY_CONTEXT *scx, const char *path, ntfs_inode *dir_ni, mode_t accesstype);
static int junction_getattr                     (ntfs_inode *ni, const REPARSE_POINT *reparse __attribute__((unused)), struct stat *stbuf);
static int insert_index_entry_in_res_dir_index  (INDEX_ENTRY *idx, u32 idx_size, MFT_RECORD *m, ntfschar *name, u32 name_size, ATTR_TYPES type);
static runlist *load_runlist                    (ntfs_volume *rawvol, s64 offset_to_file_record, ATTR_TYPES attr_type, u32 size_of_file_record);
static int ntfs_fuse_create_stream              (const char *path, ntfschar *stream_name, const int stream_name_len, struct fuse_file_info *fi);
static int ntfs_fuse_create                     (const char *org_path, mode_t typemode, dev_t dev, const char *target, struct fuse_file_info *fi);
static int upgrade_to_large_index               (MFT_RECORD *m, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, INDEX_ALLOCATION **idx);
static int ntfs_fuse_read                       (const char *org_path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi __attribute__((unused)));
static int ntfs_fuse_write                      (const char *org_path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi __attribute__((unused)));
static s64 ntfs_rlwrite                         (struct ntfs_device *dev, const runlist *rl, const u8 *val, const s64 val_len, s64 *inited_size, WRITE_TYPE write_type);
static int add_attr_bitmap                      (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *bitmap, const u32 bitmap_len);
static int ntfs_fuse_ioctl                      (const char *path, int cmd, void *arg, struct fuse_file_info *fi __attribute__((unused)), unsigned int flags, void *data);
static int ntfs_fuse_fsync                      (const char *path __attribute__((unused)), int type __attribute__((unused)), struct fuse_file_info *fi __attribute__((unused)));
static int add_attr_data                        (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const u8 *val, const s64 val_len);
static int add_attr_index_alloc                 (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *index_alloc_val, const u32 index_alloc_val_len);
static int add_attr_bitmap_positioned           (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const runlist *rl, const u8 *bitmap, const u32 bitmap_len);
static int ntfs_fuse_readdir                    (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset __attribute__((unused)), struct fuse_file_info *fi __attribute__((unused)));
static int mkntfs_attr_find                     (const ATTR_TYPES type, const ntfschar *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx);
static int add_attr_data_positioned             (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len);
static int insert_positioned_attr_in_mft_record (MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len);
static int insert_non_resident_attr_in_mft_record(MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const u8 *val, const s64 val_len, WRITE_TYPE write_type);
static int add_attr_index_root                  (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_TYPES indexed_attr_type, const COLLATION_RULES collation_rule, const u32 index_block_size);
static int ntfs_fuse_filler(ntfs_fuse_fill_context_t *fill_ctx, const ntfschar *name, const int name_len, const int name_type, const s64 pos __attribute__((unused)), const MFT_REF mref, const unsigned dt_type __attribute__((unused)));
static int insert_resident_attr_in_mft_record   (MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const RESIDENT_ATTR_FLAGS res_flags, const u8 *val, const u32 val_len);
static int mkntfs_attr_lookup                   (const ATTR_TYPES type, const ntfschar *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const VCN lowest_vcn __attribute__((unused)), const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx);
static int add_attr_file_name                   (MFT_RECORD *m, const leMFT_REF parent_dir, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type);
static int create_hardlink                      (INDEX_BLOCK *idx, const leMFT_REF ref_parent, MFT_RECORD *m_file, const leMFT_REF ref_file, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type);
static int create_hardlink_res                  (MFT_RECORD *m_parent, const leMFT_REF ref_parent, MFT_RECORD *m_file, const leMFT_REF ref_file, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type);


static void     umount_signal_process           (int signum);


static cuint8*                                  gsBuf                   = NULL;         // 每次操作写入大小，块/簇，默认 27K
static int                                      gsMftBitmapByteSize     = 0;            // 默认 8。
static u8*                                      gsMftBitmap             = NULL;         // 默认：长度8的buffer。
static int                                      gsLcnBitmapByteSize     = 0;            // 磁盘所有簇对应的位图（一个簇4096），必须是8的倍数
static runlist*                                 gsRlMft                 = NULL;         // MFT runlist...
static runlist*                                 gsRlMftBmp              = NULL;         // MFT runlist，默认 2 个。
static runlist*                                 gsRlMftmirr             = NULL;         // MFT runlist 备份
static runlist*                                 gsRlLogfile             = NULL;         // logfile runlist
static runlist*                                 gsRlBoot                = NULL;         // boot runlist
static runlist*                                 gsRlBad                 = NULL;         // 坏 簇/块 runlist
static INDEX_ALLOCATION*                        gsIndexBlock            = NULL;
static ntfs_volume*                             gsVol                   = NULL;
static struct UPCASEINFO*                       gsUpcaseInfo            = NULL;         // 大小转换信息
static int                                      gsDynamicBufSize        = 0;            // 4096，一页内存大小
static u8*                                      gsDynamicBuf            = NULL;         // ???
static int                                      gsMftSize               = 0;            // g_buf 变量长度，默认 27KB。
static long long                                gsMftLcn                = 0;            /* > 16kib，存储了NTFS中主文件表的逻辑簇号。lcn(Logical Cluster Number) of $MFT, $DATA attribute */
static long long                                gsMftmirrLcn            = 0;            /* lcn of $MFTMirr, $DATA */
static long long                                gsLogfileLcn            = 0;            /* 紧随 MFT备份的runlist后。lcn of $LogFile, $DATA */
static int                                      gsLogfileSize           = 0;            /* logfile 大小。in bytes, determined from volume_size */
static long long                                gsMftZoneEnd            = 0;            /* 默认：块总数/8（12.5%）。Determined from volume_size and mft_zone_multiplier, in clusters */
static long long                                gsNumBadBlocks          = 0;            /* Number of bad clusters */
static long long*                               gsBadBlocks             = NULL;         /* Array of bad clusters */
static struct BITMAP_ALLOCATION*                gsAllocation            = NULL;         /* 簇/块的起始地址。Head of cluster allocations */

// check
static int                                      gsErrors                = 0;
static int                                      gsUnsupported           = 0;
static s64                                      gsCurrentMftRecord      = 0;
static short                                    gsBytesPerSector        = 0;
static short                                    gsSectorsPerCluster     = 0;
static u32                                      gsMftBitmapRecords      = 0;
static u8*                                      gsMftBitmapBuf          = NULL;
static runlist_element*                         gsMftRl                 = NULL;
static runlist_element*                         gsMftBitmapRl           = NULL;

static s64                                      max_free_cluster_range  = 0;
static ntfs_fuse_context_t*                     ctx                     = NULL;
static u32                                      ntfs_sequence           = 0;

guint64 gVolumeSize = 0;
// format -- start

static struct _MkfsOpt
{
    long                heads;
    long                sectorSize;
    long long           numSectors;
    long                clusterSize;
    long long           partStartSect;
    long                sectorsPerTrack;
    long                mftZoneMultiplier;
} opts = {
    .heads              = -1,
    .sectorSize         = -1,
    .numSectors         = -1,
    .clusterSize        = -1,
    .partStartSect      = -1,
    .sectorsPerTrack    = -1,
    .mftZoneMultiplier  = -1,
};
// format -- end

static struct fuse_operations ntfs_3g_ops = {
    .getattr	= ntfs_fuse_getattr,
    .readlink	= ntfs_fuse_readlink,
    .readdir	= ntfs_fuse_readdir,
    .open		= ntfs_fuse_open,
    .release	= ntfs_fuse_release,
    .read		= ntfs_fuse_read,
    .write		= ntfs_fuse_write,
    .truncate	= ntfs_fuse_truncate,
    .ftruncate	= ntfs_fuse_ftruncate,
    .statfs		= ntfs_fuse_statfs,
    .chmod		= ntfs_fuse_chmod,
    .chown		= ntfs_fuse_chown,
    .create		= ntfs_fuse_create_file,
    .mknod		= ntfs_fuse_mknod,
    .symlink	= ntfs_fuse_symlink,
    .link		= ntfs_fuse_link,
    .unlink		= ntfs_fuse_unlink,
    .rename		= ntfs_fuse_rename,
    .mkdir		= ntfs_fuse_mkdir,
    .rmdir		= ntfs_fuse_rmdir,
#ifdef HAVE_UTIMENSAT
    .utimens	= ntfs_fuse_utimens,
#if defined(linux) & !defined(FUSE_INTERNAL) & (FUSE_VERSION < 30)
    .flag_utime_omit_ok = 1,
#endif /* defined(linux) & !defined(FUSE_INTERNAL) */
#else
    .utime		= ntfs_fuse_utime,
#endif
    .fsync		= ntfs_fuse_fsync,
    .fsyncdir	= ntfs_fuse_fsync,
    .bmap		= ntfs_fuse_bmap,
    .destroy        = ntfs_fuse_destroy2,
#if defined(FUSE_INTERNAL) || (FUSE_VERSION >= 28)
        .ioctl		= ntfs_fuse_ioctl,
#endif /* defined(FUSE_INTERNAL) || (FUSE_VERSION >= 28) */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    .access		= ntfs_fuse_access,
    .opendir	= ntfs_fuse_opendir,
    .releasedir	= ntfs_fuse_release,
#endif
#ifdef HAVE_SETXATTR
    .getxattr	= ntfs_fuse_getxattr,
    .setxattr	= ntfs_fuse_setxattr,
    .removexattr	= ntfs_fuse_removexattr,
    .listxattr	= ntfs_fuse_listxattr,
#endif /* HAVE_SETXATTR */
#if defined(__APPLE__) || defined(__DARWIN__)
    /* MacFUSE extensions. */
    .getxtimes	= ntfs_macfuse_getxtimes,
    .setcrtime	= ntfs_macfuse_setcrtime,
    .setbkuptime	= ntfs_macfuse_setbkuptime,
    .setchgtime	= ntfs_macfuse_setchgtime,
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
    .init		= ntfs_init
};

const char xattr_ntfs_3g[] = "ntfs-3g.";
const char nf_ns_user_prefix[] = "user.";
const int nf_ns_user_prefix_len = sizeof(nf_ns_user_prefix) - 1;
const char nf_ns_system_prefix[] = "system.";
const int nf_ns_system_prefix_len = sizeof(nf_ns_system_prefix) - 1;
const char nf_ns_security_prefix[] = "security.";
const int nf_ns_security_prefix_len = sizeof(nf_ns_security_prefix) - 1;
const char nf_ns_trusted_prefix[] = "trusted.";
const int nf_ns_trusted_prefix_len = sizeof(nf_ns_trusted_prefix) - 1;

static const char nf_ns_alt_xattr_efsinfo[] = "user.ntfs.efsinfo";

static const char def_opts[] = "allow_other,nonempty,";

	/*
	 *	 Table of recognized options
	 * Their order may be significant
	 * The options invalid in some configuration should still
	 * be present, so that an error can be returned
	 */
const struct DEFOPTION optionlist[] = {
	{ "ro", OPT_RO, FLGOPT_APPEND | FLGOPT_BOGUS },
	{ "noatime", OPT_NOATIME, FLGOPT_BOGUS },
	{ "atime", OPT_ATIME, FLGOPT_BOGUS },
	{ "relatime", OPT_RELATIME, FLGOPT_BOGUS },
	{ "delay_mtime", OPT_DMTIME, FLGOPT_DECIMAL | FLGOPT_OPTIONAL },
	{ "rw", OPT_RW, FLGOPT_BOGUS },
	{ "fake_rw", OPT_FAKE_RW, FLGOPT_BOGUS },
	{ "fsname", OPT_FSNAME, FLGOPT_NOSUPPORT },
	{ "no_def_opts", OPT_NO_DEF_OPTS, FLGOPT_BOGUS },
	{ "default_permissions", OPT_DEFAULT_PERMISSIONS, FLGOPT_BOGUS },
	{ "permissions", OPT_PERMISSIONS, FLGOPT_BOGUS },
	{ "acl", OPT_ACL, FLGOPT_BOGUS },
	{ "umask", OPT_UMASK, FLGOPT_OCTAL },
	{ "fmask", OPT_FMASK, FLGOPT_OCTAL },
	{ "dmask", OPT_DMASK, FLGOPT_OCTAL },
	{ "uid", OPT_UID, FLGOPT_DECIMAL },
	{ "gid", OPT_GID, FLGOPT_DECIMAL },
	{ "show_sys_files", OPT_SHOW_SYS_FILES, FLGOPT_BOGUS },
	{ "hide_hid_files", OPT_HIDE_HID_FILES, FLGOPT_BOGUS },
	{ "hide_dot_files", OPT_HIDE_DOT_FILES, FLGOPT_BOGUS },
	{ "ignore_case", OPT_IGNORE_CASE, FLGOPT_BOGUS },
	{ "windows_names", OPT_WINDOWS_NAMES, FLGOPT_BOGUS },
	{ "compression", OPT_COMPRESSION, FLGOPT_BOGUS },
	{ "nocompression", OPT_NOCOMPRESSION, FLGOPT_BOGUS },
	{ "silent", OPT_SILENT, FLGOPT_BOGUS },
	{ "recover", OPT_RECOVER, FLGOPT_BOGUS },
	{ "norecover", OPT_NORECOVER, FLGOPT_BOGUS },
	{ "remove_hiberfile", OPT_REMOVE_HIBERFILE, FLGOPT_BOGUS },
	{ "sync", OPT_SYNC, FLGOPT_BOGUS | FLGOPT_APPEND },
	{ "big_writes", OPT_BIG_WRITES, FLGOPT_BOGUS },
	{ "locale", OPT_LOCALE, FLGOPT_STRING },
	{ "nfconv", OPT_NFCONV, FLGOPT_BOGUS },
	{ "nonfconv", OPT_NONFCONV, FLGOPT_BOGUS },
	{ "streams_interface", OPT_STREAMS_INTERFACE, FLGOPT_STRING },
	{ "user_xattr", OPT_USER_XATTR, FLGOPT_BOGUS },
	{ "noauto", OPT_NOAUTO, FLGOPT_BOGUS },
	{ "debug", OPT_DEBUG, FLGOPT_BOGUS },
	{ "no_detach", OPT_NO_DETACH, FLGOPT_BOGUS },
	{ "remount", OPT_REMOUNT, FLGOPT_BOGUS },
	{ "blksize", OPT_BLKSIZE, FLGOPT_STRING },
	{ "inherit", OPT_INHERIT, FLGOPT_BOGUS },
	{ "addsecurids", OPT_ADDSECURIDS, FLGOPT_BOGUS },
	{ "staticgrps", OPT_STATICGRPS, FLGOPT_BOGUS },
	{ "usermapping", OPT_USERMAPPING, FLGOPT_STRING },
	{ "xattrmapping", OPT_XATTRMAPPING, FLGOPT_STRING },
	{ "efs_raw", OPT_EFS_RAW, FLGOPT_BOGUS },
	{ "posix_nlink", OPT_POSIX_NLINK, FLGOPT_BOGUS },
	{ "special_files", OPT_SPECIAL_FILES, FLGOPT_STRING },
	{ "--help", OPT_HELP, FLGOPT_BOGUS },
	{ "-h", OPT_HELP, FLGOPT_BOGUS },
	{ "--version", OPT_VERSION, FLGOPT_BOGUS },
	{ "-V", OPT_VERSION, FLGOPT_BOGUS },
	{ (const char*)NULL, 0, 0 } /* end marker */
};

pid_t mountPid = 0;
struct fuse* gsFuse;


SandboxFs* sandbox_fs_init(const char* devPath, const char* mountPoint)
{
    SandboxFs* sfs = NULL;

    bool hasErr = false;
    SANDBOX_FS_MUTEX_LOCK();
    do {
        sfs = g_malloc0 (sizeof(SandboxFs));
        if (!sfs) {
            hasErr = true;
            C_LOG_WARNING("sandbox malloc error.");
            break;
        }

        if (devPath) {
            if (sfs->dev) { g_free(sfs->dev); }
            sfs->dev = g_strdup (devPath);
            if (!sfs->dev) {
                hasErr = true;
            }
        }

        if (mountPoint) {
            if (sfs->mountPoint) { g_free(sfs->mountPoint); }
            sfs->mountPoint = g_strdup (mountPoint);
            if (!sfs->mountPoint) {
                hasErr = true;
            }
        }
    } while (0);

    if (hasErr) { sandbox_fs_destroy(&sfs); }
    SANDBOX_FS_MUTEX_UNLOCK();

    return sfs;
}

bool sandbox_fs_set_dev_name(SandboxFs* sandboxFs, const char* devName)
{
    g_return_val_if_fail(sandboxFs && devName, false);

    bool hasErr = false;

    SANDBOX_FS_MUTEX_LOCK();
    do {
        if (devName) {
            if (sandboxFs->dev) {
                g_free(sandboxFs->dev);
                sandboxFs->dev = NULL;
            }
            sandboxFs->dev = g_strdup (devName);
            if (!sandboxFs->dev) {
                hasErr = true;
                break;
            }
        }
    } while (0);
    SANDBOX_FS_MUTEX_UNLOCK();

    return !hasErr;
}

bool sandbox_fs_set_mount_point(SandboxFs * sandboxFs, const char * mountPoint)
{
    g_return_val_if_fail(sandboxFs && mountPoint, false);

    bool hasErr = false;

    SANDBOX_FS_MUTEX_LOCK();

    do {
        if (mountPoint) {
            if (sandboxFs->mountPoint) {
                g_free(sandboxFs->mountPoint);
                sandboxFs->mountPoint = NULL;
            }
            sandboxFs->mountPoint = g_strdup (mountPoint);
            if (!sandboxFs->mountPoint) {
                hasErr = true;
                break;
            }
        }
    } while (false);

    SANDBOX_FS_MUTEX_UNLOCK();

    return !hasErr;
}

bool sandbox_fs_generated_box (const SandboxFs* sandboxFs, cuint64 sizeMB)
{
    c_return_val_if_fail(sandboxFs && sandboxFs->dev && (sandboxFs->dev[0] == '/') && (sizeMB > 0), false);

    int fd = -1;
    bool hasError = false;

    SANDBOX_FS_MUTEX_LOCK();

    char* dirPath = g_strdup(sandboxFs->dev);
    if (dirPath) {
        char* dir = c_strrstr(dirPath, "/");
        if (dir) {
            *dir = '\0';
        }
        C_LOG_VERB("dir: %s", dirPath);

        if (!c_file_test(dirPath, C_FILE_TEST_EXISTS)) {
            if (0 != c_mkdir_with_parents(dirPath, 0755)) {
                C_LOG_VERB("mkdir_with_parents: '%s' error.", dirPath);
                hasError = true;
            }
        }
        c_free0(dirPath);
    }

    if (hasError) { goto out; }

    errno = 0;
    fd = open(sandboxFs->dev, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        hasError = true;
        C_LOG_WARNING("open: '%s' error: %s", sandboxFs->dev, c_strerror(errno));
        goto out;
    }

    if (lseek(fd, 0, SEEK_END) > 0) {
        goto out;
    }

    do {
        cuint64 needSize = 1024 * 1024 * sizeMB;
        needSize = align_4096(needSize);

        errno = 0;
        off_t ret = lseek(fd, needSize - 1, SEEK_SET);
        if (ret < 0) {
            C_LOG_VERB("lseek: '%s' error: %s", sandboxFs->dev, c_strerror(errno));
            hasError = true;
            break;
        }

        if (-1 == write(fd, "", 1)) {
            C_LOG_VERB("write: '%s' error: %s", sandboxFs->dev, c_strerror(errno));
            hasError = true;
            break;
        }
        c_fsync(fd);
    } while (false);

out:

    // close
    if (fd >= 0) {
        CError* error = NULL;
        c_close(fd, &error);
        if (error) {
            hasError = true;
            C_LOG_WARNING("close: '%s' error: %s", sandboxFs->dev, error->message);
            c_error_free(error);
        }
    }

    SANDBOX_FS_MUTEX_UNLOCK();

    return !hasError;
}

bool sandbox_fs_format(SandboxFs* sandboxFs)
{
    c_return_val_if_fail(sandboxFs && sandboxFs->dev, false);

    // set locale
    const char* locale = setlocale(LC_ALL, NULL);
    if (!locale) {
        locale = setlocale(LC_CTYPE, NULL);
        C_LOG_VERB("setlocale: '%s' error", locale ? locale : "NULL");
        return false;
    }

    //
    long long               lw = 0;
    long long               pos = 0;
    ntfs_attr_search_ctx*   ctx = NULL;
    cuint64                 upCaseCrc = 0;

    /**
     * ATTR：文件和目录都包含各种属性信息，这些信息都保存在文件的属性中，
     *       每个文件 或 目录 最多可以有 255 个属性。
     *       常见属性包括标准信息属性、扩展属性、数据流属性等。
     * MFT：是NTFS文件系统中最终要的数据结构之一，它记录了文件系统中所有文件和目录的信息。
     *       MFT是一个巨大的表，每个文件或目录在MFT中都有一个对应的记录
     *       MFT记录存储了文件或目录的各种元数据，如：文件名、属性、文件大小、文件位置等
     *       MFT记录还包括了文件的实际数据，对于小文件来说，它们的内容直接存储在MFT中
     *       MFT通常为1KB，但也可以根据需要进行扩展
     *
     * ATTR 和 MFT 这两个结构都是文件和目录的属性信息。
     */
    int                     i = 0;
    int                     err = 0;
    ATTR_RECORD*            a = NULL;
    MFT_RECORD*             m = NULL;
    bool                    hasError = false;

    srandom(sle64_to_cpu(mkntfs_time()) / 10000000);

    SANDBOX_FS_MUTEX_LOCK();

    // 单纯的分配内存
    gsVol = ntfs_volume_alloc();
    if (!gsVol) {
        C_LOG_ERROR("Could not create volume");
        hasError = true;
        goto done;
    }

    // version
    gsVol->major_ver = 3;
    gsVol->minor_ver = 1;

    /**
     * 支持的 unicode 字符
     * $UpCase
     *
     * FS区分大小写，使用 gsVol->upcase 提供高效的大小写转换操作
     */
    gsVol->upcase_len = ntfs_upcase_build_default(&gsVol->upcase);

    gsUpcaseInfo = (struct UPCASEINFO*)ntfs_malloc(sizeof(struct UPCASEINFO));
    if (!gsVol->upcase_len || !gsUpcaseInfo) {
        hasError = true;
        goto done;
    }

    /* If the CRC is correct, chkdsk does not warn about obsolete table */
    crc64(0, (byte*) NULL, 0); /* initialize the crc computation */
    upCaseCrc = crc64(0, (byte*) gsVol->upcase, gsVol->upcase_len * sizeof(ntfschar));

    /* keep the version fields as zero */
    memset(gsUpcaseInfo, 0, sizeof(struct UPCASEINFO));
    gsUpcaseInfo->len = const_cpu_to_le32(sizeof(struct UPCASEINFO));
    gsUpcaseInfo->crc = cpu_to_le64(upCaseCrc);

    /* attrdef_ntfs3x_array */
    gsVol->attrdef = ntfs_malloc(sizeof(attrdef_ntfs3x_array)); // 2560 长的数组
    if (!gsVol->attrdef) {
        C_LOG_WARNING("Could not create attrdef structure");
        hasError = true;
        goto done;
    }
    memcpy(gsVol->attrdef, attrdef_ntfs3x_array, sizeof(attrdef_ntfs3x_array));
    gsVol->attrdef_len = sizeof(attrdef_ntfs3x_array);

    if (!mkntfs_open_partition(gsVol, sandboxFs->dev)) {
        C_LOG_ERROR("Could not open partition");
        hasError = true;
        goto done;
    }

    if (!mkntfs_override_vol_params(gsVol)) {
        C_LOG_ERROR("Could not override partition");
        hasError = true;
        goto done;
    }

    /**
     * @note 文件系统尾部预留10KB的大小，实际使用8KB，关键信息放在中间8KB处
     */
    C_LOG_INFO("Start write efs header...");
    if (!mkntfs_init_sandbox_header(gsVol)) {
        C_LOG_ERROR("Could not initialize efs header");
        hasError = true;
        goto done;
    }
    C_LOG_INFO("Write efs header OK!");

#if 0
    printf("g_vol:\n");
    printf("  ntfs_device:\n");
    printf("    d_state            : %lu\n", g_vol->dev->d_state);
    printf("    d_name             : %s\n", g_vol->dev->d_name);
    printf("    d_heads            : %d\n", g_vol->dev->d_heads);
    printf("    d_sectors_per_track: %d\n", g_vol->dev->d_sectors_per_track);
    printf("  vol_name : %s\n", g_vol->vol_name);
    printf("  state    : %lu\n", g_vol->state);
    printf("  ntfs_inode\n");
    printf("    mft_no             : %lu\n", g_vol->vol_ni->mft_no);
    printf("    MFT_RECORD\n");
    printf("    ntfs_volume\n");
    printf("    state              : %lu\n", g_vol->vol_ni->state);
    printf("    attr_list_size     : %lu\n", g_vol->vol_ni->attr_list_size);
    printf("    data_size          : %d\n", g_vol->vol_ni->data_size);
    printf("    allocated_size     : %d\n", g_vol->vol_ni->allocated_size);

#endif

    // MFT 所在 簇/块 bitmap 存储分配
    if (!mkntfs_initialize_bitmaps()) {
        C_LOG_ERROR("Could not initialize bitmaps");
        hasError = true;
        goto done;
    }

    // MFT runlist、MFT 备份的 runlist、logfile 分配
    if (!mkntfs_initialize_rl_mft()) {
        C_LOG_ERROR("Could not initialize rl_mft");
        hasError = true;
        goto done;
    }

    /**
     * 负责初始化 NTFS 日志文件的运行列表(Runlist)
     *
     * 和 MFT 文件一样，NTFS 日志文件的数据也可能被分散存储在磁盘的多个不连续位置(簇)上。
     * 此函数作用是初始化这个日志文件的运行列表，它会分配一个初始的连续磁盘空间，并将其记录到日志文件的运行列表中。
     * 这个步骤确保了日志文件在刚创建时有一个良好的磁盘分布，有助于提高日志文件的访问性能
     * 日志文件在文件系统的正常运行过程中扮演着非常重要的角色。它记录了文件创建、删除、修改等关键元数据操作的历史，在文件系统崩溃恢复时候起到关键作用
     */
    if (!mkntfs_initialize_rl_logfile()) {
        C_LOG_ERROR("Could not initialize rl_logfile");
        hasError = true;
        goto done;
    }

    /**
     * 负责初始化启动扇区(Boot Sector)的运行列表(Runlist)
     *
     * NTFS 中，启动扇区包含了关键的引导信息和文件系统元数据，是系统启动和文件系统识别的重要依据
     * 和 MFT 文件以及日志文件一样，启动扇区的数据也可能被分散存储在磁盘的多个不连续位置(簇)上
     * 初始化这个启动扇区的运行列表。它会分配一个初始的连续磁盘空间，并将其记录到启动扇区的运行列表中
     * 这个步骤确保了启动扇区在刚创建时候有一个良好的磁盘分布，有助于提高系统启动和文件系统挂载的性能和可靠性
     * 启动扇区包含了诸如文件系统版本、簇大小、MFT位置等关键信息。如果启动扇区的数据布局不合理，可能会导致文件系统无法正常挂载和启动
     */
    if (!mkntfs_initialize_rl_boot()) {
        C_LOG_ERROR("Could not initialize rl_boot");
        hasError = true;
        goto done;
    }

    /* Allocate a buffer large enough to hold the mft. */
    // 写入的基本单位，默认 27K
    gsBuf = ntfs_calloc(gsMftSize);
    if (!gsBuf) {
        C_LOG_ERROR("Could not allocate memory");
        hasError = true;
        goto done;
    }

    /**
     * 负责初始化“坏簇”区域的运行列表
     *
     * NTFS文件系统创建过程中会扫描磁盘，识别处那些有缺陷或损坏的磁盘簇(簇是NTFS的基本存储单元)
     * 这些被标记为“坏簇”的磁盘区域是不能用于存储数据的，必须从文件系统中隔离出来，以防止误用
     * 此函数将磁盘坏簇区域记录在一个专门的运行列表中
     */
    if (!mkntfs_initialize_rl_bad()) {
        C_LOG_ERROR("Could not initialize rl_bad");
        hasError = true;
        goto done;
    }

    /**
     * 负责创建文件系统根目录结构
     */
    if (!mkntfs_create_root_structures()) {
        C_LOG_ERROR("Could not create root structure");
        hasError = true;
        goto done;
    }

    /**
     * - Do not step onto bad blocks!!!
     * - If any bad blocks were specified or found, modify $BadClus,
     *   allocating the bad clusters in $Bitmap.
     * - C&w bootsector backup bootsector (backup in last sector of the
     *   partition).
     * - If NTFS 3.0+, c&w $Secure file and $Extend directory with the
     *   corresponding special files in it, i.e. $ObjId, $Quota, $Reparse,
     *   and $UsnJrnl. And others? Or not all necessary?
     * - RE: Populate $root with the system files (and $Extend directory if
     *   applicable). Possibly should move this as far to the top as
     *   possible and update during each subsequent c&w of each system file.
     */
    C_LOG_VERB("Syncing root directory index record.");
    if (!mkntfs_sync_index_record(gsIndexBlock, (MFT_RECORD*) (gsBuf + 5 * gsVol->mft_record_size), NTFS_INDEX_I30, 4)) {
        C_LOG_ERROR("Could not sync root directory index record");
        hasError = true;
        goto done;
    }

    C_LOG_VERB("Syncing $Bitmap.");
    m = (MFT_RECORD*)(gsBuf + 6 * gsVol->mft_record_size);

    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Could not create an attribute search context");
        hasError = true;
        goto done;
    }

    if (mkntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        C_LOG_WARNING("BUG: $DATA attribute not found.");
        hasError = true;
        goto done;
    }

    a = ctx->attr;
    if (a->non_resident) {
        runlist *rl = ntfs_mapping_pairs_decompress(gsVol, a, NULL);
        if (!rl) {
            C_LOG_WARNING("fs_mapping_pairs_decompress() failed");
            hasError = true;
            goto done;
        }
        lw = ntfs_rlwrite(gsVol->dev, rl, (const u8*)NULL, gsLcnBitmapByteSize, NULL, WRITE_BITMAP);
        err = errno;
        free(rl);
        if (lw != gsLcnBitmapByteSize) {
            C_LOG_WARNING("fs_rlwrite: %s", lw == -1 ? strerror(err) : "unknown error");
            hasError = true;
            goto done;
        }
    }
    else {
        /* Error : the bitmap must be created non resident */
        C_LOG_WARNING("Error : the global bitmap is resident");
        hasError = true;
        goto done;
    }

    /*
     * No need to sync $MFT/$BITMAP as that has never been modified since
     * its creation.
     */
    C_LOG_VERB("Syncing $MFT.");
    pos = gsMftLcn * gsVol->cluster_size;
    lw = 1;
    for (i = 0; i < gsMftSize / (s32)gsVol->mft_record_size; i++) {
        lw = ntfs_mst_pwrite(gsVol->dev, pos, 1, gsVol->mft_record_size, gsBuf + i * gsVol->mft_record_size);
        if (lw != 1) {
            C_LOG_WARNING("fs_mst_pwrite: %s", lw == -1 ? strerror(errno) : "unknown error");
            hasError = true;
            goto done;
        }
        pos += gsVol->mft_record_size;
    }
    C_LOG_VERB("Updating $MFTMirr.");
    pos = gsMftmirrLcn * gsVol->cluster_size;
    lw = 1;

    for (i = 0; i < gsRlMftmirr[0].length * gsVol->cluster_size / gsVol->mft_record_size; i++) {
        m = (MFT_RECORD*)(gsBuf + i * gsVol->mft_record_size);
        /*
         * Decrement the usn by one, so it becomes the same as the one
         * in $MFT once it is mst protected. - This is as we need the
         * $MFTMirr to have the exact same byte by byte content as
         * $MFT, rather than just equivalent meaning content.
         */
        if (ntfs_mft_usn_dec(m)) {
            C_LOG_WARNING("fs_mft_usn_dec");
            hasError = true;
            goto done;
        }

        lw = ntfs_mst_pwrite(gsVol->dev, pos, 1, gsVol->mft_record_size, gsBuf + i * gsVol->mft_record_size);

        if (lw != 1) {
            C_LOG_WARNING("fs_mst_pwrite: %s", lw == -1 ? strerror(errno) : "unknown error");
            hasError = true;
            goto done;
        }
        pos += gsVol->mft_record_size;
    }

    C_LOG_VERB("Syncing device.");
    if (gsVol->dev->d_ops->sync(gsVol->dev)) {
        C_LOG_WARNING("Syncing device. FAILED");
        hasError = true;
        goto done;
    }
    ntfs_log_quiet("mkntfs completed successfully. Have a nice day.");

done:
    ntfs_attr_put_search_ctx(ctx);
    mkntfs_cleanup();    /* Device is unlocked and closed here */

    SANDBOX_FS_MUTEX_UNLOCK();

    setlocale(LC_ALL, locale);

    return !hasError;
}

bool sandbox_fs_check(const SandboxFs* sandboxFs)
{
    c_return_val_if_fail(sandboxFs && sandboxFs->dev, false);

    ntfs_volume         rawvol;
    int                 ret = 0;
    ntfs_volume*        vol = NULL;
    struct ntfs_device* dev = NULL;
    bool                hasError = false;

    C_LOG_VERB("start lock!");

    SANDBOX_FS_MUTEX_LOCK();

    C_LOG_VERB("Checking if the volume exists. '%s'", sandboxFs->dev);
    dev = ntfs_device_alloc(sandboxFs->dev, 0, &ntfs_device_default_io_ops, NULL);
    if (!dev) {
        hasError = true;
        C_LOG_WARNING("ntfs_device_alloc() failed");
        goto end;
    }

    if (-1 >= dev->d_ops->open(dev, O_RDONLY)) {
        C_LOG_WARNING("Error opening partition device");
        ntfs_device_free(dev);
        hasError = true;
        goto end;
    }

    if (TRUE != (ret = verify_boot_sector(dev, &rawvol))) {
        dev->d_ops->close(dev);
        hasError = true;
        C_LOG_WARNING("Error verifying boot_sector");
        goto end;
    }
    C_LOG_VERB("Boot sector verification complete. Proceeding to $MFT");

    verify_mft_preliminary(&rawvol);

    /* ntfs_device_mount() expects the device to be closed. */
    if (0 != dev->d_ops->close(dev)) {
        C_LOG_WARNING("Failed to close the device.");
        hasError = true;
        goto end;
    }

    C_LOG_VERB("Start Mount...");

    // at this point we know that the volume is valid enough for mounting.
    /* Call ntfs_device_mount() to do the actual mount. */
    vol = ntfs_device_mount(dev, NTFS_MNT_RDONLY);
    if (!vol) {
        ntfs_device_free(dev);
        hasError = true;
        C_LOG_WARNING("Failed to mount the device.");
        goto end;
    }

    replay_log(vol);

    if (vol->flags & VOLUME_IS_DIRTY) {
        C_LOG_VERB("Volume is dirty.");
    }

    check_volume(vol);

    // check andsec efs header
    if (!check_efs_header(vol)) {
        hasError = true;
        C_LOG_WARNING("Checking if the volume is not a valid filesystem");
        goto end;
    }

    if (gsErrors) {
        hasError = true;
        C_LOG_WARNING("Errors found.");
    }

    if (gsUnsupported) {
        hasError = true;
        C_LOG_WARNING("Unsupported cases found.");
    }

    if (!gsErrors && !gsUnsupported) {
        reset_dirty(vol);
    }

    ntfs_umount(vol, FALSE);

    if (gsErrors) {
        C_LOG_WARNING("s");
        goto end;
    }

    if (gsUnsupported) {
        goto end;
    }

end:
    SANDBOX_FS_MUTEX_UNLOCK();

    return !hasError;
}

bool sandbox_fs_resize(SandboxFs* sandboxFs, cuint64 sizeMB)
{
    c_return_val_if_fail(sandboxFs && sandboxFs->dev, false);

    struct stat         st;
    int                 fd = 0;
    ntfs_resize_t       resize;
    ntfs_volume*        vol = NULL;
    bool                hasError = false;
    int64_t             newSize = sizeMB * 1024 * 1024;

    memset(&resize, 0, sizeof(resize));
    newSize = align_4096(newSize);

    SANDBOX_FS_MUTEX_LOCK();

    errno = 0;
    if (0 != stat(sandboxFs->dev, &st)) {
        C_LOG_ERROR("Failed to stat '%s', error: %s", sandboxFs->dev, strerror(errno));
        hasError = true;
        goto end;
    }

    if (st.st_size >= newSize) {
        C_LOG_ERROR("Unable to reduce device size. old: %ul, new: %ul", st.st_size, newSize);
        hasError = true;
        goto end;
    }

    if (0 != access(sandboxFs->dev, R_OK | W_OK)) {
        C_LOG_ERROR("Box '%s' not exists, or unable to write.", sandboxFs->dev);
        hasError = true;
        goto end;
    }

    errno = 0;
    fd = open(sandboxFs->dev, O_RDONLY|O_WRONLY);
    if (fd < 0) {
        C_LOG_ERROR("Failed to open '%s', error: %s", sandboxFs->dev, strerror(errno));
        hasError = true;
        goto end;
    }

    do {
        int64_t size = newSize - st.st_size;
        size = align_4096(size);
        if (lseek(fd, size - 1, SEEK_END) >= 0) {
            if (write(fd, "", 1) >= 0) {
                C_LOG_INFO("Successfully resized device to %lld bytes", size + st.st_size);
            }
            else {
                hasError = true;
                C_LOG_WARNING("Failed to resize device to %lld bytes", size + st.st_size);
                break;
            }
        }
        else {
            hasError = true;
            C_LOG_ERROR("Failed to resize device to %lld bytes", size + st.st_size);
            break;
        }
    } while (0);

    if (fd >= 0) {
        close(fd);
    }

    if (hasError) { goto end; }

    // check
    vol = mount_volume(sandboxFs->dev);
    if (NULL == vol) {
        C_LOG_WARNING("Fail to mount volume: %s", sandboxFs->dev);
        hasError = true;
        goto end;
    }

    do {
        s64 device_size = ntfs_device_size_get(vol->dev, vol->sector_size);
        C_LOG_VERB("device size: %ul, sector size: %ul", device_size, vol->sector_size);
        device_size *= vol->sector_size;
        C_LOG_VERB("device size: %ul", device_size);
        if (device_size <= 0) {
            hasError = true;
            C_LOG_WARNING("Couldn't get device size (%lld)!", (long long)device_size);
            break;
        }

        resize.vol = vol;
        resize.new_volume_size = newSize / vol->cluster_size;
        resize.badclusters = check_bad_sectors(vol);

        ntfsck_t fsck;
        memset(&fsck, 0, sizeof(fsck));
        NVolSetNoFixupWarn(vol);
        check_cluster_allocation(vol, &fsck);
        print_disk_usage(vol, fsck.inuse);

        resize.inuse = fsck.inuse;
        resize.lcn_bitmap = fsck.lcn_bitmap;
        resize.mirr_from = MIRR_OLD;

        set_resize_constraints(&resize);
        set_disk_usage_constraint(&resize);
        check_resize_constraints(&resize);
        prepare_volume_fixup(vol);

        if (resize.relocations) {
            relocate_inodes(&resize);
        }

        truncate_badclust_file(&resize);
        truncate_bitmap_file(&resize);
        delayed_updates(&resize);
        update_bootsector(&resize);
        C_LOG_VERB("fs resize syncing device ...");

        if (vol->dev->d_ops->sync(vol->dev) == -1) {
            C_LOG_WARNING("resize fsync");
            break;
        }
        C_LOG_VERB("Successfully resized sandboxFS on device '%s'.", vol->dev->d_name);
    } while (false);

end:
    if (resize.lcn_bitmap.bm) {
        free(resize.lcn_bitmap.bm);
    }

    if (vol) {
        ntfs_umount(vol,0);
    }

    SANDBOX_FS_MUTEX_UNLOCK();

    return (!hasError);
}

bool sandbox_fs_mount(SandboxFs* sandboxFs)
{
    g_return_val_if_fail(sandboxFs && sandboxFs->dev && sandboxFs->mountPoint, false);

    if (!sandbox_fs_check(sandboxFs)) {
        C_LOG_WARNING("Sandbox fs check failed");
        return false;
    }

    if (sandbox_fs_is_mounted(sandboxFs)) {
        if (!sandbox_fs_unmount(sandboxFs)) {
            C_LOG_WARNING("Sandbox fs unmounted failed");
            return false;
        }
    }

    errno = 0;
    pid_t pid = fork();
    switch (pid) {
        case -1: {
            C_LOG_WARNING("Fork failed: %s", strerror(errno));
            return false;
        }
        case 0: {
            // 子进程
            signal(SIGKILL, umount_signal_process);
            mount_fs_thread(sandboxFs);
            C_LOG_INFO("Filesystem exit");
            exit(0);
            break;
        }
        default: {
            mountPid = pid;
            break;
        }
    }

    return true;
}

bool sandbox_fs_is_mounted(SandboxFs * sandboxFs)
{
    g_return_val_if_fail(sandboxFs, false);

    SANDBOX_FS_MUTEX_LOCK();

    if (sandboxFs->isMounted) {
        SANDBOX_FS_MUTEX_UNLOCK();
        return true;
    }

    sandboxFs->isMounted = utils_check_is_mounted(sandboxFs->dev, sandboxFs->mountPoint);
    if (sandboxFs->isMounted) {
        C_LOG_WARNING("Not mounted!");
    }

    sandboxFs->isMounted = utils_check_is_mounted(sandboxFs->dev, NULL);
    if (sandboxFs->isMounted) {
        C_LOG_WARNING("Not mounted!");
    }

    bool ret = sandboxFs->isMounted;

    SANDBOX_FS_MUTEX_UNLOCK();

    return ret;
}

bool sandbox_fs_unmount()
{
    bool isOK = false;
    SANDBOX_FS_MUTEX_LOCK();

    if (gsFuse) {
        isOK = true;
        fuse_exit(gsFuse);
    }

    SANDBOX_FS_MUTEX_UNLOCK();

    return isOK;
}

void sandbox_fs_destroy(SandboxFs ** sandboxFs)
{
    g_return_if_fail(sandboxFs && *sandboxFs);

    SANDBOX_FS_MUTEX_LOCK();

    if ((*sandboxFs)->dev) {
        g_free((*sandboxFs)->dev);
    }

    if ((*sandboxFs)->mountPoint) {
        g_free((*sandboxFs)->mountPoint);
    }

    if (*sandboxFs) {
        g_free(*sandboxFs);
        *sandboxFs = NULL;
    }

    SANDBOX_FS_MUTEX_UNLOCK();
}

void sandbox_fs_execute_chroot(SandboxFs * sandboxFs, const char ** env, const char * exe)
{
    g_return_if_fail(sandboxFs && exe && env);
    C_LOG_INFO("cmd: '%s', mountpoint: '%s'", exe ? exe : "<null>", sandboxFs->mountPoint? sandboxFs->mountPoint : "<null>");

   // chdir
    errno = 0;
    if ( 0 != chdir(sandboxFs->mountPoint)) {
        C_LOG_ERROR("chdir error: %s", c_strerror(errno));
        return;
    }

    // chroot
    errno = 0;
    if ( 0 != chroot(sandboxFs->mountPoint)) {
        C_LOG_ERROR("chroot error: %s", c_strerror(errno));
        return;
    }

    // set env
    if (env) {
        for (int i = 0; env[i]; ++i) {
            char** arr = c_strsplit(env[i], "=", 2);
            if (c_strv_length(arr) != 2) {
                // C_LOG_ERROR("error ENV %s", env[i]);
                c_strfreev(arr);
                continue;
            }

            char* key = arr[0];
            char* val = arr[1];
            C_LOG_VERB("[ENV] set %s", key);
            c_setenv(key, val, true);
            c_strfreev(arr);
        }
    }

    // change user
    if (c_getenv("USER")) {
        errno = 0;
        do {
            struct passwd* pwd = getpwnam(c_getenv("USER"));
            if (!pwd) {
                C_LOG_ERROR("get struct passwd error: %s", c_strerror(errno));
                break;
            }
            if (!c_file_test(pwd->pw_dir, C_FILE_TEST_EXISTS)) {
                errno = 0;
                if (!c_file_test("/home", C_FILE_TEST_EXISTS)) {
                    c_mkdir("/home", 0755);
                }
                if (0 != c_mkdir(pwd->pw_dir, 0700)) {
                    C_LOG_ERROR("mkdir error: %s", c_strerror(errno));
                }
                chown(pwd->pw_dir, pwd->pw_uid, pwd->pw_gid);
            }

            if (pwd->pw_dir) {
                c_setenv("HOME", pwd->pw_dir, true);
            }

            setuid(pwd->pw_uid);
            seteuid(pwd->pw_uid);

            setgid(pwd->pw_gid);
            setegid(pwd->pw_gid);
        } while (0);
    }

#ifdef DEBUG
    cchar** envs = c_get_environ();
    for (int i = 0; envs[i]; ++i) {
        c_log_raw(C_LOG_LEVEL_VERB, "%s", envs[i]);
    }
#endif

    // run command
#define CHECK_AND_RUN(dir)                              \
do {                                                    \
    char* cmdPath = c_strdup_printf("%s/%s", dir, exe); \
    C_LOG_VERB("Found cmd: '%s'", cmdPath);             \
    if (c_file_test(cmdPath, C_FILE_TEST_EXISTS)) {     \
        C_LOG_VERB("run cmd: '%s'", cmdPath);           \
        errno = 0;                                      \
        execvpe(cmdPath, NULL, env);                    \
        if (0 != errno) {                               \
            C_LOG_ERROR("execute cmd '%s' error: %s",   \
                cmdPath, c_strerror(errno));            \
            return;                                     \
        }                                               \
    }                                                   \
    c_free(cmdPath);                                    \
} while (0); break

    if (exe[0] == '/') {
        C_LOG_VERB("run cmd: '%s'", exe);
        errno = 0;
        execvpe(exe, NULL, env);
        if (0 != errno) { C_LOG_ERROR("execute cmd '%s' error: %s", exe, c_strerror(errno)); return; }
    }
    else {
        do {
            CHECK_AND_RUN("/bin");
            CHECK_AND_RUN("/usr/bin");
            CHECK_AND_RUN("/usr/local/bin");

            CHECK_AND_RUN("/sbin");
            CHECK_AND_RUN("/usr/sbin");
            CHECK_AND_RUN("/usr/local/sbin");

            C_LOG_ERROR("Cannot found binary path");
        } while (0);
    }

    C_LOG_INFO("Finished!");
}

bool fs_sandbox_header_check(EfsFileHeader * header)
{
    if (!header) {
        C_LOG_WARNING("header is null");
        return false;
    }

    // if (be32_to_cpu(SANDBOX_MAGIC) != header->magic) {
        // C_LOG_WARNING("invalid magic");
        // return false;
    // }

    if (FILE_TYPE_SANDBOX != header->fileType) {
        C_LOG_WARNING("invalid file type");
        return false;
    }

    return true;
}

static ntfs_time mkntfs_time(void)
{
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    ts.tv_sec = time(NULL);

    return timespec2ntfs(ts);
}

static uint64_t crc64(uint64_t crc, const byte * data, size_t size)
{
    static uint64_t polynomial = 0x9a6c9329ac4bc9b5ULL;
    static uint64_t xorout = 0xffffffffffffffffULL;
    static uint64_t table[256];

    crc ^= xorout;

    if (data == NULL) {
        /* generate the table of CRC remainders for all possible bytes */
        uint64_t c;
        uint32_t i, j;
        for (i = 0;  i < 256;  i++) {
            c = i;
            for (j = 0;  j < 8;  j++) {
                if (c & 1) {
                    c = polynomial ^ (c >> 1);
                }
                else {
                    c = (c >> 1);
                }
            }
            table[i] = c;
        }
    }
    else {
        while (size) {
            crc = table[(crc ^ *data) & 0xff] ^ (crc >> 8);
            size--;
            data++;
        }
    }

    crc ^= xorout;

    return crc;
}

static long long mkntfs_write(struct ntfs_device *dev, const void *b, long long count)
{
    long long bytes_written, total;
    int retry;

    total = 0LL;
    retry = 0;
    do {
        bytes_written = dev->d_ops->write(dev, b, count);
        if (bytes_written == -1LL) {
            retry = errno;
            C_LOG_WARNING("Error writing to %s", dev->d_name);
            errno = retry;
            return bytes_written;
        }
        else if (!bytes_written) {
            retry++;
        }
        else {
            count -= bytes_written;
            total += bytes_written;
        }
    } while (count && retry < 3);

    if (count) {
        C_LOG_WARNING("Failed to complete writing to %s after three retries.", dev->d_name);
    }

    return total;
}

static s64 mkntfs_bitmap_write(struct ntfs_device *dev, s64 offset, s64 length)
{
    s64 partial_length;
    s64 written;

    partial_length = length;
    if (partial_length > gsDynamicBufSize)
        partial_length = gsDynamicBufSize;
    /* create a partial bitmap section, and write it */
    bitmap_build(gsDynamicBuf,offset << 3,partial_length << 3);
    written = dev->d_ops->write(dev, gsDynamicBuf, partial_length);
    return (written);
}

static void bitmap_build(u8 *buf, LCN lcn, s64 length)
{
    struct BITMAP_ALLOCATION *p;
    LCN first, last;
    int j; /* byte number */
    int bn; /* bit number */

    for (j=0; (8*j)<length; j++)
        buf[j] = 0;
    for (p=gsAllocation; p; p=p->next) {
        first = (p->lcn > lcn ? p->lcn : lcn);
        last = ((p->lcn + p->length) < (lcn + length)
            ? p->lcn + p->length : lcn + length);
        if (first < last) {
            bn = first - lcn;
            /* initial partial byte, if any */
            while ((bn < (last - lcn)) && (bn & 7)) {
                buf[bn >> 3] |= 1 << (bn & 7);
                bn++;
            }
            /* full bytes */
            while (bn < (last - lcn - 7)) {
                buf[bn >> 3] = 255;
                bn += 8;
            }
            /* final partial byte, if any */
            while (bn < (last - lcn)) {
                buf[bn >> 3] |= 1 << (bn & 7);
                bn++;
            }
        }
    }
}

static BOOL bitmap_allocate(LCN lcn, s64 length)
{
    BOOL done;
    struct BITMAP_ALLOCATION *p;
    struct BITMAP_ALLOCATION *q;
    struct BITMAP_ALLOCATION *newall;

    done = TRUE;
    if (length) {
        p = gsAllocation;
        q = (struct BITMAP_ALLOCATION*)NULL;
        /* locate the first run which starts beyond the requested lcn */
        // 找到请求的lcn之后的第一个 run
        while (p && (p->lcn <= lcn)) {
            q = p;
            p = p->next;
        }

        // q 前一个, p 当前
        /* make sure the requested lcns were not allocated */
        if ((q && ((q->lcn + q->length) > lcn)) || (p && ((lcn + length) > p->lcn))) {
            C_LOG_WARNING("Bitmap allocation error");
            done = FALSE;
        }
        if (q && ((q->lcn + q->length) == lcn)) {
            /* extend current run, no overlapping possible */
            q->length += length;
        }
        else {
            newall = (struct BITMAP_ALLOCATION*) ntfs_malloc(sizeof(struct BITMAP_ALLOCATION));
            if (newall) {
                newall->lcn = lcn;
                newall->length = length;
                newall->next = p;
                if (q) {
                    q->next = newall;
                }
                else {
                    gsAllocation = newall;
                }
            }
            else {
                done = FALSE;
                C_LOG_WARNING("Not enough memory");
            }
        }
    }
    return (done);
}

static BOOL bitmap_deallocate(LCN lcn, s64 length)
{
    BOOL done;
    struct BITMAP_ALLOCATION *p;
    struct BITMAP_ALLOCATION *q;
    LCN first, last;
    s64 begin_length, end_length;

    done = TRUE;
    if (length) {
        p = gsAllocation;
        q = (struct BITMAP_ALLOCATION*)NULL;
        /* locate a run which has a common portion */
        while (p) {
            first = (p->lcn > lcn ? p->lcn : lcn);
            last = ((p->lcn + p->length) < (lcn + length)
                ? p->lcn + p->length : lcn + length);
            if (first < last) {
                /* get the parts which must be kept */
                begin_length = first - p->lcn;
                end_length = p->lcn + p->length - last;
                /* delete the entry */
                if (q)
                    q->next = p->next;
                else
                    gsAllocation = p->next;
                free(p);
                /* reallocate the beginning and the end */
                if (begin_length
                    && !bitmap_allocate(first - begin_length,
                            begin_length))
                    done = FALSE;
                if (end_length
                    && !bitmap_allocate(last, end_length))
                    done = FALSE;
                /* restart a full search */
                p = gsAllocation;
                q = (struct BITMAP_ALLOCATION*)NULL;
            } else {
                q = p;
                p = p->next;
            }
        }
    }
    return (done);
}

static int bitmap_get_and_set(LCN lcn, unsigned long length)
{
    struct BITMAP_ALLOCATION *p;
    struct BITMAP_ALLOCATION *q;
    int bit;

    if (length == 1) {
        p = gsAllocation;
        q = (struct BITMAP_ALLOCATION*)NULL;
        /* locate the first run which starts beyond the requested lcn */
        while (p && (p->lcn <= lcn)) {
            q = p;
            p = p->next;
        }
        if (q && (q->lcn <= lcn) && ((q->lcn + q->length) > lcn))
            bit = 1; /* was allocated */
        else {
            bitmap_allocate(lcn, length);
            bit = 0;
        }
    } else {
        C_LOG_WARNING("Can only allocate a single cluster at a time");
        bit = 0;
    }
    return (bit);
}

static BOOL mkntfs_parse_long(const char *string, const char *name, long *num)
{
    char *end = NULL;
    long tmp;

    if (!string || !name || !num)
        return FALSE;

    if (*num >= 0) {
        C_LOG_WARNING("You may only specify the %s once.", name);
        return FALSE;
    }

    tmp = strtol(string, &end, 0);
    if (end && *end) {
        C_LOG_WARNING("Cannot understand the %s '%s'.", name, string);
        return FALSE;
    } else {
        *num = tmp;
        return TRUE;
    }
}

static BOOL mkntfs_parse_llong(const char *string, const char *name, long long *num)
{
    char *end = NULL;
    long long tmp;

    if (!string || !name || !num)
        return FALSE;

    if (*num >= 0) {
        C_LOG_WARNING("You may only specify the %s once.", name);
        return FALSE;
    }

    tmp = strtoll(string, &end, 0);
    if (end && *end) {
        C_LOG_WARNING("Cannot understand the %s '%s'.", name,
                string);
        return FALSE;
    } else {
        *num = tmp;
        return TRUE;
    }
}

static BOOL append_to_bad_blocks(unsigned long long block)
{
    long long *new_buf;

    if (!(gsNumBadBlocks & 15)) {
        new_buf = realloc(gsBadBlocks, (gsNumBadBlocks + 16) * sizeof(long long));
        if (!new_buf) {
            C_LOG_WARNING("Reallocating memory for bad blocks list failed");
            return FALSE;
        }
        gsBadBlocks = new_buf;
    }

    gsBadBlocks[gsNumBadBlocks++] = block;
    return TRUE;
}

static s64 mkntfs_logfile_write(struct ntfs_device *dev, s64 offset __attribute__((unused)), s64 length)
{
    s64 partial_length;
    s64 written;

    partial_length = length;
    if (partial_length > gsDynamicBufSize)
        partial_length = gsDynamicBufSize;
    /* create a partial bad cluster section, and write it */
    memset(gsDynamicBuf, -1, partial_length);
    written = dev->d_ops->write(dev, gsDynamicBuf, partial_length);
    return (written);
}

static s64 ntfs_rlwrite(struct ntfs_device *dev, const runlist *rl, const u8 *val, const s64 val_len, s64 *inited_size, WRITE_TYPE write_type)
{
    s64 bytes_written, total, length, delta;
    int retry, i;

    if (inited_size)
        *inited_size = 0LL;
    total = 0LL;
    delta = 0LL;
    for (i = 0; rl[i].length; i++) {
        length = rl[i].length * gsVol->cluster_size;
        /* Don't write sparse runs. */
        if (rl[i].lcn == -1) {
            total += length;
            if (!val)
                continue;
            /* TODO: Check that *val is really zero at pos and len. */
            continue;
        }
        /*
         * Break up the write into the real data write and then a write
         * of zeroes between the end of the real data and the end of
         * the (last) run.
         */
        if (total + length > val_len) {
            delta = length;
            length = val_len - total;
            delta -= length;
        }
        if (dev->d_ops->seek(dev, rl[i].lcn * gsVol->cluster_size, SEEK_SET) == (off_t)-1) {
            return -1LL;
        }
        retry = 0;
        do {
            /* use specific functions if buffer is not prefilled */
            switch (write_type) {
            case WRITE_BITMAP :
                bytes_written = mkntfs_bitmap_write(dev, total, length);
                break;
            case WRITE_LOGFILE :
                bytes_written = mkntfs_logfile_write(dev, total, length);
                break;
            default :
                bytes_written = dev->d_ops->write(dev, val + total, length);
                break;
            }
            if (bytes_written == -1LL) {
                retry = errno;
                C_LOG_WARNING("Error writing to %s", dev->d_name);
                errno = retry;
                return bytes_written;
            }
            if (bytes_written) {
                length -= bytes_written;
                total += bytes_written;
                if (inited_size) {
                    *inited_size += bytes_written;
                }
            }
            else {
                retry++;
            }
        } while (length && retry < 3);
        if (length) {
            C_LOG_WARNING("Failed to complete writing to %s after three retries.", dev->d_name);
            return total;
        }
    }
    if (delta) {
        int eo;
        char *b = ntfs_calloc(delta);
        if (!b) {
            return -1;
        }
        bytes_written = mkntfs_write(dev, b, delta);
        eo = errno;
        free(b);
        errno = eo;
        if (bytes_written == -1LL) {
            return bytes_written;
        }
    }
    return total;
}

static int make_room_for_attribute(MFT_RECORD *m, char *pos, const u32 size)
{
    u32 biu;

    if (!size)
        return 0;
#ifdef DEBUG
    /*
     * Rigorous consistency checks. Always return -EINVAL even if more
     * appropriate codes exist for simplicity of parsing the return value.
     */
    if (size != ((size + 7) & ~7)) {
        C_LOG_WARNING("make_room_for_attribute() received non 8-byte aligned "
                "size.");
        return -EINVAL;
    }
    if (!m || !pos)
        return -EINVAL;
    if (pos < (char*)m || pos + size < (char*)m ||
            pos > (char*)m + le32_to_cpu(m->bytes_allocated) ||
            pos + size > (char*)m + le32_to_cpu(m->bytes_allocated))
        return -EINVAL;
    /* The -8 is for the attribute terminator. */
    if (pos - (char*)m > (int)le32_to_cpu(m->bytes_in_use) - 8)
        return -EINVAL;
#endif
    biu = le32_to_cpu(m->bytes_in_use);
    /* Do we have enough space? */
    if (biu + size > le32_to_cpu(m->bytes_allocated))
        return -ENOSPC;
    /* Move everything after pos to pos + size. */
    memmove(pos + size, pos, biu - (pos - (char*)m));
    /* Update mft record. */
    m->bytes_in_use = cpu_to_le32(biu + size);
    return 0;
}

static void deallocate_scattered_clusters(const runlist *rl)
{
    int i;

    if (!rl)
        return;
    /* Iterate over all runs in the runlist @rl. */
    for (i = 0; rl[i].length; i++) {
        /* Skip sparse runs. */
        if (rl[i].lcn == -1LL)
            continue;
        /* Deallocate the current run. */
        bitmap_deallocate(rl[i].lcn, rl[i].length);
    }
}

static runlist * allocate_scattered_clusters(s64 clusters)
{
    runlist *rl = NULL, *rlt;
    VCN vcn = 0LL;
    LCN lcn, end, prev_lcn = 0LL;
    int rlpos = 0;
    int rlsize = 0;
    s64 prev_run_len = 0LL;
    char bit;

    end = gsVol->nr_clusters;
    /* Loop until all clusters are allocated. */
    while (clusters) {
        /* Loop in current zone until we run out of free clusters. */
        for (lcn = gsMftZoneEnd; lcn < end; lcn++) {
            bit = bitmap_get_and_set(lcn,1);
            if (bit)
                continue;
            /*
             * Reallocate memory if necessary. Make sure we have
             * enough for the terminator entry as well.
             */
            if ((rlpos + 2) * (int)sizeof(runlist) >= rlsize) {
                rlsize += 4096; /* PAGE_SIZE */
                rlt = realloc(rl, rlsize);
                if (!rlt)
                    goto err_end;
                rl = rlt;
            }
            /* Coalesce with previous run if adjacent LCNs. */
            if (prev_lcn == lcn - prev_run_len) {
                rl[rlpos - 1].length = ++prev_run_len;
                vcn++;
            } else {
                rl[rlpos].vcn = vcn++;
                rl[rlpos].lcn = lcn;
                prev_lcn = lcn;
                rl[rlpos].length = 1LL;
                prev_run_len = 1LL;
                rlpos++;
            }
            /* Done? */
            if (!--clusters) {
                /* Add terminator element and return. */
                rl[rlpos].vcn = vcn;
                rl[rlpos].lcn = 0LL;
                rl[rlpos].length = 0LL;
                return rl;
            }

        }
        /* Switch to next zone, decreasing mft zone by factor 2. */
        end = gsMftZoneEnd;
        gsMftZoneEnd >>= 1;
        /* Have we run out of space on the volume? */
        if (gsMftZoneEnd <= 0)
            goto err_end;
    }
    return rl;
err_end:
    if (rl) {
        /* Add terminator element. */
        rl[rlpos].vcn = vcn;
        rl[rlpos].lcn = -1LL;
        rl[rlpos].length = 0LL;
        /* Deallocate all allocated clusters. */
        deallocate_scattered_clusters(rl);
        /* Free the runlist. */
        free(rl);
    }
    return NULL;
}

static int mkntfs_attr_find(const ATTR_TYPES type, const ntfschar *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
    ATTR_RECORD *a;
    ntfschar *upcase = gsVol->upcase;
    u32 upcase_len = gsVol->upcase_len;

    /*
     * Iterate over attributes in mft record starting at @ctx->attr, or the
     * attribute following that, if @ctx->is_first is TRUE.
     */
    if (ctx->is_first) {
        a = ctx->attr;
        ctx->is_first = FALSE;
    }
    else {
        a = (ATTR_RECORD*)((char*)ctx->attr + le32_to_cpu(ctx->attr->length));
    }

    for (;; a = (ATTR_RECORD*)((char*)a + le32_to_cpu(a->length))) {
        if (p2n(a) < p2n(ctx->mrec) || (char*)a > (char*)ctx->mrec + le32_to_cpu(ctx->mrec->bytes_allocated)) {
            break;
        }

        ctx->attr = a;
        if (((type != AT_UNUSED) && (le32_to_cpu(a->type) > le32_to_cpu(type))) || (a->type == AT_END)) {
            errno = ENOENT;
            return -1;
        }
        if (!a->length)
            break;
        /* If this is an enumeration return this attribute. */
        if (type == AT_UNUSED)
            return 0;
        if (a->type != type)
            continue;
        /*
         * If @name is AT_UNNAMED we want an unnamed attribute.
         * If @name is present, compare the two names.
         * Otherwise, match any attribute.
         */
        if (name == AT_UNNAMED) {
            /* The search failed if the found attribute is named. */
            if (a->name_length) {
                errno = ENOENT;
                return -1;
            }
        }
        else if (name && !ntfs_names_are_equal(name, name_len, (ntfschar*)((char*)a + le16_to_cpu(a->name_offset)), a->name_length, ic, upcase, upcase_len)) {
            int rc;
            rc = ntfs_names_full_collate(name, name_len, (ntfschar*)((char*)a + le16_to_cpu(a->name_offset)), a->name_length, IGNORE_CASE, upcase, upcase_len);
            /*
             * If @name collates before a->name, there is no
             * matching attribute.
             */
            if (rc == -1) {
                errno = ENOENT;
                return -1;
            }
            /* If the strings are not equal, continue search. */
            if (rc) {
                continue;
            }
            rc = ntfs_names_full_collate(name, name_len, (ntfschar*)((char*)a + le16_to_cpu(a->name_offset)), a->name_length, CASE_SENSITIVE, upcase, upcase_len);
            if (rc == -1) {
                errno = ENOENT;
                return -1;
            }
            if (rc) {
                continue;
            }
        }
        /*
         * The names match or @name not present and attribute is
         * unnamed. If no @val specified, we have found the attribute
         * and are done.
         */
        if (!val) {
            return 0;
        /* @val is present; compare values. */
        }
        else {
            int rc;

            rc = memcmp(val, (char*)a +le16_to_cpu(a->value_offset), min(val_len, le32_to_cpu(a->value_length)));
            /*
             * If @val collates before the current attribute's
             * value, there is no matching attribute.
             */
            if (!rc) {
                u32 avl;
                avl = le32_to_cpu(a->value_length);
                if (val_len == avl) {
                    return 0;
                }
                if (val_len < avl) {
                    errno = ENOENT;
                    return -1;
                }
            }
            else if (rc < 0) {
                errno = ENOENT;
                return -1;
            }
        }
    }
    ntfs_log_trace("File is corrupt. Run chkdsk.");
    errno = EIO;
    return -1;
}

static int mkntfs_attr_lookup(const ATTR_TYPES type, const ntfschar *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const VCN lowest_vcn __attribute__((unused)), const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
    ntfs_inode *base_ni;

    if (!ctx || !ctx->mrec || !ctx->attr) {
        errno = EINVAL;
        return -1;
    }

    if (ctx->base_ntfs_ino) {
        base_ni = ctx->base_ntfs_ino;
    }
    else {
        base_ni = ctx->ntfs_ino;
    }

    if (!base_ni || !NInoAttrList(base_ni) || type == AT_ATTRIBUTE_LIST) {
        return mkntfs_attr_find(type, name, name_len, ic, val, val_len, ctx);
    }

    errno = EOPNOTSUPP;

    return -1;
}

static int insert_positioned_attr_in_mft_record(MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len)
{
    ntfs_attr_search_ctx *ctx;
    ATTR_RECORD *a;
    u16 hdr_size;
    int asize, mpa_size, err, i;
    s64 bw = 0, inited_size;
    VCN highest_vcn;
    ntfschar *uname = NULL;
    int uname_len = 0;
    /*
    if (base record)
        attr_lookup();
    else
    */

    uname = ntfs_str2ucs(name, &uname_len);
    if (!uname)
        return -errno;

    /* Check if the attribute is already there. */
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to allocate attribute search context.");
        err = -ENOMEM;
        goto err_out;
    }
    if (ic == IGNORE_CASE) {
        C_LOG_WARNING("FIXME: Hit unimplemented code path #1.");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, NULL, 0, ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    if (errno != ENOENT) {
        C_LOG_WARNING("Corrupt inode.");
        err = -errno;
        goto err_out;
    }
    a = ctx->attr;
    if (flags & ATTR_COMPRESSION_MASK) {
        C_LOG_WARNING("Compressed attributes not supported yet.");
        /* FIXME: Compress attribute into a temporary buffer, set */
        /* val accordingly and save the compressed size. */
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
        C_LOG_WARNING("Encrypted/sparse attributes not supported.");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (flags & ATTR_COMPRESSION_MASK) {
        hdr_size = 72;
        /* FIXME: This compression stuff is all wrong. Never mind for */
        /* now. (AIA) */
        if (val_len) {
            mpa_size = 0; /* get_size_for_compressed_mapping_pairs(rl); */
        }
        else {
            mpa_size = 0;
        }
    }
    else {
        hdr_size = 64;
        if (val_len) {
            mpa_size = ntfs_get_size_for_mapping_pairs(gsVol, rl, 0, INT_MAX);
            if (mpa_size < 0) {
                err = -errno;
                C_LOG_WARNING("Failed to get size for mapping pairs.");
                goto err_out;
            }
        }
        else {
            mpa_size = 0;
        }
    }
    /* Mapping pairs array and next attribute must be 8-byte aligned. */
    asize = (((int)hdr_size + ((name_len + 7) & ~7) + mpa_size) + 7) & ~7;

    /* Get the highest vcn. */
    for (i = 0, highest_vcn = 0LL; rl[i].length; i++) {
        highest_vcn += rl[i].length;
    }

    /* Does the value fit inside the allocated size? */
    if (highest_vcn * gsVol->cluster_size < val_len) {
        C_LOG_WARNING("BUG: Allocated size is smaller than data size!");
        err = -EINVAL;
        goto err_out;
    }
    err = make_room_for_attribute(m, (char*)a, asize);
    if (err == -ENOSPC) {
        /*
         * FIXME: Make space! (AIA)
         * can we make it non-resident? if yes, do that.
         *    does it fit now? yes -> do it.
         * m's $DATA or $BITMAP+$INDEX_ALLOCATION resident?
         * yes -> make non-resident
         *    does it fit now? yes -> do it.
         * make all attributes non-resident
         *    does it fit now? yes -> do it.
         * m is a base record? yes -> allocate extension record
         *    does the new attribute fit in there? yes -> do it.
         * split up runlist into extents and place each in an extension
         * record.
         * FIXME: the check for needing extension records should be
         * earlier on as it is very quick: asize > m->bytes_allocated?
         */
        err = -EOPNOTSUPP;
        goto err_out;
#ifdef DEBUG
    } else if (err == -EINVAL) {
        C_LOG_WARNING("BUG(): in insert_positioned_attribute_in_mft_"
                "record(): make_room_for_attribute() returned "
                "error: EINVAL!");
        goto err_out;
#endif
    }
    a->type = type;
    a->length = cpu_to_le32(asize);
    a->non_resident = 1;
    a->name_length = name_len;
    a->name_offset = cpu_to_le16(hdr_size);
    a->flags = flags;
    a->instance = m->next_attr_instance;
    m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
    a->lowest_vcn = const_cpu_to_sle64(0);
    a->highest_vcn = cpu_to_sle64(highest_vcn - 1LL);
    a->mapping_pairs_offset = cpu_to_le16(hdr_size + ((name_len + 7) & ~7));
    memset(a->reserved1, 0, sizeof(a->reserved1));
    /* FIXME: Allocated size depends on compression. */
    a->allocated_size = cpu_to_sle64(highest_vcn * gsVol->cluster_size);
    a->data_size = cpu_to_sle64(val_len);
    if (name_len) {
        memcpy((char*)a + hdr_size, uname, name_len << 1);
    }
    if (flags & ATTR_COMPRESSION_MASK) {
        if (flags & ATTR_COMPRESSION_MASK & ~ATTR_IS_COMPRESSED) {
            C_LOG_WARNING("Unknown compression format. Reverting to standard compression.");
            a->flags &= ~ATTR_COMPRESSION_MASK;
            a->flags |= ATTR_IS_COMPRESSED;
        }
        a->compression_unit = 4;
        inited_size = val_len;
        /* FIXME: Set the compressed size. */
        a->compressed_size = const_cpu_to_sle64(0);
        /* FIXME: Write out the compressed data. */
        /* FIXME: err = build_mapping_pairs_compressed(); */
        err = -EOPNOTSUPP;
    }
    else {
        a->compression_unit = 0;
        if ((type == AT_DATA) && (m->mft_record_number == const_cpu_to_le32(FILE_LogFile))) {
            bw = ntfs_rlwrite(gsVol->dev, rl, val, val_len, &inited_size, WRITE_LOGFILE);
        }
        else {
            bw = ntfs_rlwrite(gsVol->dev, rl, val, val_len, &inited_size, WRITE_STANDARD);
        }
        if (bw != val_len) {
            C_LOG_WARNING("Error writing non-resident attribute value.");
            return -errno;
        }
        err = ntfs_mapping_pairs_build(gsVol, (u8*)a + hdr_size + ((name_len + 7) & ~7), mpa_size, rl, 0, NULL);
    }
    a->initialized_size = cpu_to_sle64(inited_size);
    if (err < 0 || bw != val_len) {
        /* FIXME: Handle error. */
        /* deallocate clusters */
        /* remove attribute */
        if (err >= 0) {
            err = -EIO;
        }
        C_LOG_WARNING("insert_positioned_attr_in_mft_record failed with error %i.", err < 0 ? err : (int)bw);
    }
err_out:
    if (ctx) {
        ntfs_attr_put_search_ctx(ctx);
    }
    ntfs_ucsfree(uname);

    return err;
}

static int insert_non_resident_attr_in_mft_record(MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const u8 *val, const s64 val_len, WRITE_TYPE write_type)
{
    ntfs_attr_search_ctx *ctx;
    ATTR_RECORD *a;
    u16 hdr_size;
    int asize, mpa_size, err, i;
    runlist *rl = NULL;
    s64 bw = 0;
    ntfschar *uname = NULL;
    int uname_len = 0;
    /*
    if (base record)
        attr_lookup();
    else
    */

    uname = ntfs_str2ucs(name, &uname_len);
    if (!uname) {
        return -errno;
    }

    /* Check if the attribute is already there. */
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to allocate attribute search context.");
        err = -ENOMEM;
        goto err_out;
    }
    if (ic == IGNORE_CASE) {
        C_LOG_WARNING("FIXME: Hit unimplemented code path #2.");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, NULL, 0, ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    if (errno != ENOENT) {
        C_LOG_WARNING("Corrupt inode.");
        err = -errno;
        goto err_out;
    }
    a = ctx->attr;
    if (flags & ATTR_COMPRESSION_MASK) {
        C_LOG_WARNING("Compressed attributes not supported yet.");
        /* FIXME: Compress attribute into a temporary buffer, set */
        /* val accordingly and save the compressed size. */
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
        C_LOG_WARNING("Encrypted/sparse attributes not supported.");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (val_len) {
        rl = allocate_scattered_clusters((val_len + gsVol->cluster_size - 1) / gsVol->cluster_size);
        if (!rl) {
            err = -errno;
            C_LOG_WARNING("Failed to allocate scattered clusters");
            goto err_out;
        }
    }
    else {
        rl = NULL;
    }
    if (flags & ATTR_COMPRESSION_MASK) {
        hdr_size = 72;
        /* FIXME: This compression stuff is all wrong. Never mind for */
        /* now. (AIA) */
        if (val_len) {
            mpa_size = 0; /* get_size_for_compressed_mapping_pairs(rl); */
        }
        else {
            mpa_size = 0;
        }
    }
    else {
        hdr_size = 64;
        if (val_len) {
            mpa_size = ntfs_get_size_for_mapping_pairs(gsVol, rl, 0, INT_MAX);
            if (mpa_size < 0) {
                err = -errno;
                C_LOG_WARNING("Failed to get size for mapping pairs.");
                goto err_out;
            }
        }
        else {
            mpa_size = 0;
        }
    }
    /* Mapping pairs array and next attribute must be 8-byte aligned. */
    asize = (((int)hdr_size + ((name_len + 7) & ~7) + mpa_size) + 7) & ~7;
    err = make_room_for_attribute(m, (char*)a, asize);
    if (err == -ENOSPC) {
        /*
         * FIXME: Make space! (AIA)
         * can we make it non-resident? if yes, do that.
         *    does it fit now? yes -> do it.
         * m's $DATA or $BITMAP+$INDEX_ALLOCATION resident?
         * yes -> make non-resident
         *    does it fit now? yes -> do it.
         * make all attributes non-resident
         *    does it fit now? yes -> do it.
         * m is a base record? yes -> allocate extension record
         *    does the new attribute fit in there? yes -> do it.
         * split up runlist into extents and place each in an extension
         * record.
         * FIXME: the check for needing extension records should be
         * earlier on as it is very quick: asize > m->bytes_allocated?
         */
        err = -EOPNOTSUPP;
        goto err_out;
#ifdef DEBUG
    }
    else if (err == -EINVAL) {
        C_LOG_WARNING("BUG(): in insert_non_resident_attribute_in_mft_record(): make_room_for_attribute() returned error: EINVAL!");
        goto err_out;
#endif
    }
    a->type = type;
    a->length = cpu_to_le32(asize);
    a->non_resident = 1;
    a->name_length = name_len;
    a->name_offset = cpu_to_le16(hdr_size);
    a->flags = flags;
    a->instance = m->next_attr_instance;
    m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
    a->lowest_vcn = const_cpu_to_sle64(0);

    for (i = 0; rl[i].length; i++);

    a->highest_vcn = cpu_to_sle64(rl[i].vcn - 1);
    a->mapping_pairs_offset = cpu_to_le16(hdr_size + ((name_len + 7) & ~7));
    memset(a->reserved1, 0, sizeof(a->reserved1));
    /* FIXME: Allocated size depends on compression. */
    a->allocated_size = cpu_to_sle64((val_len + (gsVol->cluster_size - 1)) & ~(gsVol->cluster_size - 1));
    a->data_size = cpu_to_sle64(val_len);
    a->initialized_size = cpu_to_sle64(val_len);
    if (name_len) {
        memcpy((char*)a + hdr_size, uname, name_len << 1);
    }
    if (flags & ATTR_COMPRESSION_MASK) {
        if (flags & ATTR_COMPRESSION_MASK & ~ATTR_IS_COMPRESSED) {
            C_LOG_WARNING("Unknown compression format. Reverting to standard compression.");
            a->flags &= ~ATTR_COMPRESSION_MASK;
            a->flags |= ATTR_IS_COMPRESSED;
        }
        a->compression_unit = 4;
        /* FIXME: Set the compressed size. */
        a->compressed_size = const_cpu_to_sle64(0);
        /* FIXME: Write out the compressed data. */
        /* FIXME: err = build_mapping_pairs_compressed(); */
        err = -EOPNOTSUPP;
    }
    else {
        a->compression_unit = 0;
        bw = ntfs_rlwrite(gsVol->dev, rl, val, val_len, NULL, write_type);
        if (bw != val_len) {
            C_LOG_WARNING("Error writing non-resident attribute value.");
            return -errno;
        }
        err = ntfs_mapping_pairs_build(gsVol, (u8*)a + hdr_size + ((name_len + 7) & ~7), mpa_size, rl, 0, NULL);
    }
    if (err < 0 || bw != val_len) {
        /* FIXME: Handle error. */
        /* deallocate clusters */
        /* remove attribute */
        if (err >= 0) {
            err = -EIO;
        }
        C_LOG_WARNING("insert_non_resident_attr_in_mft_record failed with error %lld.", (long long) (err < 0 ? err : bw));
    }
err_out:
    if (ctx) {
        ntfs_attr_put_search_ctx(ctx);
    }
    ntfs_ucsfree(uname);
    free(rl);
    return err;
}

static int insert_resident_attr_in_mft_record(MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const RESIDENT_ATTR_FLAGS res_flags, const u8 *val, const u32 val_len)
{
    ntfs_attr_search_ctx *ctx;
    ATTR_RECORD *a;
    int asize, err;
    ntfschar *uname = NULL;
    int uname_len = 0;
    /*
    if (base record)
        mkntfs_attr_lookup();
    else
    */

    uname = ntfs_str2ucs(name, &uname_len);
    if (!uname)
        return -errno;

    /* Check if the attribute is already there. */
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to allocate attribute search context.");
        err = -ENOMEM;
        goto err_out;
    }
    if (ic == IGNORE_CASE) {
        C_LOG_WARNING("FIXME: Hit unimplemented code path #3.");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, val, val_len,
            ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    if (errno != ENOENT) {
        C_LOG_WARNING("Corrupt inode.");
        err = -errno;
        goto err_out;
    }
    a = ctx->attr;
    /* sizeof(resident attribute record header) == 24 */
    asize = ((24 + ((name_len*2 + 7) & ~7) + val_len) + 7) & ~7;
    err = make_room_for_attribute(m, (char*)a, asize);
    if (err == -ENOSPC) {
        /*
         * FIXME: Make space! (AIA)
         * can we make it non-resident? if yes, do that.
         *    does it fit now? yes -> do it.
         * m's $DATA or $BITMAP+$INDEX_ALLOCATION resident?
         * yes -> make non-resident
         *    does it fit now? yes -> do it.
         * make all attributes non-resident
         *    does it fit now? yes -> do it.
         * m is a base record? yes -> allocate extension record
         *    does the new attribute fit in there? yes -> do it.
         * split up runlist into extents and place each in an extension
         * record.
         * FIXME: the check for needing extension records should be
         * earlier on as it is very quick: asize > m->bytes_allocated?
         */
        err = -EOPNOTSUPP;
        goto err_out;
    }
#ifdef DEBUG
    if (err == -EINVAL) {
        C_LOG_WARNING("BUG(): in insert_resident_attribute_in_mft_"
                "record(): make_room_for_attribute() returned "
                "error: EINVAL!");
        goto err_out;
    }
#endif
    a->type = type;
    a->length = cpu_to_le32(asize);
    a->non_resident = 0;
    a->name_length = name_len;
    if (type == AT_OBJECT_ID) {
        a->name_offset = const_cpu_to_le16(0);
    }
    else {
        a->name_offset = const_cpu_to_le16(24);
    }
    a->flags = flags;
    a->instance = m->next_attr_instance;
    m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
    a->value_length = cpu_to_le32(val_len);
    a->value_offset = cpu_to_le16(24 + ((name_len*2 + 7) & ~7));
    a->resident_flags = res_flags;
    a->reservedR = 0;
    if (name_len)
        memcpy((char*)a + 24, uname, name_len << 1);
    if (val_len)
        memcpy((char*)a + le16_to_cpu(a->value_offset), val, val_len);
err_out:
    if (ctx)
        ntfs_attr_put_search_ctx(ctx);
    ntfs_ucsfree(uname);
    return err;
}

static int add_attr_std_info(MFT_RECORD *m, const FILE_ATTR_FLAGS flags, le32 security_id)
{
    STANDARD_INFORMATION si;
    int err, sd_size;

    sd_size = 48;

    si.creation_time = mkntfs_time();
    si.last_data_change_time = si.creation_time;
    si.last_mft_change_time = si.creation_time;
    si.last_access_time = si.creation_time;
    si.file_attributes = flags; /* already LE */
    si.maximum_versions = const_cpu_to_le32(0);
    si.version_number = const_cpu_to_le32(0);
    si.class_id = const_cpu_to_le32(0);
    si.security_id = security_id;
    if (si.security_id != const_cpu_to_le32(0))
        sd_size = 72;
    /* FIXME: $Quota support... */
    si.owner_id = const_cpu_to_le32(0);
    si.quota_charged = const_cpu_to_le64(0ULL);
    /* FIXME: $UsnJrnl support... Not needed on fresh w2k3-volume */
    si.usn = const_cpu_to_le64(0ULL);
    /* NTFS 1.2: size of si = 48, NTFS 3.[01]: size of si = 72 */
    err = insert_resident_attr_in_mft_record(m, AT_STANDARD_INFORMATION,
            NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0),
            0, (u8*)&si, sd_size);
    if (err < 0)
        C_LOG_WARNING("add_attr_std_info failed");
    return err;
}

static BOOL non_resident_unnamed_data(MFT_RECORD *m)
{
    ATTR_RECORD *a;
    ntfs_attr_search_ctx *ctx;
    BOOL nonres;

    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (ctx && !mkntfs_attr_find(AT_DATA,
                (const ntfschar*)NULL, 0, CASE_SENSITIVE,
                (u8*)NULL, 0, ctx)) {
        a = ctx->attr;
        nonres = a->non_resident != 0;
                } else {
                    C_LOG_WARNING("BUG: Unnamed data not found");
                    nonres = TRUE;
                }
    if (ctx)
        ntfs_attr_put_search_ctx(ctx);
    return (nonres);
}

static ntfs_time stdinfo_time(MFT_RECORD *m)
{
    STANDARD_INFORMATION *si;
    ntfs_attr_search_ctx *ctx;
    ntfs_time info_time;

    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (ctx && !mkntfs_attr_find(AT_STANDARD_INFORMATION,
                (const ntfschar*)NULL, 0, CASE_SENSITIVE,
                (u8*)NULL, 0, ctx)) {
        si = (STANDARD_INFORMATION*)((char*)ctx->attr +
                le16_to_cpu(ctx->attr->value_offset));
        info_time = si->creation_time;
                } else {
                    C_LOG_WARNING("BUG: Standard information not found");
                    info_time = mkntfs_time();
                }
    if (ctx)
        ntfs_attr_put_search_ctx(ctx);
    return (info_time);
}

static int add_attr_file_name(MFT_RECORD *m, const leMFT_REF parent_dir, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type)
{
    ntfs_attr_search_ctx *ctx;
    STANDARD_INFORMATION *si;
    FILE_NAME_ATTR *fn;
    int i, fn_size;
    ntfschar *uname;

    /* Check if the attribute is already there. */
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to get attribute search context.");
        return -ENOMEM;
    }
    if (mkntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED, 0,
                CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        int eo = errno;
        C_LOG_WARNING("BUG: Standard information attribute not "
                "present in file record.");
        ntfs_attr_put_search_ctx(ctx);
        return -eo;
    }
    si = (STANDARD_INFORMATION*)((char*)ctx->attr +
            le16_to_cpu(ctx->attr->value_offset));
    i = (strlen(file_name) + 1) * sizeof(ntfschar);
    fn_size = sizeof(FILE_NAME_ATTR) + i;
    fn = ntfs_malloc(fn_size);
    if (!fn) {
        ntfs_attr_put_search_ctx(ctx);
        return -errno;
    }
    fn->parent_directory = parent_dir;

    fn->creation_time = si->creation_time;
    fn->last_data_change_time = si->last_data_change_time;
    fn->last_mft_change_time = si->last_mft_change_time;
    fn->last_access_time = si->last_access_time;
    ntfs_attr_put_search_ctx(ctx);

    fn->allocated_size = cpu_to_sle64(allocated_size);
    fn->data_size = cpu_to_sle64(data_size);
    fn->file_attributes = flags;
    /* These are in a union so can't have both. */
    if (packed_ea_size && reparse_point_tag) {
        free(fn);
        return -EINVAL;
    }
    if (packed_ea_size) {
        fn->packed_ea_size = cpu_to_le16(packed_ea_size);
        fn->reserved = const_cpu_to_le16(0);
    } else {
        fn->reparse_point_tag = cpu_to_le32(reparse_point_tag);
    }
    fn->file_name_type = file_name_type;
    uname = fn->file_name;
    i = ntfs_mbstoucs_libntfscompat(file_name, &uname, i);
    if (i < 1) {
        free(fn);
        return -EINVAL;
    }
    if (i > 0xff) {
        free(fn);
        return -ENAMETOOLONG;
    }
    /* No terminating null in file names. */
    fn->file_name_length = i;
    fn_size = sizeof(FILE_NAME_ATTR) + i * sizeof(ntfschar);
    i = insert_resident_attr_in_mft_record(m, AT_FILE_NAME, NULL, 0,
            CASE_SENSITIVE, const_cpu_to_le16(0),
            RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
    free(fn);
    if (i < 0)
        C_LOG_WARNING("add_attr_file_name failed: %s", strerror(-i));
    return i;
}

static int add_attr_object_id(MFT_RECORD *m, const GUID *object_id)
{
    OBJECT_ID_ATTR oi;
    int err;

    oi = (OBJECT_ID_ATTR) {
        .object_id = *object_id,
    };
    err = insert_resident_attr_in_mft_record(m, AT_OBJECT_ID, NULL,
            0, CASE_SENSITIVE, const_cpu_to_le16(0),
            0, (u8*)&oi, sizeof(oi.object_id));
    if (err < 0)
        C_LOG_WARNING("add_attr_vol_info failed: %s", strerror(-err));
    return err;
}

static int add_attr_sd(MFT_RECORD *m, const u8 *sd, const s64 sd_len)
{
    int err;

    /* Does it fit? NO: create non-resident. YES: create resident. */
    if (le32_to_cpu(m->bytes_in_use) + 24 + sd_len > le32_to_cpu(m->bytes_allocated)) {
        err = insert_non_resident_attr_in_mft_record(m, AT_SECURITY_DESCRIPTOR, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), sd, sd_len, WRITE_STANDARD);
    }
    else {
        err = insert_resident_attr_in_mft_record(m, AT_SECURITY_DESCRIPTOR, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), 0, sd, sd_len);
    }

    if (err < 0) {
        C_LOG_WARNING("add_attr_sd failed: %s", strerror(-err));
    }
    return err;
}

static int add_attr_data(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const u8 *val, const s64 val_len)
{
    int err;

    /*
     * Does it fit? NO: create non-resident. YES: create resident.
     *
     * FIXME: Introduced arbitrary limit of mft record allocated size - 512.
     * This is to get around the problem that if $Bitmap/$DATA becomes too
     * big, but is just small enough to be resident, we would make it
     * resident, and later run out of space when creating the other
     * attributes and this would cause us to abort as making resident
     * attributes non-resident is not supported yet.
     * The proper fix is to support making resident attribute non-resident.
     */
    if (le32_to_cpu(m->bytes_in_use) + 24 + val_len >
            min(le32_to_cpu(m->bytes_allocated),
            le32_to_cpu(m->bytes_allocated) - 512))
        err = insert_non_resident_attr_in_mft_record(m, AT_DATA, name,
                name_len, ic, flags, val, val_len,
                WRITE_STANDARD);
    else
        err = insert_resident_attr_in_mft_record(m, AT_DATA, name,
                name_len, ic, flags, 0, val, val_len);

    if (err < 0)
        C_LOG_WARNING("add_attr_data failed: %s", strerror(-err));
    return err;
}

static int add_attr_data_positioned(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len)
{
    int err;

    err = insert_positioned_attr_in_mft_record(m, AT_DATA, name, name_len, ic, flags, rl, val, val_len);
    if (err < 0) {
        C_LOG_WARNING("add_attr_data_positioned failed: %s", strerror(-err));
    }

    return err;
}

static int add_attr_vol_name(MFT_RECORD *m, const char *vol_name, const int vol_name_len __attribute__((unused)))
{
    ntfschar *uname = NULL;
    int uname_len = 0;
    int i;

    if (vol_name) {
        uname_len = ntfs_mbstoucs(vol_name, &uname);
        if (uname_len < 0)
            return -errno;
        if (uname_len > 128) {
            free(uname);
            return -ENAMETOOLONG;
        }
    }
    i = insert_resident_attr_in_mft_record(m, AT_VOLUME_NAME, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), 0, (u8*)uname, uname_len*sizeof(ntfschar));
    free(uname);
    if (i < 0)
        C_LOG_WARNING("add_attr_vol_name failed: %s", strerror(-i));
    return i;
}

static int add_attr_vol_info(MFT_RECORD *m, const VOLUME_FLAGS flags, const u8 major_ver, const u8 minor_ver)
{
    VOLUME_INFORMATION vi;
    int err;

    memset(&vi, 0, sizeof(vi));
    vi.major_ver = major_ver;
    vi.minor_ver = minor_ver;
    vi.flags = flags & VOLUME_FLAGS_MASK;
    err = insert_resident_attr_in_mft_record(m, AT_VOLUME_INFORMATION, NULL,
            0, CASE_SENSITIVE, const_cpu_to_le16(0),
            0, (u8*)&vi, sizeof(vi));
    if (err < 0)
        C_LOG_WARNING("add_attr_vol_info failed: %s", strerror(-err));
    return err;
}

static int add_attr_index_root(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_TYPES indexed_attr_type, const COLLATION_RULES collation_rule, const u32 index_block_size)
{
    INDEX_ROOT *r;
    INDEX_ENTRY_HEADER *e;
    int err, val_len;

    val_len = sizeof(INDEX_ROOT) + sizeof(INDEX_ENTRY_HEADER);
    r = ntfs_malloc(val_len);
    if (!r)
        return -errno;
    r->type = (indexed_attr_type == AT_FILE_NAME) ? AT_FILE_NAME : const_cpu_to_le32(0);
    if (indexed_attr_type == AT_FILE_NAME && collation_rule != COLLATION_FILE_NAME) {
        free(r);
        C_LOG_WARNING("add_attr_index_root: indexed attribute is $FILE_NAME "
            "but collation rule is not COLLATION_FILE_NAME.");
        return -EINVAL;
    }
    r->collation_rule = collation_rule;
    r->index_block_size = cpu_to_le32(index_block_size);
    if (index_block_size >= gsVol->cluster_size) {
        if (index_block_size % gsVol->cluster_size) {
            C_LOG_WARNING("add_attr_index_root: index block size is not "
                    "a multiple of the cluster size.");
            free(r);
            return -EINVAL;
        }
        r->clusters_per_index_block = index_block_size / gsVol->cluster_size;
    } else { /* if (g_vol->cluster_size > index_block_size) */
        if (index_block_size & (index_block_size - 1)) {
            C_LOG_WARNING("add_attr_index_root: index block size is not "
                    "a power of 2.");
            free(r);
            return -EINVAL;
        }
        r->clusters_per_index_block = index_block_size >> NTFS_BLOCK_SIZE_BITS;
    }
    memset(&r->reserved, 0, sizeof(r->reserved));
    r->index.entries_offset = const_cpu_to_le32(sizeof(INDEX_HEADER));
    r->index.index_length = const_cpu_to_le32(sizeof(INDEX_HEADER) + sizeof(INDEX_ENTRY_HEADER));
    r->index.allocated_size = r->index.index_length;
    r->index.ih_flags = SMALL_INDEX;
    memset(&r->index.reserved, 0, sizeof(r->index.reserved));
    e = (INDEX_ENTRY_HEADER*)((u8*)&r->index + le32_to_cpu(r->index.entries_offset));
    /*
     * No matter whether this is a file index or a view as this is a
     * termination entry, hence no key value / data is associated with it
     * at all. Thus, we just need the union to be all zero.
     */
    e->indexed_file = const_cpu_to_le64(0LL);
    e->length = const_cpu_to_le16(sizeof(INDEX_ENTRY_HEADER));
    e->key_length = const_cpu_to_le16(0);
    e->flags = INDEX_ENTRY_END;
    e->reserved = const_cpu_to_le16(0);
    err = insert_resident_attr_in_mft_record(m, AT_INDEX_ROOT, name, name_len, ic, const_cpu_to_le16(0), 0, (u8*)r, val_len);
    free(r);
    if (err < 0) {
        C_LOG_WARNING("add_attr_index_root failed: %s", strerror(-err));
    }
    return err;
}

static int add_attr_index_alloc(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *index_alloc_val, const u32 index_alloc_val_len)
{
    int err;

    err = insert_non_resident_attr_in_mft_record(m, AT_INDEX_ALLOCATION,
            name, name_len, ic, const_cpu_to_le16(0),
            index_alloc_val, index_alloc_val_len, WRITE_STANDARD);
    if (err < 0)
        C_LOG_WARNING("add_attr_index_alloc failed: %s", strerror(-err));
    return err;
}

static int add_attr_bitmap(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *bitmap, const u32 bitmap_len)
{
    int err;

    /* Does it fit? NO: create non-resident. YES: create resident. */
    if (le32_to_cpu(m->bytes_in_use) + 24 + bitmap_len > le32_to_cpu(m->bytes_allocated))
        err = insert_non_resident_attr_in_mft_record(m, AT_BITMAP, name, name_len, ic, const_cpu_to_le16(0), bitmap, bitmap_len, WRITE_STANDARD);
    else
        err = insert_resident_attr_in_mft_record(m, AT_BITMAP, name,
                name_len, ic, const_cpu_to_le16(0), 0,
                bitmap, bitmap_len);

    if (err < 0)
        C_LOG_WARNING("add_attr_bitmap failed: %s", strerror(-err));
    return err;
}

static int add_attr_bitmap_positioned(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const runlist *rl, const u8 *bitmap, const u32 bitmap_len)
{
    int err;

    err = insert_positioned_attr_in_mft_record(m, AT_BITMAP, name, name_len,
            ic, const_cpu_to_le16(0), rl, bitmap, bitmap_len);
    if (err < 0)
        C_LOG_WARNING("add_attr_bitmap_positioned failed: %s",
                strerror(-err));
    return err;
}

static int upgrade_to_large_index(MFT_RECORD *m, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, INDEX_ALLOCATION **idx)
{
    ntfs_attr_search_ctx *ctx;
    ATTR_RECORD *a;
    INDEX_ROOT *r;
    INDEX_ENTRY *re;
    INDEX_ALLOCATION *ia_val = NULL;
    ntfschar *uname = NULL;
    int uname_len = 0;
    u8 bmp[8];
    char *re_start, *re_end;
    int i, err, index_block_size;

    uname = ntfs_str2ucs(name, &uname_len);
    if (!uname)
        return -errno;

    /* Find the index root attribute. */
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to allocate attribute search context.");
        ntfs_ucsfree(uname);
        return -ENOMEM;
    }
    if (ic == IGNORE_CASE) {
        C_LOG_WARNING("FIXME: Hit unimplemented code path #4.");
        err = -EOPNOTSUPP;
        ntfs_ucsfree(uname);
        goto err_out;
    }
    err = mkntfs_attr_lookup(AT_INDEX_ROOT, uname, uname_len, ic, 0, NULL, 0, ctx);
    ntfs_ucsfree(uname);
    if (err) {
        err = -ENOTDIR;
        goto err_out;
    }
    a = ctx->attr;
    if (a->non_resident || a->flags) {
        err = -EINVAL;
        goto err_out;
    }
    r = (INDEX_ROOT*)((char*)a + le16_to_cpu(a->value_offset));
    re_end = (char*)r + le32_to_cpu(a->value_length);
    re_start = (char*)&r->index + le32_to_cpu(r->index.entries_offset);
    re = (INDEX_ENTRY*)re_start;
    index_block_size = le32_to_cpu(r->index_block_size);
    memset(bmp, 0, sizeof(bmp));
    ntfs_bit_set(bmp, 0ULL, 1);
    /* Bitmap has to be at least 8 bytes in size. */
    err = add_attr_bitmap(m, name, name_len, ic, bmp, sizeof(bmp));
    if (err)
        goto err_out;
    ia_val = ntfs_calloc(index_block_size);
    if (!ia_val) {
        err = -errno;
        goto err_out;
    }
    /* Setup header. */
    ia_val->magic = magic_INDX;
    ia_val->usa_ofs = const_cpu_to_le16(sizeof(INDEX_ALLOCATION));
    if (index_block_size >= NTFS_BLOCK_SIZE) {
        ia_val->usa_count = cpu_to_le16(index_block_size /
                NTFS_BLOCK_SIZE + 1);
    } else {
        ia_val->usa_count = const_cpu_to_le16(1);
        C_LOG_WARNING("Sector size is bigger than index block size. "
                "Setting usa_count to 1. If Windows chkdsk "
                "reports this as corruption, please email %s "
                "stating that you saw this message and that "
                "the filesystem created was corrupt.  "
                "Thank you.", NTFS_DEV_LIST);
    }
    /* Set USN to 1. */
    *(le16*)((char*)ia_val + le16_to_cpu(ia_val->usa_ofs)) =
            const_cpu_to_le16(1);
    ia_val->lsn = const_cpu_to_sle64(0);
    ia_val->index_block_vcn = const_cpu_to_sle64(0);
    ia_val->index.ih_flags = LEAF_NODE;
    /* Align to 8-byte boundary. */
    ia_val->index.entries_offset = cpu_to_le32((sizeof(INDEX_HEADER) +
            le16_to_cpu(ia_val->usa_count) * 2 + 7) & ~7);
    ia_val->index.allocated_size = cpu_to_le32(index_block_size -
            (sizeof(INDEX_ALLOCATION) - sizeof(INDEX_HEADER)));
    /* Find the last entry in the index root and save it in re. */
    while ((char*)re < re_end && !(re->ie_flags & INDEX_ENTRY_END)) {
        /* Next entry in index root. */
        re = (INDEX_ENTRY*)((char*)re + le16_to_cpu(re->length));
    }
    /* Copy all the entries including the termination entry. */
    i = (char*)re - re_start + le16_to_cpu(re->length);
    memcpy((char*)&ia_val->index +
            le32_to_cpu(ia_val->index.entries_offset), re_start, i);
    /* Finish setting up index allocation. */
    ia_val->index.index_length = cpu_to_le32(i +
            le32_to_cpu(ia_val->index.entries_offset));
    /* Move the termination entry forward to the beginning if necessary. */
    if ((char*)re > re_start) {
        memmove(re_start, (char*)re, le16_to_cpu(re->length));
        re = (INDEX_ENTRY*)re_start;
    }
    /* Now fixup empty index root with pointer to index allocation VCN 0. */
    r->index.ih_flags = LARGE_INDEX;
    re->ie_flags |= INDEX_ENTRY_NODE;
    if (le16_to_cpu(re->length) < sizeof(INDEX_ENTRY_HEADER) + sizeof(VCN))
        re->length = cpu_to_le16(le16_to_cpu(re->length) + sizeof(VCN));
    r->index.index_length = cpu_to_le32(le32_to_cpu(r->index.entries_offset)
            + le16_to_cpu(re->length));
    r->index.allocated_size = r->index.index_length;
    /* Resize index root attribute. */
    if (ntfs_resident_attr_value_resize(m, a, sizeof(INDEX_ROOT) -
            sizeof(INDEX_HEADER) +
            le32_to_cpu(r->index.allocated_size))) {
        /* TODO: Remove the added bitmap! */
        /* Revert index root from index allocation. */
        err = -errno;
        goto err_out;
    }
    /* Set VCN pointer to 0LL. */
    *(leVCN*)((char*)re + le16_to_cpu(re->length) - sizeof(VCN)) =
            const_cpu_to_sle64(0);
    err = ntfs_mst_pre_write_fixup((NTFS_RECORD*)ia_val, index_block_size);
    if (err) {
        err = -errno;
        C_LOG_WARNING("ntfs_mst_pre_write_fixup() failed in "
                "upgrade_to_large_index.");
        goto err_out;
    }
    err = add_attr_index_alloc(m, name, name_len, ic, (u8*)ia_val,
            index_block_size);
    ntfs_mst_post_write_fixup((NTFS_RECORD*)ia_val);
    if (err) {
        /* TODO: Remove the added bitmap! */
        /* Revert index root from index allocation. */
        goto err_out;
    }
    *idx = ia_val;
    ntfs_attr_put_search_ctx(ctx);
    return 0;
err_out:
    ntfs_attr_put_search_ctx(ctx);
    free(ia_val);
    return err;
}

static int make_room_for_index_entry_in_index_block(INDEX_BLOCK *idx, INDEX_ENTRY *pos, u32 size)
{
    u32 biu;

    if (!size)
        return 0;
#ifdef DEBUG
    /*
     * Rigorous consistency checks. Always return -EINVAL even if more
     * appropriate codes exist for simplicity of parsing the return value.
     */
    if (size != ((size + 7) & ~7)) {
        C_LOG_WARNING("make_room_for_index_entry_in_index_block() received "
                "non 8-byte aligned size.");
        return -EINVAL;
    }
    if (!idx || !pos)
        return -EINVAL;
    if ((char*)pos < (char*)idx || (char*)pos + size < (char*)idx ||
            (char*)pos > (char*)idx + sizeof(INDEX_BLOCK) -
                sizeof(INDEX_HEADER) +
                le32_to_cpu(idx->index.allocated_size) ||
            (char*)pos + size > (char*)idx + sizeof(INDEX_BLOCK) -
                sizeof(INDEX_HEADER) +
                le32_to_cpu(idx->index.allocated_size))
        return -EINVAL;
    /* The - sizeof(INDEX_ENTRY_HEADER) is for the index terminator. */
    if ((char*)pos - (char*)&idx->index >
            (int)le32_to_cpu(idx->index.index_length)
            - (int)sizeof(INDEX_ENTRY_HEADER))
        return -EINVAL;
#endif
    biu = le32_to_cpu(idx->index.index_length);
    /* Do we have enough space? */
    if (biu + size > le32_to_cpu(idx->index.allocated_size))
        return -ENOSPC;
    /* Move everything after pos to pos + size. */
    memmove((char*)pos + size, (char*)pos, biu - ((char*)pos -
            (char*)&idx->index));
    /* Update index block. */
    idx->index.index_length = cpu_to_le32(biu + size);
    return 0;
}

static int ntfs_index_keys_compare(u8 *key1, u8 *key2, int key1_length, int key2_length, COLLATION_RULES collation_rule)
{
    u32 u1, u2;
    int i;

    if (collation_rule == COLLATION_NTOFS_ULONG) {
        /* i.e. $SII or $QUOTA-$Q */
        u1 = le32_to_cpup((const le32*)key1);
        u2 = le32_to_cpup((const le32*)key2);
        if (u1 < u2)
            return -1;
        if (u1 > u2)
            return 1;
        /* u1 == u2 */
        return 0;
    }
    if (collation_rule == COLLATION_NTOFS_ULONGS) {
        /* i.e $OBJID-$O */
        i = 0;
        while (i < min(key1_length, key2_length)) {
            u1 = le32_to_cpup((const le32*)(key1 + i));
            u2 = le32_to_cpup((const le32*)(key2 + i));
            if (u1 < u2)
                return -1;
            if (u1 > u2)
                return 1;
            /* u1 == u2 */
            i += sizeof(u32);
        }
        if (key1_length < key2_length)
            return -1;
        if (key1_length > key2_length)
            return 1;
        return 0;
    }
    if (collation_rule == COLLATION_NTOFS_SECURITY_HASH) {
        /* i.e. $SDH */
        u1 = le32_to_cpu(((SDH_INDEX_KEY*)key1)->hash);
        u2 = le32_to_cpu(((SDH_INDEX_KEY*)key2)->hash);
        if (u1 < u2)
            return -1;
        if (u1 > u2)
            return 1;
        /* u1 == u2 */
        u1 = le32_to_cpu(((SDH_INDEX_KEY*)key1)->security_id);
        u2 = le32_to_cpu(((SDH_INDEX_KEY*)key2)->security_id);
        if (u1 < u2)
            return -1;
        if (u1 > u2)
            return 1;
        return 0;
    }
    if (collation_rule == COLLATION_NTOFS_SID) {
        /* i.e. $QUOTA-O */
        i = memcmp(key1, key2, min(key1_length, key2_length));
        if (!i) {
            if (key1_length < key2_length)
                return -1;
            if (key1_length > key2_length)
                return 1;
        }
        return i;
    }
    ntfs_log_critical("ntfs_index_keys_compare called without supported "
            "collation rule.");
    return 0;    /* Claim they're equal.  What else can we do? */
}

static int insert_index_entry_in_res_dir_index(INDEX_ENTRY *idx, u32 idx_size, MFT_RECORD *m, ntfschar *name, u32 name_size, ATTR_TYPES type)
{
    ntfs_attr_search_ctx *ctx;
    INDEX_HEADER *idx_header;
    INDEX_ENTRY *idx_entry, *idx_end;
    ATTR_RECORD *a;
    COLLATION_RULES collation_rule;
    int err, i;

    err = 0;
    /* does it fit ?*/
    if (gsVol->mft_record_size > idx_size + le32_to_cpu(m->bytes_allocated))
        return -ENOSPC;
    /* find the INDEX_ROOT attribute:*/
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to allocate attribute search "
                "context.");
        err = -ENOMEM;
        goto err_out;
    }
    if (mkntfs_attr_lookup(AT_INDEX_ROOT, name, name_size,
            CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    /* found attribute */
    a = (ATTR_RECORD*)ctx->attr;
    collation_rule = ((INDEX_ROOT*)((u8*)a +
            le16_to_cpu(a->value_offset)))->collation_rule;
    idx_header = (INDEX_HEADER*)((u8*)a + le16_to_cpu(a->value_offset)
            + 0x10);
    idx_entry = (INDEX_ENTRY*)((u8*)idx_header +
            le32_to_cpu(idx_header->entries_offset));
    idx_end = (INDEX_ENTRY*)((u8*)idx_entry +
            le32_to_cpu(idx_header->index_length));
    /*
     * Loop until we exceed valid memory (corruption case) or until we
     * reach the last entry.
     */
    if (type == AT_FILE_NAME) {
        while (((u8*)idx_entry < (u8*)idx_end) &&
                !(idx_entry->ie_flags & INDEX_ENTRY_END)) {
            /*
            i = ntfs_file_values_compare(&idx->key.file_name,
                    &idx_entry->key.file_name, 1,
                    IGNORE_CASE, g_vol->upcase,
                    g_vol->upcase_len);
            */
            i = ntfs_names_full_collate(idx->key.file_name.file_name, idx->key.file_name.file_name_length,
                    idx_entry->key.file_name.file_name, idx_entry->key.file_name.file_name_length,
                    IGNORE_CASE, gsVol->upcase,
                    gsVol->upcase_len);
            /*
             * If @file_name collates before ie->key.file_name,
             * there is no matching index entry.
             */
            if (i == -1)
                break;
            /* If file names are not equal, continue search. */
            if (i)
                goto do_next;
            if (idx->key.file_name.file_name_type !=
                    FILE_NAME_POSIX ||
                    idx_entry->key.file_name.file_name_type
                    != FILE_NAME_POSIX)
                return -EEXIST;
            /*
            i = ntfs_file_values_compare(&idx->key.file_name,
                    &idx_entry->key.file_name, 1,
                    CASE_SENSITIVE, g_vol->upcase,
                    g_vol->upcase_len);
            */
            i = ntfs_names_full_collate(idx->key.file_name.file_name, idx->key.file_name.file_name_length,
                    idx_entry->key.file_name.file_name, idx_entry->key.file_name.file_name_length,
                    CASE_SENSITIVE, gsVol->upcase,
                    gsVol->upcase_len);
            if (!i)
                return -EEXIST;
            if (i == -1)
                break;
do_next:
            idx_entry = (INDEX_ENTRY*)((u8*)idx_entry +
                    le16_to_cpu(idx_entry->length));
        }
    } else if (type == AT_UNUSED) {  /* case view */
        while (((u8*)idx_entry < (u8*)idx_end) &&
                !(idx_entry->ie_flags & INDEX_ENTRY_END)) {
            i = ntfs_index_keys_compare((u8*)idx + 0x10,
                    (u8*)idx_entry + 0x10,
                    le16_to_cpu(idx->key_length),
                    le16_to_cpu(idx_entry->key_length),
                    collation_rule);
            if (!i)
                return -EEXIST;
            if (i == -1)
                break;
            idx_entry = (INDEX_ENTRY*)((u8*)idx_entry +
                    le16_to_cpu(idx_entry->length));
        }
    } else
        return -EINVAL;
    memmove((u8*)idx_entry + idx_size, (u8*)idx_entry,
            le32_to_cpu(m->bytes_in_use) -
            ((u8*)idx_entry - (u8*)m));
    memcpy((u8*)idx_entry, (u8*)idx, idx_size);
    /* Adjust various offsets, etc... */
    m->bytes_in_use = cpu_to_le32(le32_to_cpu(m->bytes_in_use) + idx_size);
    a->length = cpu_to_le32(le32_to_cpu(a->length) + idx_size);
    a->value_length = cpu_to_le32(le32_to_cpu(a->value_length) + idx_size);
    idx_header->index_length = cpu_to_le32(
            le32_to_cpu(idx_header->index_length) + idx_size);
    idx_header->allocated_size = cpu_to_le32(
            le32_to_cpu(idx_header->allocated_size) + idx_size);
err_out:
    if (ctx)
        ntfs_attr_put_search_ctx(ctx);
    return err;
}

static int initialize_secure(char *sds, u32 sds_size, MFT_RECORD *m)
{
    int err, sdh_size, sii_size;
    SECURITY_DESCRIPTOR_HEADER *sds_header;
    INDEX_ENTRY *idx_entry_sdh, *idx_entry_sii;
    SDH_INDEX_DATA *sdh_data;
    SII_INDEX_DATA *sii_data;

    sds_header = (SECURITY_DESCRIPTOR_HEADER*)sds;
    sdh_size  = sizeof(INDEX_ENTRY_HEADER);
    sdh_size += sizeof(SDH_INDEX_KEY) + sizeof(SDH_INDEX_DATA);
    sii_size  = sizeof(INDEX_ENTRY_HEADER);
    sii_size += sizeof(SII_INDEX_KEY) + sizeof(SII_INDEX_DATA);
    idx_entry_sdh = ntfs_calloc(sizeof(INDEX_ENTRY));
    if (!idx_entry_sdh)
        return -errno;
    idx_entry_sii = ntfs_calloc(sizeof(INDEX_ENTRY));
    if (!idx_entry_sii) {
        free(idx_entry_sdh);
        return -errno;
    }
    err = 0;

    while ((char*)sds_header < (char*)sds + sds_size) {
        if (!sds_header->length)
            break;
        /* SDH index entry */
        idx_entry_sdh->data_offset = const_cpu_to_le16(0x18);
        idx_entry_sdh->data_length = const_cpu_to_le16(0x14);
        idx_entry_sdh->reservedV = const_cpu_to_le32(0x00);
        idx_entry_sdh->length = const_cpu_to_le16(0x30);
        idx_entry_sdh->key_length = const_cpu_to_le16(0x08);
        idx_entry_sdh->ie_flags = const_cpu_to_le16(0x00);
        idx_entry_sdh->reserved = const_cpu_to_le16(0x00);
        idx_entry_sdh->key.sdh.hash = sds_header->hash;
        idx_entry_sdh->key.sdh.security_id = sds_header->security_id;
        sdh_data = (SDH_INDEX_DATA*)((u8*)idx_entry_sdh + le16_to_cpu(idx_entry_sdh->data_offset));
        sdh_data->hash = sds_header->hash;
        sdh_data->security_id = sds_header->security_id;
        sdh_data->offset = sds_header->offset;
        sdh_data->length = sds_header->length;
        sdh_data->reserved_II = const_cpu_to_le32(0x00490049);

        /* SII index entry */
        idx_entry_sii->data_offset = const_cpu_to_le16(0x14);
        idx_entry_sii->data_length = const_cpu_to_le16(0x14);
        idx_entry_sii->reservedV = const_cpu_to_le32(0x00);
        idx_entry_sii->length = const_cpu_to_le16(0x28);
        idx_entry_sii->key_length = const_cpu_to_le16(0x04);
        idx_entry_sii->ie_flags = const_cpu_to_le16(0x00);
        idx_entry_sii->reserved = const_cpu_to_le16(0x00);
        idx_entry_sii->key.sii.security_id = sds_header->security_id;
        sii_data = (SII_INDEX_DATA*)((u8*)idx_entry_sii +
                le16_to_cpu(idx_entry_sii->data_offset));
        sii_data->hash = sds_header->hash;
        sii_data->security_id = sds_header->security_id;
        sii_data->offset = sds_header->offset;
        sii_data->length = sds_header->length;
        if ((err = insert_index_entry_in_res_dir_index(idx_entry_sdh, sdh_size, m, NTFS_INDEX_SDH, 4, AT_UNUSED)))
            break;
        if ((err = insert_index_entry_in_res_dir_index(idx_entry_sii, sii_size, m, NTFS_INDEX_SII, 4, AT_UNUSED)))
            break;
        sds_header = (SECURITY_DESCRIPTOR_HEADER*)((u8*)sds_header + ((le32_to_cpu(sds_header->length) + 15) & ~15));
    }
    free(idx_entry_sdh);
    free(idx_entry_sii);
    return err;
}

static int initialize_quota(MFT_RECORD *m)
{
    int o_size, q1_size, q2_size, err, i;
    INDEX_ENTRY *idx_entry_o, *idx_entry_q1, *idx_entry_q2;
    QUOTA_O_INDEX_DATA *idx_entry_o_data;
    QUOTA_CONTROL_ENTRY *idx_entry_q1_data, *idx_entry_q2_data;

    err = 0;
    /* q index entry num 1 */
    q1_size = 0x48;
    idx_entry_q1 = ntfs_calloc(q1_size);
    if (!idx_entry_q1)
        return errno;
    idx_entry_q1->data_offset = const_cpu_to_le16(0x14);
    idx_entry_q1->data_length = const_cpu_to_le16(0x30);
    idx_entry_q1->reservedV = const_cpu_to_le32(0x00);
    idx_entry_q1->length = const_cpu_to_le16(0x48);
    idx_entry_q1->key_length = const_cpu_to_le16(0x04);
    idx_entry_q1->ie_flags = const_cpu_to_le16(0x00);
    idx_entry_q1->reserved = const_cpu_to_le16(0x00);
    idx_entry_q1->key.owner_id = const_cpu_to_le32(0x01);
    idx_entry_q1_data = (QUOTA_CONTROL_ENTRY*)((char*)idx_entry_q1 + le16_to_cpu(idx_entry_q1->data_offset));
    idx_entry_q1_data->version = const_cpu_to_le32(0x02);
    idx_entry_q1_data->flags = QUOTA_FLAG_DEFAULT_LIMITS;
    idx_entry_q1_data->bytes_used = const_cpu_to_le64(0x00);
    idx_entry_q1_data->change_time = mkntfs_time();
    idx_entry_q1_data->threshold = const_cpu_to_sle64(-1);
    idx_entry_q1_data->limit = const_cpu_to_sle64(-1);
    idx_entry_q1_data->exceeded_time = const_cpu_to_sle64(0);
    err = insert_index_entry_in_res_dir_index(idx_entry_q1, q1_size, m, NTFS_INDEX_Q, 2, AT_UNUSED);
    free(idx_entry_q1);
    if (err)
        return err;
    /* q index entry num 2 */
    q2_size = 0x58;
    idx_entry_q2 = ntfs_calloc(q2_size);
    if (!idx_entry_q2)
        return errno;
    idx_entry_q2->data_offset = const_cpu_to_le16(0x14);
    idx_entry_q2->data_length = const_cpu_to_le16(0x40);
    idx_entry_q2->reservedV = const_cpu_to_le32(0x00);
    idx_entry_q2->length = const_cpu_to_le16(0x58);
    idx_entry_q2->key_length = const_cpu_to_le16(0x04);
    idx_entry_q2->ie_flags = const_cpu_to_le16(0x00);
    idx_entry_q2->reserved = const_cpu_to_le16(0x00);
    idx_entry_q2->key.owner_id = QUOTA_FIRST_USER_ID;
    idx_entry_q2_data = (QUOTA_CONTROL_ENTRY*)((char*)idx_entry_q2 + le16_to_cpu(idx_entry_q2->data_offset));
    idx_entry_q2_data->version = const_cpu_to_le32(0x02);
    idx_entry_q2_data->flags = QUOTA_FLAG_DEFAULT_LIMITS;
    idx_entry_q2_data->bytes_used = const_cpu_to_le64(0x00);
    idx_entry_q2_data->change_time = mkntfs_time();
    idx_entry_q2_data->threshold = const_cpu_to_sle64(-1);
    idx_entry_q2_data->limit = const_cpu_to_sle64(-1);
    idx_entry_q2_data->exceeded_time = const_cpu_to_sle64(0);
    idx_entry_q2_data->sid.revision = 1;
    idx_entry_q2_data->sid.sub_authority_count = 2;
    for (i = 0; i < 5; i++)
        idx_entry_q2_data->sid.identifier_authority.value[i] = 0;
    idx_entry_q2_data->sid.identifier_authority.value[5] = 0x05;
    idx_entry_q2_data->sid.sub_authority[0] = const_cpu_to_le32(SECURITY_BUILTIN_DOMAIN_RID);
    idx_entry_q2_data->sid.sub_authority[1] = const_cpu_to_le32(DOMAIN_ALIAS_RID_ADMINS);
    err = insert_index_entry_in_res_dir_index(idx_entry_q2, q2_size, m, NTFS_INDEX_Q, 2, AT_UNUSED);
    free(idx_entry_q2);
    if (err)
        return err;
    o_size = 0x28;
    idx_entry_o = ntfs_calloc(o_size);
    if (!idx_entry_o)
        return errno;
    idx_entry_o->data_offset = const_cpu_to_le16(0x20);
    idx_entry_o->data_length = const_cpu_to_le16(0x04);
    idx_entry_o->reservedV = const_cpu_to_le32(0x00);
    idx_entry_o->length = const_cpu_to_le16(0x28);
    idx_entry_o->key_length = const_cpu_to_le16(0x10);
    idx_entry_o->ie_flags = const_cpu_to_le16(0x00);
    idx_entry_o->reserved = const_cpu_to_le16(0x00);
    idx_entry_o->key.sid.revision = 0x01;
    idx_entry_o->key.sid.sub_authority_count = 0x02;
    for (i = 0; i < 5; i++)
        idx_entry_o->key.sid.identifier_authority.value[i] = 0;
    idx_entry_o->key.sid.identifier_authority.value[5] = 0x05;
    idx_entry_o->key.sid.sub_authority[0] = const_cpu_to_le32(SECURITY_BUILTIN_DOMAIN_RID);
    idx_entry_o->key.sid.sub_authority[1] = const_cpu_to_le32(DOMAIN_ALIAS_RID_ADMINS);
    idx_entry_o_data = (QUOTA_O_INDEX_DATA*)((char*)idx_entry_o + le16_to_cpu(idx_entry_o->data_offset));
    idx_entry_o_data->owner_id  = QUOTA_FIRST_USER_ID;
    /* 20 00 00 00 padding after here on ntfs 3.1. 3.0 is unchecked. */
    idx_entry_o_data->unknown = const_cpu_to_le32(32);
    err = insert_index_entry_in_res_dir_index(idx_entry_o, o_size, m, NTFS_INDEX_O, 2, AT_UNUSED);
    free(idx_entry_o);

    return err;
}

static int insert_file_link_in_dir_index(INDEX_BLOCK *idx, leMFT_REF file_ref, FILE_NAME_ATTR *file_name, u32 file_name_size)
{
    int err, i;
    INDEX_ENTRY *ie;
    char *index_end;

    /*
     * Lookup dir entry @file_name in dir @idx to determine correct
     * insertion location. FIXME: Using a very oversimplified lookup
     * method which is sufficient for mkntfs but no good whatsoever in
     * real world scenario. (AIA)
     */

    index_end = (char*)&idx->index + le32_to_cpu(idx->index.index_length);
    ie = (INDEX_ENTRY*)((char*)&idx->index +
            le32_to_cpu(idx->index.entries_offset));
    /*
     * Loop until we exceed valid memory (corruption case) or until we
     * reach the last entry.
     */
    while ((char*)ie < index_end && !(ie->ie_flags & INDEX_ENTRY_END)) {
#if 0
#ifdef DEBUG
        C_LOG_VERB("file_name_attr1->file_name_length = %i",
                file_name->file_name_length);
        if (file_name->file_name_length) {
            char *__buf = NULL;
            i = ntfs_ucstombs((ntfschar*)&file_name->file_name,
                file_name->file_name_length, &__buf, 0);
            if (i < 0)
                C_LOG_VERB("Name contains non-displayable "
                        "Unicode characters.");
            C_LOG_VERB("file_name_attr1->file_name = %s",
                    __buf);
            free(__buf);
        }
        C_LOG_VERB("file_name_attr2->file_name_length = %i",
                ie->key.file_name.file_name_length);
        if (ie->key.file_name.file_name_length) {
            char *__buf = NULL;
            i = ntfs_ucstombs(ie->key.file_name.file_name,
                ie->key.file_name.file_name_length + 1, &__buf,
                0);
            if (i < 0)
                C_LOG_VERB("Name contains non-displayable "
                        "Unicode characters.");
            C_LOG_VERB("file_name_attr2->file_name = %s",
                    __buf);
            free(__buf);
        }
#endif
#endif
        /*
        i = ntfs_file_values_compare(file_name,
                (FILE_NAME_ATTR*)&ie->key.file_name, 1,
                IGNORE_CASE, g_vol->upcase, g_vol->upcase_len);
        */
        i = ntfs_names_full_collate(file_name->file_name, file_name->file_name_length,
                ((FILE_NAME_ATTR*)&ie->key.file_name)->file_name, ((FILE_NAME_ATTR*)&ie->key.file_name)->file_name_length,
                IGNORE_CASE, gsVol->upcase, gsVol->upcase_len);
        /*
         * If @file_name collates before ie->key.file_name, there is no
         * matching index entry.
         */
        if (i == -1)
            break;
        /* If file names are not equal, continue search. */
        if (i)
            goto do_next;
        /* File names are equal when compared ignoring case. */
        /*
         * If BOTH file names are in the POSIX namespace, do a case
         * sensitive comparison as well. Otherwise the names match so
         * we return -EEXIST. FIXME: There are problems with this in a
         * real world scenario, when one is POSIX and one isn't, but
         * fine for mkntfs where we don't use POSIX namespace at all
         * and hence this following code is luxury. (AIA)
         */
        if (file_name->file_name_type != FILE_NAME_POSIX ||
            ie->key.file_name.file_name_type != FILE_NAME_POSIX)
            return -EEXIST;
        /*
        i = ntfs_file_values_compare(file_name,
                (FILE_NAME_ATTR*)&ie->key.file_name, 1,
                CASE_SENSITIVE, g_vol->upcase,
                g_vol->upcase_len);
        */
        i = ntfs_names_full_collate(file_name->file_name, file_name->file_name_length,
                ((FILE_NAME_ATTR*)&ie->key.file_name)->file_name, ((FILE_NAME_ATTR*)&ie->key.file_name)->file_name_length,
                CASE_SENSITIVE, gsVol->upcase, gsVol->upcase_len);
        if (i == -1)
            break;
        /* Complete match. Bugger. Can't insert. */
        if (!i)
            return -EEXIST;
do_next:
#ifdef DEBUG
        /* Next entry. */
        if (!ie->length) {
            C_LOG_VERB("BUG: ie->length is zero, breaking out "
                    "of loop.");
            break;
        }
#endif
        ie = (INDEX_ENTRY*)((char*)ie + le16_to_cpu(ie->length));
    };
    i = (sizeof(INDEX_ENTRY_HEADER) + file_name_size + 7) & ~7;
    err = make_room_for_index_entry_in_index_block(idx, ie, i);
    if (err) {
        C_LOG_WARNING("make_room_for_index_entry_in_index_block "
                "failed: %s", strerror(-err));
        return err;
    }
    /* Create entry in place and copy file name attribute value. */
    ie->indexed_file = file_ref;
    ie->length = cpu_to_le16(i);
    ie->key_length = cpu_to_le16(file_name_size);
    ie->ie_flags = const_cpu_to_le16(0);
    ie->reserved = const_cpu_to_le16(0);
    memcpy((char*)&ie->key.file_name, (char*)file_name, file_name_size);
    return 0;
}

static int create_hardlink_res(MFT_RECORD *m_parent, const leMFT_REF ref_parent, MFT_RECORD *m_file, const leMFT_REF ref_file, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type)
{
    FILE_NAME_ATTR *fn;
    int i, fn_size, idx_size;
    INDEX_ENTRY *idx_entry_new;
    ntfschar *uname;

    /* Create the file_name attribute. */
    i = (strlen(file_name) + 1) * sizeof(ntfschar);
    fn_size = sizeof(FILE_NAME_ATTR) + i;
    fn = ntfs_malloc(fn_size);
    if (!fn)
        return -errno;
    fn->parent_directory = ref_parent;
    fn->creation_time = stdinfo_time(m_file);
    fn->last_data_change_time = fn->creation_time;
    fn->last_mft_change_time = fn->creation_time;
    fn->last_access_time = fn->creation_time;
    fn->allocated_size = cpu_to_sle64(allocated_size);
    fn->data_size = cpu_to_sle64(data_size);
    fn->file_attributes = flags;
    /* These are in a union so can't have both. */
    if (packed_ea_size && reparse_point_tag) {
        free(fn);
        return -EINVAL;
    }
    if (packed_ea_size) {
        free(fn);
        return -EINVAL;
    }
    if (packed_ea_size) {
        fn->packed_ea_size = cpu_to_le16(packed_ea_size);
        fn->reserved = const_cpu_to_le16(0);
    } else {
        fn->reparse_point_tag = cpu_to_le32(reparse_point_tag);
    }
    fn->file_name_type = file_name_type;
    uname = fn->file_name;
    i = ntfs_mbstoucs_libntfscompat(file_name, &uname, i);
    if (i < 1) {
        free(fn);
        return -EINVAL;
    }
    if (i > 0xff) {
        free(fn);
        return -ENAMETOOLONG;
    }
    /* No terminating null in file names. */
    fn->file_name_length = i;
    fn_size = sizeof(FILE_NAME_ATTR) + i * sizeof(ntfschar);
    /* Increment the link count of @m_file. */
    i = le16_to_cpu(m_file->link_count);
    if (i == 0xffff) {
        C_LOG_WARNING("Too many hardlinks present already.");
        free(fn);
        return -EINVAL;
    }
    m_file->link_count = cpu_to_le16(i + 1);
    /* Add the file_name to @m_file. */
    i = insert_resident_attr_in_mft_record(m_file, AT_FILE_NAME, NULL, 0,
            CASE_SENSITIVE, const_cpu_to_le16(0),
            RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
    if (i < 0) {
        C_LOG_WARNING("create_hardlink failed adding file name "
                "attribute: %s", strerror(-i));
        free(fn);
        /* Undo link count increment. */
        m_file->link_count = cpu_to_le16(
                le16_to_cpu(m_file->link_count) - 1);
        return i;
    }
    /* Insert the index entry for file_name in @idx. */
    idx_size = (fn_size + 7)  & ~7;
    idx_entry_new = ntfs_calloc(idx_size + 0x10);
    if (!idx_entry_new)
        return -errno;
    idx_entry_new->indexed_file = ref_file;
    idx_entry_new->length = cpu_to_le16(idx_size + 0x10);
    idx_entry_new->key_length = cpu_to_le16(fn_size);
    memcpy((u8*)idx_entry_new + 0x10, (u8*)fn, fn_size);
    i = insert_index_entry_in_res_dir_index(idx_entry_new, idx_size + 0x10, m_parent, NTFS_INDEX_I30, 4, AT_FILE_NAME);
    if (i < 0) {
        C_LOG_WARNING("create_hardlink failed inserting index entry: "
                "%s", strerror(-i));
        /* FIXME: Remove the file name attribute from @m_file. */
        free(idx_entry_new);
        free(fn);
        /* Undo link count increment. */
        m_file->link_count = cpu_to_le16(le16_to_cpu(m_file->link_count) - 1);
        return i;
    }
    free(idx_entry_new);
    free(fn);
    return 0;
}

static int create_hardlink(INDEX_BLOCK *idx, const leMFT_REF ref_parent, MFT_RECORD *m_file, const leMFT_REF ref_file, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type)
{
    FILE_NAME_ATTR *fn;
    int i, fn_size;
    ntfschar *uname;

    /* Create the file_name attribute. */
    i = (strlen(file_name) + 1) * sizeof(ntfschar);
    fn_size = sizeof(FILE_NAME_ATTR) + i;
    fn = ntfs_malloc(fn_size);
    if (!fn)
        return -errno;
    fn->parent_directory = ref_parent;
    fn->creation_time = stdinfo_time(m_file);
    fn->last_data_change_time = fn->creation_time;
    fn->last_mft_change_time = fn->creation_time;
    fn->last_access_time = fn->creation_time;
        /* allocated size depends on unnamed data being resident */
    if (allocated_size && non_resident_unnamed_data(m_file))
        fn->allocated_size = cpu_to_sle64(allocated_size);
    else
        fn->allocated_size = cpu_to_sle64((data_size + 7) & -8);
    fn->data_size = cpu_to_sle64(data_size);
    fn->file_attributes = flags;
    /* These are in a union so can't have both. */
    if (packed_ea_size && reparse_point_tag) {
        free(fn);
        return -EINVAL;
    }
    if (packed_ea_size) {
        fn->packed_ea_size = cpu_to_le16(packed_ea_size);
        fn->reserved = const_cpu_to_le16(0);
    } else {
        fn->reparse_point_tag = cpu_to_le32(reparse_point_tag);
    }
    fn->file_name_type = file_name_type;
    uname = fn->file_name;
    i = ntfs_mbstoucs_libntfscompat(file_name, &uname, i);
    if (i < 1) {
        free(fn);
        return -EINVAL;
    }
    if (i > 0xff) {
        free(fn);
        return -ENAMETOOLONG;
    }
    /* No terminating null in file names. */
    fn->file_name_length = i;
    fn_size = sizeof(FILE_NAME_ATTR) + i * sizeof(ntfschar);
    /* Increment the link count of @m_file. */
    i = le16_to_cpu(m_file->link_count);
    if (i == 0xffff) {
        C_LOG_WARNING("Too many hardlinks present already.");
        free(fn);
        return -EINVAL;
    }
    m_file->link_count = cpu_to_le16(i + 1);
    /* Add the file_name to @m_file. */
    i = insert_resident_attr_in_mft_record(m_file, AT_FILE_NAME, NULL, 0,
            CASE_SENSITIVE, const_cpu_to_le16(0),
            RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
    if (i < 0) {
        C_LOG_WARNING("create_hardlink failed adding file name attribute: "
                "%s", strerror(-i));
        free(fn);
        /* Undo link count increment. */
        m_file->link_count = cpu_to_le16(
                le16_to_cpu(m_file->link_count) - 1);
        return i;
    }
    /* Insert the index entry for file_name in @idx. */
    i = insert_file_link_in_dir_index(idx, ref_file, fn, fn_size);
    if (i < 0) {
        C_LOG_WARNING("create_hardlink failed inserting index entry: %s", strerror(-i));
        /* FIXME: Remove the file name attribute from @m_file. */
        free(fn);
        /* Undo link count increment. */
        m_file->link_count = cpu_to_le16(le16_to_cpu(m_file->link_count) - 1);
        return i;
    }
    free(fn);
    return 0;
}

static int index_obj_id_insert(MFT_RECORD *m, const GUID *guid, const leMFT_REF ref)
{
    INDEX_ENTRY *idx_entry_new;
    int data_ofs, idx_size, err;
    OBJ_ID_INDEX_DATA *oi;

    /*
     * Insert the index entry for the object id in the index.
     *
     * First determine the size of the index entry to be inserted.  This
     * consists of the index entry header, followed by the index key, i.e.
     * the GUID, followed by the index data, i.e. OBJ_ID_INDEX_DATA.
     */
    data_ofs = (sizeof(INDEX_ENTRY_HEADER) + sizeof(GUID) + 7) & ~7;
    idx_size = (data_ofs + sizeof(OBJ_ID_INDEX_DATA) + 7) & ~7;
    idx_entry_new = ntfs_calloc(idx_size);
    if (!idx_entry_new)
        return -errno;
    idx_entry_new->data_offset = cpu_to_le16(data_ofs);
    idx_entry_new->data_length = const_cpu_to_le16(sizeof(OBJ_ID_INDEX_DATA));
    idx_entry_new->length = cpu_to_le16(idx_size);
    idx_entry_new->key_length = const_cpu_to_le16(sizeof(GUID));
    idx_entry_new->key.object_id = *guid;
    oi = (OBJ_ID_INDEX_DATA*)((u8*)idx_entry_new + data_ofs);
    oi->mft_reference = ref;
    err = insert_index_entry_in_res_dir_index(idx_entry_new, idx_size, m,
            NTFS_INDEX_O, 2, AT_UNUSED);
    free(idx_entry_new);
    if (err < 0) {
        C_LOG_WARNING("index_obj_id_insert failed inserting index entry: %s", strerror(-err));
        return err;
    }
    return 0;
}

static void mkntfs_cleanup(void)
{
    struct BITMAP_ALLOCATION *p, *q;

    /* Close the volume */
    if (gsVol) {
        if (gsVol->dev) {
            if (NDevOpen(gsVol->dev) && gsVol->dev->d_ops->close(gsVol->dev)) {
                C_LOG_WARNING("Warning: Could not close %s", gsVol->dev->d_name);
            }
            ntfs_device_free(gsVol->dev);
        }
        free(gsVol->vol_name);
        free(gsVol->attrdef);
        free(gsVol->upcase);
        free(gsVol);
        gsVol = NULL;
    }

    /* Free any memory we've used */
    free(gsBadBlocks);          gsBadBlocks     = NULL;
    free(gsBuf);                gsBuf           = NULL;
    free(gsIndexBlock);         gsIndexBlock    = NULL;
    free(gsDynamicBuf);         gsDynamicBuf    = NULL;
    free(gsMftBitmap);          gsMftBitmap     = NULL;
    free(gsRlBad);              gsRlBad         = NULL;
    free(gsRlBoot);             gsRlBoot        = NULL;
    free(gsRlLogfile);          gsRlLogfile     = NULL;
    free(gsRlMft);              gsRlMft         = NULL;
    free(gsRlMftBmp);           gsRlMftBmp      = NULL;
    free(gsRlMftmirr);          gsRlMftmirr     = NULL;

    p = gsAllocation;
    while (p) {
        q = p->next;
        free(p);
        p = q;
    }
}

static BOOL mkntfs_open_partition(ntfs_volume *vol, const char* devName)
{
    BOOL result = FALSE;
    struct stat sbuf;
    unsigned long mnt_flags;

    /**
     * 此处 设备读写，对 Linux C 读写等函数做了封装，所有信息放到 vol->dev 中
     */
    vol->dev = ntfs_device_alloc(devName, 0, &ntfs_device_default_io_ops, NULL);
    if (!vol->dev) {
        C_LOG_WARNING("Could not create device");
        goto done;
    }

    /**
     * opts.no_action 命令行输入，是否执行实际的格式化磁盘操作
     */

    /**
     * 打开磁盘
     */
    if (vol->dev->d_ops->open(vol->dev, O_RDWR)) {
        if (errno == ENOENT) {
            C_LOG_WARNING("The device doesn't exist; did you specify it correctly?");
        }
        else {
            C_LOG_WARNING("Could not open %s", vol->dev->d_name);
        }
        goto done;
    }

    /**
     * 获取磁盘信息，相当于执行 stat 操作
     */
    if (vol->dev->d_ops->stat(vol->dev, &sbuf)) {
        C_LOG_WARNING("Error getting information about %s", vol->dev->d_name);
        goto done;
    }

    if (!S_ISBLK(sbuf.st_mode)) {
        if (!sbuf.st_size && !sbuf.st_blocks) {
            C_LOG_WARNING("You must specify the number of sectors.");
            goto done;
        }
#ifdef HAVE_LINUX_MAJOR_H
    }
    else if ((IDE_DISK_MAJOR(MAJOR(sbuf.st_rdev)) && MINOR(sbuf.st_rdev) % 64 == 0)
        || (SCSI_DISK_MAJOR(MAJOR(sbuf.st_rdev)) && MINOR(sbuf.st_rdev) % 16 == 0)) {
        C_LOG_WARNING("%s is entire device, not just one partition.", vol->dev->d_name);
#endif
    }

    /**
     * 根据 /etc/mtab 确认是否挂载
     */
    if (ntfs_check_if_mounted(vol->dev->d_name, &mnt_flags)) {
        C_LOG_WARNING("Failed to determine whether %s is mounted", vol->dev->d_name);
    }
    else if (mnt_flags & NTFS_MF_MOUNTED) {
        C_LOG_WARNING("%s is mounted.", vol->dev->d_name);
        C_LOG_WARNING("format forced anyway. Hope /etc/mtab is incorrect.");
    }
    result = TRUE;

done:
    return result;
}

static long mkntfs_get_page_size(void)
{
    long page_size;
#ifdef _SC_PAGESIZE
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0)
#endif
    {
        C_LOG_WARNING("Failed to determine system page size. Assuming safe default of 4096 bytes.");
        return 4096;
    }
    C_LOG_VERB("System page size is %li bytes.", page_size);
    return page_size;
}

static BOOL mkntfs_override_vol_params(ntfs_volume *vol)
{
    s64 volume_size;
    long page_size;
    int i;
    BOOL winboot = TRUE;

    /* If user didn't specify the sector size, determine it now. */
    opts.sectorSize = 512;

    /* Validate sector size. */
    if ((opts.sectorSize - 1) & opts.sectorSize) {
        C_LOG_WARNING("The sector size is invalid.  It must be a power of two, e.g. 512, 1024.");
        return FALSE;
    }

    if (opts.sectorSize < 256 || opts.sectorSize > 4096) {
        C_LOG_WARNING("The sector size is invalid.  The minimum size is 256 bytes and the maximum is 4096 bytes.");
        return FALSE;
    }

    C_LOG_VERB("sector size = %ld bytes", opts.sectorSize);

    /**
     * 设置扇区大小
     */
    if (ntfs_device_block_size_set(vol->dev, (int) opts.sectorSize)) {
        C_LOG_VERB("Failed to set the device block size to the sector size.  This may cause problems when creating the backup boot sector and also may affect performance but should be harmless otherwise.  Error: %s", strerror(errno));
    }

    /**
     * 扇区数量：测盘总大小 / 扇区大小
     */
    opts.numSectors = ntfs_device_size_get(vol->dev, (int) opts.sectorSize);
    if (opts.numSectors <= 0) {
        C_LOG_WARNING("Couldn't determine the size of %s.  Please specify the number of sectors manually.", vol->dev->d_name);
        return FALSE;
    }
    C_LOG_VERB("number of sectors = %lld (0x%llx)", opts.numSectors, opts.numSectors);

    /**
     * 预留最后一个扇区作为备份引导扇区
     *
     * 如果扇区大小小于512字节，则预留512字节的扇区。
     */
    i = 1;
    if (opts.sectorSize < 512) {
        i = 512 / opts.sectorSize;
    }
    opts.numSectors -= i;

    /* If user didn't specify the partition start sector, determine it. */
    if (opts.partStartSect < 0) {
        opts.partStartSect = ntfs_device_partition_start_sector_get(vol->dev);    // linux/hdreg.h 中获取磁盘扇区开始位置
        if (opts.partStartSect < 0) {
            C_LOG_INFO("The partition start sector was not specified for %s and it could not be obtained automatically. It has been set to 0.", vol->dev->d_name);
            opts.partStartSect = 0;
            winboot = FALSE;
        }
        else if (opts.partStartSect >> 32) {
            C_LOG_WARNING("The partition start sector was not specified for %s and the automatically determined value is too large (%lld). It has been set to 0.", vol->dev->d_name, (long long)opts.partStartSect);
            opts.partStartSect = 0;
            winboot = FALSE;
        }
    }
    else if (opts.partStartSect >> 32) {
        C_LOG_WARNING("Invalid partition start sector.  Maximum is 4294967295 (2^32-1).");
        return FALSE;
    }

    /* If user didn't specify the sectors per track, determine it now. */
    /**
     * 每个track多少扇区
     */
    if (opts.sectorsPerTrack < 0) {
        opts.sectorsPerTrack = ntfs_device_sectors_per_track_get(vol->dev);
        if (opts.sectorsPerTrack < 0) {
            C_LOG_INFO("The number of sectors per track was not specified for %s and it could not be obtained automatically.It has been set to 0.", vol->dev->d_name);
            opts.sectorsPerTrack = 0;
            winboot = FALSE;
        }
        else if (opts.sectorsPerTrack > 65535) {
            C_LOG_WARNING("The number of sectors per track was not specified for %s and the automatically determined value is too large.  It has been set to 0.", vol->dev->d_name);
            opts.sectorsPerTrack = 0;
            winboot = FALSE;
        }
    }
    else if (opts.sectorsPerTrack > 65535) {
        C_LOG_WARNING("Invalid number of sectors per track.  Maximum is 65535.");
        return FALSE;
    }

    /**
     * 磁盘磁头数量
     */
    if (opts.heads < 0) {
        opts.heads = ntfs_device_heads_get(vol->dev);
        if (opts.heads < 0) {
            C_LOG_INFO("The number of heads was not specified for %s and it could not be obtained automatically.It has been set to 0.", vol->dev->d_name);
            opts.heads = 0;
            winboot = FALSE;
        }
        else if (opts.heads > 65535) {
            C_LOG_WARNING("The number of heads was not specified for %s and the automatically determined value is too large.It has been set to 0.", vol->dev->d_name);
            opts.heads = 0;
            winboot = FALSE;
        }
    }
    else if (opts.heads > 65535) {
        C_LOG_WARNING("Invalid number of heads.Maximum is 65535.");
        return FALSE;
    }
    volume_size = opts.numSectors * opts.sectorSize;  // 磁盘扇区数量 x 磁盘扇区大小 = 磁盘总容量
    gVolumeSize = volume_size;

    /**
     * 磁盘容量需大于 1MB
     */
    if (volume_size < (1 << 20)) {            /* 1MiB */
        C_LOG_WARNING("Device is too small (%llikiB).  Minimum Sandbox FS volume size is 1MiB.", (long long)(volume_size / 1024));
        return FALSE;
    }
    C_LOG_VERB("volume size = %llikiB", (long long) (volume_size / 1024));

    /**
     * cluster size: 文件系统中最小的存储单元，也称 “簇” 或 “区块”
     * Windows Vista always uses 4096 bytes as the default cluster
     * size regardless of the volume size so we do it, too.
     */
    if (!vol->cluster_size) {
        vol->cluster_size = 4096;
        /* For small volumes on devices with large sector sizes. */
        if (vol->cluster_size < (u32)opts.sectorSize) {
            vol->cluster_size = opts.sectorSize;
        }
        /**
         * For huge volumes, grow the cluster size until the number of
         * clusters fits into 32 bits or the cluster size exceeds the
         * maximum limit of NTFS_MAX_CLUSTER_SIZE.
         */
        while (volume_size >> (ffs(vol->cluster_size) - 1 + 32)) {
            vol->cluster_size <<= 1;
            // 块大小不能大于 2MB
            if (vol->cluster_size >= NTFS_MAX_CLUSTER_SIZE) {
                C_LOG_WARNING("Device is too large to hold an Sandbox FS volume (maximum size is 256TiB).");
                return FALSE;
            }
        }
        C_LOG_VERB("Cluster size has been automatically set to %u bytes.", (unsigned)vol->cluster_size);
    }

    /**
     * 检测块大小是否是 2 的指数次
     */
    if (vol->cluster_size & (vol->cluster_size - 1)) {
        C_LOG_WARNING("The cluster size is invalid.It must be a power of two, e.g. 1024, 4096.");
        return FALSE;
    }
    if (vol->cluster_size < (u32)opts.sectorSize) {
        C_LOG_WARNING("The cluster size is invalid.  It must be equal to, or larger than, the sector size.");
        return FALSE;
    }

    /* Before Windows 10 Creators, the limit was 128 */
    if (vol->cluster_size > 4096 * (u32)opts.sectorSize) {
        C_LOG_WARNING("The cluster size is invalid.  It cannot be more that 4096 times the size of the sector size.");
        return FALSE;
    }

    if (vol->cluster_size > NTFS_MAX_CLUSTER_SIZE) {
        C_LOG_WARNING("The cluster size is invalid.  The maximum cluster size is %lu bytes (%lukiB).", (unsigned long)NTFS_MAX_CLUSTER_SIZE, (unsigned long)(NTFS_MAX_CLUSTER_SIZE >> 10));
        return FALSE;
    }

    vol->cluster_size_bits = ffs(vol->cluster_size) - 1;
    C_LOG_VERB("cluster size = %u bytes", (unsigned int)vol->cluster_size);
    if (vol->cluster_size > 4096) {
        C_LOG_WARNING("Cannot use compression when the cluster size is larger than 4096 bytes. Compression has been disabled for this volume.");
    }

    // 簇/块 数量
    vol->nr_clusters = volume_size / vol->cluster_size;

    /*
     * Check the cluster_size and num_sectors for consistency with
     * sector_size and num_sectors. And check both of these for consistency
     * with volume_size.
     */
    if ((vol->nr_clusters != ((opts.numSectors * opts.sectorSize) / vol->cluster_size)
        || (volume_size / opts.sectorSize) != opts.numSectors
        || (volume_size / vol->cluster_size) != vol->nr_clusters)) {
        /* XXX is this code reachable? */
        C_LOG_WARNING("Illegal combination of volume/cluster/sector size and/or cluster/sector number.");
        return FALSE;
    }
    C_LOG_VERB("number of clusters = %llu (0x%llx)", (unsigned long long)vol->nr_clusters, (unsigned long long)vol->nr_clusters);

    /* Number of clusters must fit within 32 bits (Win2k limitation). */
    if (vol->nr_clusters >> 32) {
        if (vol->cluster_size >= 65536) {
            C_LOG_WARNING("Device is too large to hold an NTFS volume (maximum size is 256TiB).");
            return FALSE;
        }
        C_LOG_WARNING("Number of clusters exceeds 32 bits.  Please try again with a largercluster size or leave the cluster size unspecified and the smallest possible cluster size for the size of the device will be used.");
        return FALSE;
    }
    page_size = mkntfs_get_page_size();     // memory page size: 4096

    /**
     * Set the mft record size.  By default this is 1024 but it has to be
     * at least as big as a sector and not bigger than a page on the system
     * or the NTFS kernel driver will not be able to mount the volume.
     * TODO: The mft record size should be user specifiable just like the
     * "inode size" can be specified on other Linux/Unix file systems.
     *
     * Master File Table(MFT)的大小
     *
     * MFT用来管理文件和目录的元数据信息，MFT中每一条记录对应一个文件或目录
     *  mft_record_size 是 NTFS 中每一条MFT记录的大小
     *  较小的MFT减少磁盘空间浪费，增加MFT查找和访问开销
     *  较大的MFT提高MFT的访问性能，但会浪费更多的磁盘空间
     */
    vol->mft_record_size = 1024;
    if (vol->mft_record_size < (u32)opts.sectorSize) {
        vol->mft_record_size = opts.sectorSize;
    }
    if (vol->mft_record_size > (unsigned long)page_size) {
        C_LOG_WARNING("Mft record size (%u bytes) exceeds system page size (%li bytes). You will not be able to mount this volume using the Sandbox FS kernel driver.", (unsigned)vol->mft_record_size, page_size);
    }

    // ffs 位操作中用于定位下一个可用的位（从低位开始，下一个可被设位1的位置）
    // 默认值：10
    vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;
    C_LOG_VERB("mft record size = %u bytes", (unsigned)vol->mft_record_size);

    /**
     * Set the index record size.  By default this is 4096 but it has to be
     * at least as big as a sector and not bigger than a page on the system
     * or the NTFS kernel driver will not be able to mount the volume.
     * FIXME: Should we make the index record size to be user specifiable?
     *
     * NTFS使用索引来加快文件和目录的查找和访问。索引记录保存了文件和目录的元数据信息
     * index_record_size 就是NTFS中每个索引记录的大小
     *  较小的索引大小可以减少磁盘空间的浪费，但是增加了索引查找的开销
     *  较大的索引大小可以提高索引的查找性能，但会浪费更多的磁盘空间
     */
    vol->indx_record_size = 4096;
    if (vol->indx_record_size < (u32)opts.sectorSize) {
        vol->indx_record_size = opts.sectorSize;
    }

    if (vol->indx_record_size > (unsigned long)page_size) {
        C_LOG_WARNING("Index record size (%u bytes) exceeds system page size (%li bytes).  You will not be able to mount this volume using the Sandbox FS kernel driver.", (unsigned)vol->indx_record_size, page_size);
    }
    vol->indx_record_size_bits = ffs(vol->indx_record_size) - 1;
    C_LOG_VERB("index record size = %u bytes", (unsigned)vol->indx_record_size);
    if (!winboot) {
        C_LOG_WARNING("To boot from a device, Needs the 'partition start sector', the 'sectors per track' and the 'number of heads' to be set.");
        C_LOG_WARNING("If not, You will not be able to boot from this device.");
    }
    return TRUE;
}

/**
 * @brief mkntfs_initialize_bitmaps -
 * 初始化 NTFS 文件系统的位图数据结构
 *
 * NTFS 文件系统使用位图来跟踪磁盘空间的使用情况。每一个位代表磁盘上的一个簇(cluster)是否被使用
 * 此函数负责初始化位图数据结构，将整个磁盘分区的所有簇对应的位全部标记为"未使用"状态
 */
static BOOL mkntfs_initialize_bitmaps(void)
{
    u64 i;
    int mft_bitmap_size;

    /* Determine lcn bitmap byte size and allocate it. */
    /**
     * (扇区数量 + 7) / 8
     */
    gsLcnBitmapByteSize = (gsVol->nr_clusters + 7) >> 3;

    /* Needs to be multiple of 8 bytes. */
    gsLcnBitmapByteSize = (gsLcnBitmapByteSize + 7) & ~7;                             // 8的倍数
    i = (gsLcnBitmapByteSize + gsVol->cluster_size - 1) & ~(gsVol->cluster_size - 1);    // cluster 大小的倍数
    C_LOG_VERB("g_lcn_bitmap_byte_size = %i, allocated = %llu", gsLcnBitmapByteSize, (unsigned long long)i);
    gsDynamicBufSize = mkntfs_get_page_size(); // 4096
    gsDynamicBuf = (u8*)ntfs_calloc(gsDynamicBufSize);
    if (!gsDynamicBuf) {
        return FALSE;
    }

    /**
     * $Bitmap can overlap the end of the volume. Any bits in this region
     * must be set. This region also encompasses the backup boot sector.
     *
     * $Bitmap可以重叠卷的末尾。该区域内的所有位都必须设置。
     * 该区域还包括引导扇区的备份。
     *
     * 簇大小 x 8 - 簇数量
     *
     * 每 位 代表一个 族/块
     * 生成一个 struct BITMAP_ALLOCATION 结构，并指向 g_allocation
     */
    if (!bitmap_allocate(gsVol->nr_clusters, ((s64)gsLcnBitmapByteSize << 3) - gsVol->nr_clusters)) {
        return (FALSE);
    }

    /**
     * Mft size is 27 (NTFS 3.0+) mft records or one cluster, whichever is bigger.
     *
     * Mft 大小取 27 条 mft 所占空间 与 一簇 大小 中 最大者。
     */
    gsMftSize = 27;
    gsMftSize *= gsVol->mft_record_size; // 27K
    if (gsMftSize < (s32)gsVol->cluster_size) {
        gsMftSize = gsVol->cluster_size;
    }
    C_LOG_VERB("MFT size = %i (0x%x) bytes", gsMftSize, gsMftSize);

    /* Determine mft bitmap size and allocate it. */
    mft_bitmap_size = gsMftSize / gsVol->mft_record_size;      // 7

    /* Convert to bytes, at least one. */
    gsMftBitmapByteSize = (mft_bitmap_size + 7) >> 3;        // 1

    /* Mft bitmap is allocated in multiples of 8 bytes. */
    gsMftBitmapByteSize = (gsMftBitmapByteSize + 7) & ~7; // 8
    C_LOG_VERB("mft_bitmap_size = %i, g_mft_bitmap_byte_size = %i", mft_bitmap_size, gsMftBitmapByteSize);
    gsMftBitmap = ntfs_calloc(gsMftBitmapByteSize);
    if (!gsMftBitmap) {
        return FALSE;
    }

    /* Create runlist for mft bitmap. */
    gsRlMftBmp = ntfs_malloc(2 * sizeof(runlist));
    if (!gsRlMftBmp) {
        return FALSE;
    }
    gsRlMftBmp[0].vcn = 0LL;

    /* Mft bitmap is right after $Boot's data. */
    // Mft bitmap 位于$Boot的数据之后。
    i = (8192 + gsVol->cluster_size - 1) / gsVol->cluster_size;     // (8192 + 一个簇/块大小 - 1) / 一个簇/块大小 = 2
    gsRlMftBmp[0].lcn = i;

    /*
     * Size is always one cluster, even though valid data size and
     * initialized data size are only 8 bytes.
     */
    gsRlMftBmp[1].vcn = 1LL;
    gsRlMftBmp[0].length = 1LL;
    gsRlMftBmp[1].lcn = -1LL;
    gsRlMftBmp[1].length = 0LL;

    /* Allocate cluster for mft bitmap. */
    return (bitmap_allocate(i, 1));
}

bool mkntfs_init_sandbox_header(ntfs_volume *vol)
{
    g_return_val_if_fail(vol != NULL && vol->dev != NULL, false);

    bool hasErr = false;
    EfsSandboxFileHeader* header = ntfs_malloc(sizeof(EfsSandboxFileHeader));
    if (!header) {
        C_LOG_ERROR("Failed to allocate memory for EfsSandboxFileHeader");
        goto done;
    }

    memset(header, 0, sizeof(EfsSandboxFileHeader));

    header->fileHeader.version = SANDBOX_VERSION;
    header->fileHeader.headSize = sizeof(EfsSandboxFileHeader);
    header->fileHeader.fileType = FILE_TYPE_SANDBOX;

    C_LOG_WARNING("sandbox: %d, %d, %d, %d, %d",
        sizeof (EfsSandboxFileHeader), sizeof(header->ps), sizeof(header->pe), sizeof(SANDBOX_EFS_HEADER_START), sizeof(SANDBOX_EFS_HEADER_END));

    char* bufT = header->ps;
    int i = 0, j = 0;
    for (i = 0; i < sizeof(header->ps); i += (sizeof(SANDBOX_EFS_HEADER_START) - 1)) {
        memcpy(bufT + i, SANDBOX_EFS_HEADER_START, sizeof(SANDBOX_EFS_HEADER_START) - 1);
    }

    char* bufE = header->pe;
    for (j = 0; j < sizeof(header->pe); j += (sizeof(SANDBOX_EFS_HEADER_END) - 1)) {
        memcpy(bufE + j, SANDBOX_EFS_HEADER_END, sizeof(SANDBOX_EFS_HEADER_END) -1);
    }

    C_LOG_VERB("Sandbox start: %d, end: %d", i, j);

    // 初始化最后磁盘内容
    s64 sizeA = ntfs_device_size_get_all_size(vol->dev);
    C_LOG_VERB("Sandbox size: %lld", sizeA);
    s64 n = (s64) sizeA - gVolumeSize;
    vol->dev->d_ops->seek(vol->dev, (int64_t) gVolumeSize, SEEK_SET);
    for (; n > 0; n--) { vol->dev->d_ops->write(vol->dev, "\0", 1); };

    errno = 0;
    vol->dev->d_ops->seek(vol->dev, (int64_t) gVolumeSize + 1024, SEEK_SET);
    if (vol->dev->d_ops->write(vol->dev, header, sizeof(EfsSandboxFileHeader)) != sizeof(EfsSandboxFileHeader)) {
        C_LOG_WARNING("write efs header error: %s", strerror(errno));
        hasErr = true;
        goto done;
    }
    vol->dev->d_ops->sync(vol->dev);

done:
    if (header) {
        ntfs_free(header);
    }
    return !hasErr;
}

static bool found_efs_header(ntfs_volume* vol, EfsSandboxFileHeader* header)
{
    g_return_val_if_fail(vol != NULL && vol->dev != NULL && NULL != header, false);

    s64 dSize = ntfs_device_size_get_all_size(vol->dev);
    s64 startP = dSize - (s64) sizeof(EfsSandboxFileHeader);

    bool isOK = false;
    do {
        vol->dev->d_ops->seek(vol->dev, startP, SEEK_SET);
        s64 rS = vol->dev->d_ops->read(vol->dev, header, sizeof(EfsSandboxFileHeader));
        if (rS != sizeof(EfsSandboxFileHeader)) {
            C_LOG_WARNING("read error");
            break;
        }

        if (0 == strncmp(header->ps, SANDBOX_EFS_HEADER_START_STR, sizeof(SANDBOX_EFS_HEADER_START_STR) - 1)) {
            for (int j = 0; j < (int) sizeof(header->pe); j++) {
                if (0 == strncmp(header->pe + j * (sizeof(SANDBOX_EFS_HEADER_END) - 1), SANDBOX_EFS_HEADER_END, sizeof(SANDBOX_EFS_HEADER_END) - 1)) {
                    isOK = true;
                    break;
                }
            }
        }
        --startP;
    } while (startP >= gVolumeSize);

    return isOK;
}

bool check_efs_header(ntfs_volume * vol)
{
    g_return_val_if_fail(vol != NULL && vol->dev != NULL, false);

    EfsSandboxFileHeader header;

    if (!found_efs_header(vol, &header)) {
        C_LOG_WARNING("Cannot find efs header");
        return false;
    }

    // fixme:// 比较

    return true;
}

/**
 * @brief mkntfs_initialize_rl_mft -
 *  MFT 是 NTFS 文件系统的核心数据结构，用于存储文件和目录的元数据信息
 *  MFT 本身也是一个文件，它的数据可能会被分散存储在磁盘的多个不连续位置（称为 簇）
 *  为了描述 MFT 文件在磁盘上的实际分布情况，NTFS使用了一种称为“运行列表”(Runlist)的数据结构
 *  mkntfs_initialize_rl_mft 函数的作用就是初始化这个 MFT 文件的运行列表。它会分配一个初始的连续磁盘空间，并将其记录到运行列表中
 *  这个步骤确保了MFT文件在刚创建时候有一个良好的磁盘分布，有助于提高 MFT 的访问性能
 *  随着文件系统的使用，MFT文件可能会被碎片化，需要进一步优化...
 */
static BOOL mkntfs_initialize_rl_mft(void)
{
    int j;
    BOOL done;

    /* If user didn't specify the mft lcn, determine it now. */
    if (!gsMftLcn) {
        /**
         * We start at the higher value out of 16kiB and just after the
         * mft bitmap.
         */
        gsMftLcn = gsRlMftBmp[0].lcn + gsRlMftBmp[0].length;
        if (gsMftLcn * gsVol->cluster_size < 16 * 1024) {
            gsMftLcn = (16 * 1024 + gsVol->cluster_size - 1) / gsVol->cluster_size;
        }
    }
    C_LOG_VERB("$MFT logical cluster number = 0x%llx", gsMftLcn);

    /* Determine MFT zone size. */
    gsMftZoneEnd = gsVol->nr_clusters;
    switch (opts.mftZoneMultiplier) {  /* % of volume size in clusters */
        case 4: {
            gsMftZoneEnd = gsMftZoneEnd >> 1;    /* 50%   */
            break;
        }
        case 3: {
            gsMftZoneEnd = gsMftZoneEnd * 3 >> 3;/* 37.5% */
            break;
        }
        case 2: {
            gsMftZoneEnd = gsMftZoneEnd >> 2;    /* 25%   */
            break;
        }
        case 1:
        default: {
            gsMftZoneEnd = gsMftZoneEnd >> 3;    /* 12.5% */
            break;
        }
    }
    C_LOG_VERB("MFT zone size = %lldkiB", gsMftZoneEnd << gsVol->cluster_size_bits >> 10 /* >> 10 == / 1024 */);

    /*
     * The mft zone begins with the mft data attribute, not at the beginning
     * of the device.
     */
    gsMftZoneEnd += gsMftLcn;

    /* Create runlist for mft. */
    gsRlMft = ntfs_malloc(2 * sizeof(runlist));
    if (!gsRlMft) {
        return FALSE;
    }

    gsRlMft[0].vcn = 0LL;
    gsRlMft[0].lcn = gsMftLcn;

    /* rounded up division by cluster size */
    j = (gsMftSize + gsVol->cluster_size - 1) / gsVol->cluster_size;
    gsRlMft[1].vcn = j;
    gsRlMft[0].length = j;
    gsRlMft[1].lcn = -1LL;
    gsRlMft[1].length = 0LL;

    /* Allocate clusters for mft. */
    bitmap_allocate(gsMftLcn,j);

    /* Determine mftmirr_lcn (middle of volume). */
    gsMftmirrLcn = (opts.numSectors * opts.sectorSize >> 1) / gsVol->cluster_size;
    C_LOG_VERB("$MFTMirr logical cluster number = 0x%llx", gsMftmirrLcn);

    /* Create runlist for mft mirror. */
    gsRlMftmirr = ntfs_malloc(2 * sizeof(runlist));
    if (!gsRlMftmirr) {
        return FALSE;
    }

    gsRlMftmirr[0].vcn = 0LL;
    gsRlMftmirr[0].lcn = gsMftmirrLcn;
    /*
     * The mft mirror is either 4kb (the first four records) or one cluster
     * in size, which ever is bigger. In either case, it contains a
     * byte-for-byte identical copy of the beginning of the mft (i.e. either
     * the first four records (4kb) or the first cluster worth of records,
     * whichever is bigger).
     */
    j = (4 * gsVol->mft_record_size + gsVol->cluster_size - 1) / gsVol->cluster_size;
    gsRlMftmirr[1].vcn = j;
    gsRlMftmirr[0].length = j;
    gsRlMftmirr[1].lcn = -1LL;
    gsRlMftmirr[1].length = 0LL;

    /* Allocate clusters for mft mirror. */
    done = bitmap_allocate(gsMftmirrLcn,j);
    gsLogfileLcn = gsMftmirrLcn + j;
    C_LOG_VERB("$LogFile logical cluster number = 0x%llx", gsLogfileLcn);

    return (done);
}

static BOOL mkntfs_initialize_rl_logfile(void)
{
    int j;
    u64 volume_size;

    /* Create runlist for log file. */
    gsRlLogfile = ntfs_malloc(2 * sizeof(runlist));
    if (!gsRlLogfile) {
        return FALSE;
    }

    volume_size = gsVol->nr_clusters << gsVol->cluster_size_bits;

    gsRlLogfile[0].vcn = 0LL;
    gsRlLogfile[0].lcn = gsLogfileLcn;
    /*
     * Determine logfile_size from volume_size (rounded up to a cluster),
     * making sure it does not overflow the end of the volume.
     */
    if (volume_size < 2048LL * 1024) {                  /* < 2MiB    */
        gsLogfileSize = 256LL * 1024;                  /*   -> 256kiB    */
    }
    else if (volume_size < 4000000LL) {                 /* < 4MB    */
        gsLogfileSize = 512LL * 1024;                  /*   -> 512kiB    */
    }
    else if (volume_size <= 200LL * 1024 * 1024) {      /* < 200MiB    */
        gsLogfileSize = 2048LL * 1024;                 /*   -> 2MiB    */
    }
    else {
        /*
         * FIXME: The $LogFile size is 64 MiB upwards from 12GiB but
         * the "200" divider below apparently approximates "100" or
         * some other value as the volume size decreases. For example:
         *      Volume size   LogFile size    Ratio
         *      8799808        46048       191.100
         *      8603248        45072       190.877
         *      7341704        38768       189.375
         *      6144828        32784       187.433
         *      4192932        23024       182.111
         */
        if (volume_size >= 12LL << 30) {                /* > 12GiB    */
            gsLogfileSize = 64 << 20;                  /*   -> 64MiB    */
        }
        else {
            gsLogfileSize = (volume_size / 200) & ~(gsVol->cluster_size - 1);
        }
    }
    j = gsLogfileSize / gsVol->cluster_size;
    while (gsRlLogfile[0].lcn + j >= gsVol->nr_clusters) {
        /*
         * $Logfile would overflow volume. Need to make it smaller than
         * the standard size. It's ok as we are creating a non-standard
         * volume anyway if it is that small.
         */
        gsLogfileSize >>= 1;
        j = gsLogfileSize / gsVol->cluster_size;
    }
    gsLogfileSize = (gsLogfileSize + gsVol->cluster_size - 1) & ~(gsVol->cluster_size - 1);
    C_LOG_VERB("$LogFile (journal) size = %ikiB", gsLogfileSize / 1024);
    /*
     * FIXME: The 256kiB limit is arbitrary. Should find out what the real
     * minimum requirement for Windows is so it doesn't blue screen.
     */
    if (gsLogfileSize < 256 << 10) {
        C_LOG_WARNING("$LogFile would be created with invalid size. This is not allowed as it would cause Windows to blue screen and during boot.");
        return FALSE;
    }
    gsRlLogfile[1].vcn = j;
    gsRlLogfile[0].length = j;
    gsRlLogfile[1].lcn = -1LL;
    gsRlLogfile[1].length = 0LL;

    /* Allocate clusters for log file. */
    return (bitmap_allocate(gsLogfileLcn,j));
}

static BOOL mkntfs_initialize_rl_boot(void)
{
    int j;
    /* Create runlist for $Boot. */
    gsRlBoot = ntfs_malloc(2 * sizeof(runlist));
    if (!gsRlBoot) {
        return FALSE;
    }

    gsRlBoot[0].vcn = 0LL;
    gsRlBoot[0].lcn = 0LL;
    /*
     * $Boot is always 8192 (0x2000) bytes or 1 cluster, whichever is
     * bigger.
     */
    j = (8192 + gsVol->cluster_size - 1) / gsVol->cluster_size;
    gsRlBoot[1].vcn = j;
    gsRlBoot[0].length = j;        // 2
    gsRlBoot[1].lcn = -1LL;
    gsRlBoot[1].length = 0LL;

    /* Allocate clusters for $Boot. */
    return (bitmap_allocate(0, j));
}

static BOOL mkntfs_initialize_rl_bad(void)
{
    /* Create runlist for $BadClus, $DATA named stream $Bad. */
    gsRlBad = ntfs_malloc(2 * sizeof(runlist));
    if (!gsRlBad) {
        return FALSE;
    }

    gsRlBad[0].vcn = 0LL;
    gsRlBad[0].lcn = -1LL;
    /*
     * $BadClus named stream $Bad contains the whole volume as a single
     * sparse runlist entry.
     */
    gsRlBad[1].vcn = gsVol->nr_clusters;
    gsRlBad[0].length = gsVol->nr_clusters;
    gsRlBad[1].lcn = -1LL;
    gsRlBad[1].length = 0LL;

    /* TODO: Mark bad blocks as such. */
    return TRUE;
}

static BOOL mkntfs_fill_device_with_zeroes(void)
{
    /**
     * If not quick format, fill the device with 0s.
     * FIXME: Except bad blocks! (AIA)
     */
    int i;
    ssize_t bw;
    unsigned long long position;
    float progress_inc = (float)gsVol->nr_clusters / 100;       // 进度条 步长
    u64 volume_size;

    volume_size = gsVol->nr_clusters << gsVol->cluster_size_bits;

    ntfs_log_progress("Initializing device with zeroes:   0%%");
    for (position = 0; position < (unsigned long long)gsVol->nr_clusters; position++) {
        if (!(position % (int)(progress_inc + 1))) {
            ntfs_log_progress("\b\b\b\b%3.0f%%", position / progress_inc);
        }

        bw = mkntfs_write(gsVol->dev, gsBuf, gsVol->cluster_size);
        if (bw != (ssize_t)gsVol->cluster_size) {
            if (bw != -1 || errno != EIO) {
                C_LOG_WARNING("This should not happen.");
                return FALSE;
            }

            if (!position) {
                C_LOG_WARNING("Error: Cluster zero is bad. Cannot create NTFS file system.");
                return FALSE;
            }

            /* Add the baddie to our bad blocks list. */
            if (!append_to_bad_blocks(position)) {
                return FALSE;
            }
            ntfs_log_quiet("Found bad cluster (%lld). Adding to list of bad blocks.Initializing device with zeroes: %3.0f%%", position, position / progress_inc);

            /* Seek to next cluster. */
            gsVol->dev->d_ops->seek(gsVol->dev, ((off_t) position + 1) * gsVol->cluster_size, SEEK_SET);
        }
    }
    ntfs_log_progress("\b\b\b\b100%%");

    position = (volume_size & (gsVol->cluster_size - 1)) / opts.sectorSize;
    for (i = 0; (unsigned long)i < position; i++) {
        bw = mkntfs_write(gsVol->dev, gsBuf, opts.sectorSize);
        if (bw != opts.sectorSize) {
            if (bw != -1 || errno != EIO) {
                C_LOG_WARNING("This should not happen.");
                return FALSE;
            }
            else if (i + 1ull == position) {
                C_LOG_WARNING("Error: Bad cluster found in location reserved for system file $Boot.");
                return FALSE;
            }
            /* Seek to next sector. */
            gsVol->dev->d_ops->seek(gsVol->dev, opts.sectorSize, SEEK_CUR);
        }
    }
    ntfs_log_progress(" - Done.");

    return TRUE;
}

static BOOL mkntfs_sync_index_record(INDEX_ALLOCATION* idx, MFT_RECORD* m, ntfschar* name, u32 name_len)
{
    int i, err;
    ntfs_attr_search_ctx *ctx;
    ATTR_RECORD *a;
    long long lw;
    runlist* rl_index = NULL;

    i = 5 * sizeof(ntfschar);
    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        C_LOG_WARNING("Failed to allocate attribute search context");
        return FALSE;
    }
    /* FIXME: This should be IGNORE_CASE! */
    if (mkntfs_attr_lookup(AT_INDEX_ALLOCATION, name, name_len, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        ntfs_attr_put_search_ctx(ctx);
        C_LOG_WARNING("BUG: $INDEX_ALLOCATION attribute not found.");
        return FALSE;
    }
    a = ctx->attr;
    rl_index = ntfs_mapping_pairs_decompress(gsVol, a, NULL);
    if (!rl_index) {
        ntfs_attr_put_search_ctx(ctx);
        C_LOG_WARNING("Failed to decompress runlist of $INDEX_ALLOCATION attribute.");
        return FALSE;
    }
    if (sle64_to_cpu(a->initialized_size) < i) {
        ntfs_attr_put_search_ctx(ctx);
        free(rl_index);
        C_LOG_WARNING("BUG: $INDEX_ALLOCATION attribute too short.");
        return FALSE;
    }
    ntfs_attr_put_search_ctx(ctx);
    i = sizeof(INDEX_BLOCK) - sizeof(INDEX_HEADER) + le32_to_cpu(idx->index.allocated_size);
    err = ntfs_mst_pre_write_fixup((NTFS_RECORD*)idx, i);
    if (err) {
        free(rl_index);
        C_LOG_WARNING("ntfs_mst_pre_write_fixup() failed while syncing index block.");
        return FALSE;
    }
    lw = ntfs_rlwrite(gsVol->dev, rl_index, (u8*)idx, i, NULL, WRITE_STANDARD);
    free(rl_index);
    if (lw != i) {
        C_LOG_WARNING("Error writing $INDEX_ALLOCATION.");
        return FALSE;
    }
    /* No more changes to @idx below here so no need for fixup: */
    /* ntfs_mst_post_write_fixup((NTFS_RECORD*)idx); */
    return TRUE;
}

static BOOL create_file_volume(MFT_RECORD *m, leMFT_REF root_ref, VOLUME_FLAGS fl, const GUID *volume_guid)
{
    int i, err;
    u8 *sd;

    C_LOG_VERB("Creating $Volume (mft record 3)");
    m = (MFT_RECORD*)(gsBuf + 3 * gsVol->mft_record_size);
    err = create_hardlink(gsIndexBlock, root_ref, m,
            MK_LE_MREF(FILE_Volume, FILE_Volume), 0LL, 0LL,
            FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
            "$Volume", FILE_NAME_WIN32_AND_DOS);
    if (!err) {
        init_system_file_sd(FILE_Volume, &sd, &i);
        err = add_attr_sd(m, sd, i);
    }
    if (!err) {
        err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), NULL, 0);
    }
    if (!err) {
        err = add_attr_vol_name(m, gsVol->vol_name, gsVol->vol_name ? strlen(gsVol->vol_name) : 0);
    }
    if (!err) {
        if (fl & VOLUME_IS_DIRTY) {
            ntfs_log_quiet("Setting the volume dirty so check disk runs on next reboot into Windows.");
        }
        err = add_attr_vol_info(m, fl, gsVol->major_ver, gsVol->minor_ver);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $Volume: %s", strerror(-err));
        return FALSE;
    }
    return TRUE;
}

static int create_backup_boot_sector(u8 *buff)
{
    const char *s;
    ssize_t bw;
    int size, e;

    C_LOG_VERB("Creating backup boot sector.");
    /**
     * 将(512, opts.sector_size) 大小的数据写到最后一个扇区，大小不超过 8192 因为这是Boot分区大小
     * Write the first max(512, opts.sector_size) bytes from buf to the
     * last sector, but limit that to 8192 bytes of written data since that
     * is how big $Boot is (and how big our buffer is)..
     */
    size = 512;
    if (size < opts.sectorSize) {
        size = opts.sectorSize;
    }
    if (gsVol->dev->d_ops->seek(gsVol->dev, (opts.numSectors + 1) * opts.sectorSize - size, SEEK_SET) == (off_t) -1) {
        C_LOG_WARNING("Seek failed");
        goto bb_err;
    }

    if (size > 8192) {
        size = 8192;
    }
    bw = mkntfs_write(gsVol->dev, buff, size);
    if (bw == size) {
        C_LOG_VERB("OK!");
        return 0;
    }
    e = errno;
    if (bw == -1LL) {
        s = strerror(e);
    }
    else {
        s = "unknown error";
    }
    /* At least some 2.4 kernels return EIO instead of ENOSPC. */
    if (bw != -1LL || (bw == -1LL && e != ENOSPC && e != EIO)) {
        C_LOG_WARNING("Couldn't write backup boot sector: %s", s);
        return -1;
    }

bb_err:
    C_LOG_WARNING("Couldn't write backup boot sector. This is due to a "
                "limitation in the Linux kernel. This is not a major "
                "problem as Windows check disk will create the"
                "backup boot sector when it is run on your next boot "
                "into Windows.");
    return -1;
}

static BOOL mkntfs_create_root_structures(void)
{
    NTFS_BOOT_SECTOR *bs;
    MFT_RECORD *m;
    leMFT_REF root_ref;
    leMFT_REF extend_ref;
    int i;
    int j;
    int err;
    u8 *sd;
    FILE_ATTR_FLAGS extend_flags;
    VOLUME_FLAGS volume_flags = const_cpu_to_le16(0);
    int sectors_per_cluster;
    int nr_sysfiles;
    int buf_sds_first_size;
    char *buf_sds;
    GUID vol_guid;

    ntfs_log_quiet("Creating Sandbox FS volume structures.");
    nr_sysfiles = 27;       // 系统 MFT
    /**
     * Setup an empty mft record.  Note, we can just give 0 as the mft
     * reference as we are creating an NTFS 1.2 volume for which the mft
     * reference is ignored by ntfs_mft_record_layout().
     *
     * Copy the mft record onto all 16 records in the buffer and setup the
     * sequence numbers of each system file to equal the mft record number
     * of that file (only for $MFT is the sequence number 1 rather than 0).
     *
     * 设置一个空的mft记录。注意，我们可以只给0作为mft引用，因为我们正在创建一个NTFS 1.2卷，ntfs_mft_record_layout()会忽略mft引用。
     * 将mft记录复制到缓冲区中的所有16条记录上，并设置每个系统文件的序列号，使其等于该文件的mft记录号(只有$mft是序列号1而不是0)。
     *
     * 0 ~ 27K
     */
    for (i = 0; i < nr_sysfiles; i++) {
        // 初始化 g_vol->mft_record_size，
        // MFT 结构体 初始化
        if (ntfs_mft_record_layout(gsVol, 0, m = (MFT_RECORD *)(gsBuf + i * gsVol->mft_record_size))) {
            C_LOG_WARNING("Failed to layout system mft records.");
            return FALSE;
        }

        // 0 与 24 ~ 27 ? 为什么
        if (i == 0 || i > 23) {
            m->sequence_number = const_cpu_to_le16(1);
        }
        else {
            m->sequence_number = cpu_to_le16(i);
        }
    }

    /**
     * If only one cluster contains all system files then
     * fill the rest of it with empty, formatted records.
     *
     * 27KB 最后一块...
     */
    if (nr_sysfiles * (s32)gsVol->mft_record_size < gsMftSize) {
        for (i = nr_sysfiles; i * (s32)gsVol->mft_record_size < gsMftSize; i++) {
            m = (MFT_RECORD *)(gsBuf + i * gsVol->mft_record_size);
            if (ntfs_mft_record_layout(gsVol, 0, m)) {
                C_LOG_WARNING("Failed to layout mft record.");
                return FALSE;
            }
            m->flags = const_cpu_to_le16(0);
            m->sequence_number = cpu_to_le16(i);
        }
    }

    /*
     * Create the 16 system files, adding the system information attribute
     * to each as well as marking them in use in the mft bitmap.
     */
    for (i = 0; i < nr_sysfiles; i++) {
        le32 file_attrs;
        m = (MFT_RECORD*)(gsBuf + i * gsVol->mft_record_size);
        // 0 ~ 16 与 23 ~ 27
        if (i < 16 || i > 23) {
            m->mft_record_number = cpu_to_le32(i);
            m->flags |= MFT_RECORD_IN_USE;
            ntfs_bit_set(gsMftBitmap, 0LL + i, 1);
        }
        file_attrs = FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM;
        // 第 5 个FMT是root 5KiB 处
        if (i == FILE_root) {
            file_attrs |= FILE_ATTR_ARCHIVE;
        }

        /* setting specific security_id flag and */
        /* file permissions for ntfs 3.x */
        if (i == 0 || i == 1 || i == 2 || i == 6 || i == 8 || i == 10) {
            add_attr_std_info(m, file_attrs, const_cpu_to_le32(0x0100));
        }
        else if (i == 9) {
            file_attrs |= FILE_ATTR_VIEW_INDEX_PRESENT;
            add_attr_std_info(m, file_attrs, const_cpu_to_le32(0x0101));
        }
        else if (i == 11) {
            add_attr_std_info(m, file_attrs, const_cpu_to_le32(0x0101));
        }
        else if (i == 24 || i == 25 || i == 26) {
            file_attrs |= FILE_ATTR_ARCHIVE;
            file_attrs |= FILE_ATTR_VIEW_INDEX_PRESENT;
            add_attr_std_info(m, file_attrs, const_cpu_to_le32(0x0101));
        }
        else {
            add_attr_std_info(m, file_attrs, const_cpu_to_le32(0x00));
        }
    }

    /* The root directory mft reference. */
    root_ref = MK_LE_MREF(FILE_root, FILE_root);
    extend_ref = MK_LE_MREF(11, 11);
    C_LOG_VERB("Creating root directory (mft record 5)");
    m = (MFT_RECORD*)(gsBuf + 5 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_DIRECTORY;
    m->link_count = cpu_to_le16(le16_to_cpu(m->link_count) + 1);
    err = add_attr_file_name(m, root_ref, 0LL, 0LL, FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM | FILE_ATTR_I30_INDEX_PRESENT, 0, 0, ".", FILE_NAME_WIN32_AND_DOS);
    if (!err) {
        init_root_sd(&sd, &i);
        err = add_attr_sd(m, sd, i);
    }

    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$I30", 4, CASE_SENSITIVE, AT_FILE_NAME, COLLATION_FILE_NAME, gsVol->indx_record_size);
    }

    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = upgrade_to_large_index(m, "$I30", 4, CASE_SENSITIVE, &gsIndexBlock);
    }

    if (!err) {
        ntfs_attr_search_ctx *ctx;
        ATTR_RECORD *a;
        ctx = ntfs_attr_get_search_ctx(NULL, m);
        if (!ctx) {
            C_LOG_WARNING("Failed to allocate attribute search context");
            return FALSE;
        }
        /* There is exactly one file name so this is ok. */
        if (mkntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
            ntfs_attr_put_search_ctx(ctx);
            C_LOG_WARNING("BUG: $FILE_NAME attribute not found.");
            return FALSE;
        }
        a = ctx->attr;
        err = insert_file_link_in_dir_index(gsIndexBlock, root_ref, (FILE_NAME_ATTR*)((char*)a + le16_to_cpu(a->value_offset)), le32_to_cpu(a->value_length));
        ntfs_attr_put_search_ctx(ctx);
    }
    if (err) {
        C_LOG_WARNING("Couldn't create root directory: %s", strerror(-err));
        return FALSE;
    }

    /* Add all other attributes, on a per-file basis for clarity. */
    C_LOG_VERB("Creating $MFT (mft record 0)");
    m = (MFT_RECORD*)gsBuf;
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), gsRlMft, gsBuf, gsMftSize);
    if (!err)
        err = create_hardlink(gsIndexBlock,
            root_ref,
            m,
            MK_LE_MREF(FILE_MFT, 1),
            ((gsMftSize - 1) | (gsVol->cluster_size - 1)) + 1,
            gsMftSize, FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM,
            0,
            0,
            "$MFT",
            FILE_NAME_WIN32_AND_DOS);
    /* mft_bitmap is not modified in mkntfs; no need to sync it later. */
    if (!err) {
        err = add_attr_bitmap_positioned(m, NULL, 0, CASE_SENSITIVE, gsRlMftBmp, gsMftBitmap, gsMftBitmapByteSize);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $MFT: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $MFTMirr (mft record 1)");
    m = (MFT_RECORD*)(gsBuf + 1 * gsVol->mft_record_size);
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), gsRlMftmirr, gsBuf, gsRlMftmirr[0].length * gsVol->cluster_size);
    if (!err) {
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_MFTMirr, FILE_MFTMirr),
                gsRlMftmirr[0].length * gsVol->cluster_size,
                gsRlMftmirr[0].length * gsVol->cluster_size,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$MFTMirr", FILE_NAME_WIN32_AND_DOS);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $MFTMirr: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $LogFile (mft record 2)");
    m = (MFT_RECORD*)(gsBuf + 2 * gsVol->mft_record_size);
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), gsRlLogfile, (const u8*)NULL, gsLogfileSize);
    if (!err) {
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_LogFile, FILE_LogFile),
                gsLogfileSize, gsLogfileSize,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$LogFile", FILE_NAME_WIN32_AND_DOS);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $LogFile: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $AttrDef (mft record 4)");
    m = (MFT_RECORD*)(gsBuf + 4 * gsVol->mft_record_size);
    err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), (u8*)gsVol->attrdef, gsVol->attrdef_len);
    if (!err) {
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_AttrDef, FILE_AttrDef),
                (gsVol->attrdef_len + gsVol->cluster_size - 1) &
                ~(gsVol->cluster_size - 1), gsVol->attrdef_len,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$AttrDef", FILE_NAME_WIN32_AND_DOS);
    }
    if (!err) {
        init_system_file_sd(FILE_AttrDef, &sd, &i);
        err = add_attr_sd(m, sd, i);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $AttrDef: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $Bitmap (mft record 6)");
    m = (MFT_RECORD*)(gsBuf + 6 * gsVol->mft_record_size);
    /* the data attribute of $Bitmap must be non-resident or otherwise */
    /* windows 2003 will regard the volume as corrupt (ERSO) */
    if (!err) {
        err = insert_non_resident_attr_in_mft_record(m, AT_DATA,  NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), (const u8*)NULL, gsLcnBitmapByteSize, WRITE_BITMAP);
    }

    if (!err) {
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_Bitmap, FILE_Bitmap),
                (gsLcnBitmapByteSize + gsVol->cluster_size -
                1) & ~(gsVol->cluster_size - 1),
                gsLcnBitmapByteSize,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$Bitmap", FILE_NAME_WIN32_AND_DOS);
    }

    if (err < 0) {
        C_LOG_WARNING("Couldn't create $Bitmap: %s", strerror(-err));
        return FALSE;
    }

    C_LOG_VERB("Creating $Boot (mft record 7)");
    m = (MFT_RECORD*)(gsBuf + 7 * gsVol->mft_record_size);
    bs = ntfs_calloc(8192);
    if (!bs) {
        return FALSE;
    }
    memcpy(bs, boot_array, sizeof(boot_array));

    /*
     * Create the boot sector in bs. Note, that bs is already zeroed
     * in the boot sector section and that it has the NTFS OEM id/magic
     * already inserted, so no need to worry about these things.
     */
    bs->bpb.bytes_per_sector = cpu_to_le16(opts.sectorSize);
    sectors_per_cluster = gsVol->cluster_size / opts.sectorSize;
    if (sectors_per_cluster > 128) {
        bs->bpb.sectors_per_cluster = 257 - ffs(sectors_per_cluster);
    }
    else {
        bs->bpb.sectors_per_cluster = sectors_per_cluster;
    }
    bs->bpb.media_type = 0xf8; /* hard disk */
    bs->bpb.sectors_per_track = cpu_to_le16(opts.sectorsPerTrack);
    C_LOG_VERB("sectors per track = %ld (0x%lx)", opts.sectorsPerTrack, opts.sectorsPerTrack);
    bs->bpb.heads = cpu_to_le16(opts.heads);
    C_LOG_VERB("heads = %ld (0x%lx)", opts.heads, opts.heads);
    bs->bpb.hidden_sectors = cpu_to_le32(opts.partStartSect);
    C_LOG_VERB("hidden sectors = %llu (0x%llx)", opts.partStartSect, opts.partStartSect);
    bs->physical_drive = 0x80;          /* boot from hard disk */
    bs->extended_boot_signature = 0x80; /* everybody sets this, so we do */
    bs->number_of_sectors = cpu_to_sle64(opts.numSectors);
    bs->mft_lcn = cpu_to_sle64(gsMftLcn);
    bs->mftmirr_lcn = cpu_to_sle64(gsMftmirrLcn);
    if (gsVol->mft_record_size >= gsVol->cluster_size) {
        bs->clusters_per_mft_record = gsVol->mft_record_size / gsVol->cluster_size;
    }
    else {
        bs->clusters_per_mft_record = -(ffs(gsVol->mft_record_size) - 1);
        if ((u32)(1 << -bs->clusters_per_mft_record) != gsVol->mft_record_size) {
            free(bs);
            C_LOG_WARNING("BUG: calculated clusters_per_mft_record is wrong (= 0x%x)", bs->clusters_per_mft_record);
            return FALSE;
        }
    }
    C_LOG_VERB("clusters per mft record = %i (0x%x)", bs->clusters_per_mft_record, bs->clusters_per_mft_record);
    if (gsVol->indx_record_size >= gsVol->cluster_size) {
        bs->clusters_per_index_record = gsVol->indx_record_size / gsVol->cluster_size;
    }
    else {
        bs->clusters_per_index_record = -gsVol->indx_record_size_bits;
        if ((1 << -bs->clusters_per_index_record) != (s32)gsVol->indx_record_size) {
            free(bs);
            C_LOG_WARNING("BUG: calculated clusters_per_index_record is wrong (= 0x%x)", bs->clusters_per_index_record);
            return FALSE;
        }
    }
    C_LOG_VERB("clusters per index block = %i (0x%x)", bs->clusters_per_index_record, bs->clusters_per_index_record);
    /* Generate a 64-bit random number for the serial number. */
    bs->volume_serial_number = cpu_to_le64(((u64)random() << 32) | ((u64)random() & 0xffffffff));
    /*
     * Leave zero for now as NT4 leaves it zero, too. If want it later, see
     * ../libntfs/bootsect.c for how to calculate it.
     */
    bs->checksum = const_cpu_to_le32(0);
    /* Make sure the bootsector is ok. */
    if (!ntfs_boot_sector_is_ntfs(bs)) {
        free(bs);
        C_LOG_ERROR("FATAL: Generated boot sector is invalid!");
        return FALSE;
    }

    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), gsRlBoot, (u8*)bs, 8192);
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_Boot, FILE_Boot),
                (8192 + gsVol->cluster_size - 1) &
                ~(gsVol->cluster_size - 1), 8192,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$Boot", FILE_NAME_WIN32_AND_DOS);
    if (!err) {
        init_system_file_sd(FILE_Boot, &sd, &i);
        err = add_attr_sd(m, sd, i);
    }
    if (err < 0) {
        free(bs);
        C_LOG_WARNING("Couldn't create $Boot: %s", strerror(-err));
        return FALSE;
    }
    if (create_backup_boot_sector((u8*)bs)) {
        /*
         * Pre-2.6 kernels couldn't access the last sector if it was
         * odd and we failed to set the device block size to the sector
         * size, hence we schedule chkdsk to create it.
         */
        volume_flags |= VOLUME_IS_DIRTY;
    }
    free(bs);

    /*
     * We cheat a little here and if the user has requested all times to be
     * set to zero then we set the GUID to zero as well.  This options is
     * only used for development purposes so that should be fine.
     */
    memset(&vol_guid, 0, sizeof(vol_guid));
    if (!create_file_volume(m, root_ref, volume_flags, &vol_guid)) {
        return FALSE;
    }
    C_LOG_VERB("Creating $BadClus (mft record 8)");
    m = (MFT_RECORD*)(gsBuf + 8 * gsVol->mft_record_size);
    /* FIXME: This should be IGNORE_CASE */
    /* Create a sparse named stream of size equal to the volume size. */
    err = add_attr_data_positioned(m, "$Bad", 4, CASE_SENSITIVE, const_cpu_to_le16(0), gsRlBad, NULL, gsVol->nr_clusters * gsVol->cluster_size);
    if (!err) {
        err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), NULL, 0);
    }
    if (!err) {
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_BadClus, FILE_BadClus),
                0LL, 0LL, FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM,
                0, 0, "$BadClus", FILE_NAME_WIN32_AND_DOS);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $BadClus: %s", strerror(-err));
        return FALSE;
    }
    /* create $Secure (NTFS 3.0+) */
    C_LOG_VERB("Creating $Secure (mft record 9)");
    m = (MFT_RECORD*)(gsBuf + 9 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(9, 9), 0LL, 0LL,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
                FILE_ATTR_VIEW_INDEX_PRESENT, 0, 0,
                "$Secure", FILE_NAME_WIN32_AND_DOS);
    buf_sds = NULL;
    buf_sds_first_size = 0;
    if (!err) {
        int buf_sds_size;
        buf_sds_first_size = 0xfc;
        buf_sds_size = 0x40000 + buf_sds_first_size;
        buf_sds = ntfs_calloc(buf_sds_size);
        if (!buf_sds) {
            return FALSE;
        }
        init_secure_sds(buf_sds);
        memcpy(buf_sds + 0x40000, buf_sds, buf_sds_first_size);
        err = add_attr_data(m, "$SDS", 4, CASE_SENSITIVE, const_cpu_to_le16(0), (u8*)buf_sds, buf_sds_size);
    }
    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$SDH", 4, CASE_SENSITIVE,
            AT_UNUSED, COLLATION_NTOFS_SECURITY_HASH,
            gsVol->indx_record_size);
    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$SII", 4, CASE_SENSITIVE, AT_UNUSED, COLLATION_NTOFS_ULONG, gsVol->indx_record_size);
    }
    if (!err) {
        err = initialize_secure(buf_sds, buf_sds_first_size, m);
    }
    free(buf_sds);
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $Secure: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $UpCase (mft record 0xa)");
    m = (MFT_RECORD*)(gsBuf + 0xa * gsVol->mft_record_size);
    err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), (u8*)gsVol->upcase, gsVol->upcase_len << 1);
    /*
     * The $Info only exists since Windows 8, but it apparently
     * does not disturb chkdsk from earlier versions.
     */
    if (!err) {
        err = add_attr_data(m, "$Info", 5, CASE_SENSITIVE, const_cpu_to_le16(0), (u8*)gsUpcaseInfo, sizeof(struct UPCASEINFO));
    }
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_UpCase, FILE_UpCase),
                ((gsVol->upcase_len << 1) +
                gsVol->cluster_size - 1) &
                ~(gsVol->cluster_size - 1),
                gsVol->upcase_len << 1,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$UpCase", FILE_NAME_WIN32_AND_DOS);
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $UpCase: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $Extend (mft record 11)");
    /*
     * $Extend index must be resident.  Otherwise, w2k3 will regard the
     * volume as corrupt. (ERSO)
     */
    m = (MFT_RECORD*)(gsBuf + 11 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_DIRECTORY;
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(11, 11), 0LL, 0LL,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
                FILE_ATTR_I30_INDEX_PRESENT, 0, 0,
                "$Extend", FILE_NAME_WIN32_AND_DOS);
    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$I30", 4, CASE_SENSITIVE, AT_FILE_NAME, COLLATION_FILE_NAME, gsVol->indx_record_size);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $Extend: %s", strerror(-err));
        return FALSE;
    }
    /* NTFS reserved system files (mft records 0xc-0xf) */
    for (i = 0xc; i < 0x10; i++) {
        C_LOG_VERB("Creating system file (mft record 0x%x)", i);
        m = (MFT_RECORD*)(gsBuf + i * gsVol->mft_record_size);
        err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0), NULL, 0);
        if (!err) {
            init_system_file_sd(i, &sd, &j);
            err = add_attr_sd(m, sd, j);
        }
        if (err < 0) {
            C_LOG_WARNING("Couldn't create system file %i (0x%x): %s", i, i, strerror(-err));
            return FALSE;
        }
    }
    /* create systemfiles for ntfs volumes (3.1) */
    /* starting with file 24 (ignoring file 16-23) */
    extend_flags = FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM | FILE_ATTR_ARCHIVE | FILE_ATTR_VIEW_INDEX_PRESENT;
    C_LOG_VERB("Creating $Quota (mft record 24)");
    m = (MFT_RECORD*)(gsBuf + 24 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_4;
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err) {
        err = create_hardlink_res((MFT_RECORD*)(gsBuf + 11 * gsVol->mft_record_size), extend_ref, m, MK_LE_MREF(24, 1), 0LL, 0LL, extend_flags, 0, 0,
            "$Quota", FILE_NAME_WIN32_AND_DOS);
    }
    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$Q", 2, CASE_SENSITIVE, AT_UNUSED, COLLATION_NTOFS_ULONG, gsVol->indx_record_size);
    }
    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$O", 2, CASE_SENSITIVE, AT_UNUSED, COLLATION_NTOFS_SID, gsVol->indx_record_size);
    }
    if (!err) {
        err = initialize_quota(m);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $Quota: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $ObjId (mft record 25)");
    m = (MFT_RECORD*)(gsBuf + 25 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_4;
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err) {
        err = create_hardlink_res((MFT_RECORD*)(gsBuf + 11 * gsVol->mft_record_size), extend_ref, m, MK_LE_MREF(25, 1), 0LL, 0LL, extend_flags, 0, 0,
            "$ObjId", FILE_NAME_WIN32_AND_DOS);
    }

    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$O", 2, CASE_SENSITIVE, AT_UNUSED, COLLATION_NTOFS_ULONGS, gsVol->indx_record_size);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $ObjId: %s", strerror(-err));
        return FALSE;
    }
    C_LOG_VERB("Creating $Reparse (mft record 26)");
    m = (MFT_RECORD*)(gsBuf + 26 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_4;
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err) {
        err = create_hardlink_res((MFT_RECORD*)(gsBuf + 11 * gsVol->mft_record_size), extend_ref, m, MK_LE_MREF(26, 1), 0LL, 0LL, extend_flags, 0,
            0, "$Reparse", FILE_NAME_WIN32_AND_DOS);
    }
    /* FIXME: This should be IGNORE_CASE */
    if (!err) {
        err = add_attr_index_root(m, "$R", 2, CASE_SENSITIVE, AT_UNUSED, COLLATION_NTOFS_ULONGS, gsVol->indx_record_size);
    }
    if (err < 0) {
        C_LOG_WARNING("Couldn't create $Reparse: %s", strerror(-err));
        return FALSE;
    }
    return TRUE;
}

static BOOL verify_boot_sector(struct ntfs_device *dev, ntfs_volume *rawvol)
{
    u8 buf[512] = {0};
    NTFS_BOOT_SECTOR *ntfs_boot = (NTFS_BOOT_SECTOR *)&buf;

    gsCurrentMftRecord = 9;

    if (ntfs_pread(dev, 0, sizeof(buf), buf) != sizeof(buf)) {
        C_LOG_WARNING("Failed to read boot sector.");
        return FALSE;
    }

    if ((buf[0]!=0xeb) || ((buf[1]!=0x52) && (buf[1]!=0x5b)) || (buf[2]!=0x90)) {
        C_LOG_WARNING("Boot sector: Bad jump.");
        return FALSE;
    }

    if (ntfs_boot->oem_id != magicNTFS) {
        C_LOG_WARNING("Boot sector: Bad NTFS magic.");
        return FALSE;
    }

    gsBytesPerSector = le16_to_cpu(ntfs_boot->bpb.bytes_per_sector);
    if (!gsBytesPerSector) {
        C_LOG_WARNING("Boot sector: Bytes per sector is 0.");
    }

    if (gsBytesPerSector % 512) {
        C_LOG_WARNING("Boot sector: Bytes per sector is not a multiple of 512.");
    }
    gsSectorsPerCluster = ntfs_boot->bpb.sectors_per_cluster;

    // todo: if partition, query bios and match heads/tracks? */

    // Initialize some values into rawvol. We will need those later.
    rawvol->dev = dev;
    ntfs_boot_sector_parse(rawvol, (NTFS_BOOT_SECTOR *)buf);

    C_LOG_INFO("verfiy boot OK!");

    return TRUE;
}

static void check_volume(ntfs_volume *vol)
{
    s64 mft_num, nr_mft_records;

    C_LOG_VERB("Unsupported: check_volume()");

    // For each mft record, verify that it contains a valid file record.
    nr_mft_records = vol->mft_na->initialized_size >> vol->mft_record_size_bits;
    C_LOG_VERB("Checking %lld MFT records.", (long long)nr_mft_records);

    for (mft_num=0; mft_num < nr_mft_records; mft_num++) {
        verify_mft_record(vol, mft_num);
    }

    // todo: Check metadata files.

    // todo: Second pass on mft records. Now check the contents as well.
    // todo: When going through runlists, build a bitmap.

    // todo: cluster accounting.
    return;
}

static int reset_dirty(ntfs_volume *vol)
{
    le16 flags;

    if (!(vol->flags | VOLUME_IS_DIRTY)) {
        return 0;
    }

    C_LOG_VERB("Resetting dirty flag.");

    flags = vol->flags & ~VOLUME_IS_DIRTY;

    if (ntfs_volume_write_flags(vol, flags)) {
        C_LOG_WARNING("Error setting volume flags.");
        return -1;
    }
    return 0;
}

static int verify_mft_preliminary(ntfs_volume *rawvol)
{
    gsCurrentMftRecord = 0;
    s64 mft_offset, mftmirr_offset;
    int res;

    C_LOG_VERB("Entering verify_mft_preliminary().");
    // todo: get size_of_file_record from boot sector
    // Load the first segment of the $MFT/DATA runlist.
    mft_offset = rawvol->mft_lcn * rawvol->cluster_size;
    mftmirr_offset = rawvol->mftmirr_lcn * rawvol->cluster_size;
    gsMftRl = load_runlist(rawvol, mft_offset, AT_DATA, 1024);
    if (!gsMftRl) {
        C_LOG_WARNING("Loading $MFT runlist failed. Trying $MFTMirr.");
        gsMftRl = load_runlist(rawvol, mftmirr_offset, AT_DATA, 1024);
    }
    if (!gsMftRl) {
        C_LOG_WARNING("Loading $MFTMirr runlist failed too. Aborting.");
        return RETURN_FS_ERRORS_LEFT_UNCORRECTED | RETURN_OPERATIONAL_ERROR;
    }
    // TODO: else { recover $MFT } // Use $MFTMirr to recover $MFT.
    // todo: support loading the next runlist extents when ATTRIBUTE_LIST is used on $MFT.
    // If attribute list: Gradually load mft runlist. (parse runlist from first file record, check all referenced file records, continue with the next file record). If no attribute list, just load it.

    // Load the runlist of $MFT/Bitmap.
    // todo: what about ATTRIBUTE_LIST? Can we reuse code?
    gsMftBitmapRl = load_runlist(rawvol, mft_offset, AT_BITMAP, 1024);
    if (!gsMftBitmapRl) {
        C_LOG_WARNING("Loading $MFT/Bitmap runlist failed. Trying $MFTMirr.");
        gsMftBitmapRl = load_runlist(rawvol, mftmirr_offset, AT_BITMAP, 1024);
    }
    if (!gsMftBitmapRl) {
        C_LOG_WARNING("Loading $MFTMirr/Bitmap runlist failed too. Aborting.");
        return RETURN_FS_ERRORS_LEFT_UNCORRECTED;
        // todo: rebuild the bitmap by using the "in_use" file record flag or by filling it with 1's.
    }

    /* Load $MFT/Bitmap */
    if ((res = mft_bitmap_load(rawvol))) {
        return res;
    }

    return -1; /* FIXME: Just added to fix compiler warning without thinking about what should be here.  (Yura) */
}

static void verify_mft_record(ntfs_volume *vol, s64 mft_num)
{
    u8 *buffer;
    int is_used;

    gsCurrentMftRecord = mft_num;

    is_used = mft_bitmap_get_bit(mft_num);
    if (is_used<0) {
        C_LOG_WARNING("Error getting bit value for record %lld.",
            (long long)mft_num);
    } else if (!is_used) {
        C_LOG_VERB("Record %lld unused. Skipping.", (long long)mft_num);
        return;
    }

    buffer = ntfs_malloc(vol->mft_record_size);
    if (!buffer)
        goto verify_mft_record_error;

    C_LOG_VERB("MFT record %lld", (long long)mft_num);
    if (ntfs_attr_pread(vol->mft_na, mft_num*vol->mft_record_size, vol->mft_record_size, buffer) < 0) {
        C_LOG_WARNING("Couldn't read $MFT record %lld", (long long)mft_num);
        goto verify_mft_record_error;
    }

    check_file_record(buffer, vol->mft_record_size);
    // todo: if offset to first attribute >= 0x30, number of mft record should match.
    // todo: Match the "record is used" with the mft bitmap.
    // todo: if this is not base, check that the parent is a base, and is in use, and pointing to this record.

    // todo: if base record: for each extent record:
    //   todo: verify_file_record
    //   todo: hard link count should be the number of 0x30 attributes.
    //   todo: Order of attributes.
    //   todo: make sure compression_unit is the same.

    return;
    verify_mft_record_error:

        if (buffer)
            free(buffer);
    gsErrors++;
}

static void replay_log(ntfs_volume *vol __attribute__((unused)))
{
    // At this time, only check that the log is fully replayed.
    C_LOG_WARNING("Unsupported: replay_log()");
    // todo: if logfile is clean, return success.
}

static BOOL check_file_record(u8 *buffer, u16 buflen)
{
    u16 usa_count, usa_ofs, attrs_offset, usa;
    u32 bytes_in_use, bytes_allocated, i;
    MFT_RECORD *mft_rec = (MFT_RECORD *)buffer;
    ATTR_REC *attr_rec;

    // check record magic
    assert_u32_equal(le32_to_cpu(mft_rec->magic), le32_to_cpu(magic_FILE), "FILE record magic");
    // todo: records 16-23 must be filled in order.
    // todo: what to do with magic_BAAD?

    // check usa_count+offset to update seq <= attrs_offset <
    //    bytes_in_use <= bytes_allocated <= buflen.
    usa_ofs = le16_to_cpu(mft_rec->usa_ofs);
    usa_count = le16_to_cpu(mft_rec->usa_count);
    attrs_offset = le16_to_cpu(mft_rec->attrs_offset);
    bytes_in_use = le32_to_cpu(mft_rec->bytes_in_use);
    bytes_allocated = le32_to_cpu(mft_rec->bytes_allocated);
    if (assert_u32_lesseq(usa_ofs+usa_count, attrs_offset,
                "usa_ofs+usa_count <= attrs_offset") ||
            assert_u32_less(attrs_offset, bytes_in_use,
                "attrs_offset < bytes_in_use") ||
            assert_u32_lesseq(bytes_in_use, bytes_allocated,
                "bytes_in_use <= bytes_allocated") ||
            assert_u32_lesseq(bytes_allocated, buflen,
                "bytes_allocated <= max_record_size")) {
        return 1;
    }


    // We should know all the flags.
    if (le16_to_cpu(mft_rec->flags) > 0xf) {
        C_LOG_WARNING("Unknown MFT record flags (0x%x).", (unsigned int)le16_to_cpu(mft_rec->flags));
    }
    // todo: flag in_use must be on.

    // Remove update seq & check it.
    usa = *(u16*)(buffer+usa_ofs); // The value that should be at the end of every sector.
    if (assert_u32_equal(usa_count-1, buflen/NTFS_BLOCK_SIZE, "USA length"))
        return (1);
    for (i=1;i<usa_count;i++) {
        u16 *fixup = (u16*)(buffer+NTFS_BLOCK_SIZE*i-2); // the value at the end of the sector.
        u16 saved_val = *(u16*)(buffer+usa_ofs+2*i); // the actual data value that was saved in the us array.

        assert_u32_equal(*fixup, usa, "fixup");
        *fixup = saved_val; // remove it.
    }

    attr_rec = (ATTR_REC *)(buffer + attrs_offset);
    while ((u8*)attr_rec<=buffer+buflen-4) {

        // Check attribute record. (Only what is in the buffer)
        if (attr_rec->type==AT_END) {
            // Done.
            return 0;
        }
        if ((u8*)attr_rec>buffer+buflen-8) {
            // not AT_END yet no room for the length field.
            C_LOG_WARNING("Attribute 0x%x is not AT_END, yet no "
                    "room for the length field.",
                    (int)le32_to_cpu(attr_rec->type));
            return 1;
        }

        attr_rec = check_attr_record(attr_rec, mft_rec, buflen);
        if (!attr_rec)
            return 1;
    }
    // If we got here, there was an overflow.
    return 1;

    // todo: an attribute should be at the offset to first attribute, and the offset should be inside the buffer. It should have the value of "next attribute id".
    // todo: if base record, it should start with attribute 0x10.

    // Highlevel check of attributes.
    //  todo: Attributes are well-formed.
    //  todo: Room for next attribute in the end of the previous record.

    return FALSE;
}

static ATTR_REC *check_attr_record(ATTR_REC *attr_rec, MFT_RECORD *mft_rec, u16 buflen)
{
    u16 name_offset;
    u16 attrs_offset = le16_to_cpu(mft_rec->attrs_offset);
    u32 attr_type = le32_to_cpu(attr_rec->type);
    u32 length = le32_to_cpu(attr_rec->length);

    // Check that this attribute does not overflow the mft_record
    if ((u8*)attr_rec+length >= ((u8*)mft_rec)+buflen) {
        C_LOG_WARNING("Attribute (0x%x) is larger than FILE record (%lld).",
                (int)attr_type, (long long)gsCurrentMftRecord);
        return NULL;
    }

    // Attr type must be a multiple of 0x10 and 0x10<=x<=0x100.
    if ((attr_type & ~0x0F0) && (attr_type != 0x100)) {
        C_LOG_WARNING("Unknown attribute type 0x%x.",
            (int)attr_type);
        goto check_attr_record_next_attr;
    }

    if (length<24) {
        C_LOG_WARNING("Attribute %lld:0x%x Length too short (%u).",
            (long long)gsCurrentMftRecord, (int)attr_type,
            (int)length);
        goto check_attr_record_next_attr;
    }

    // If this is the first attribute:
    // todo: instance number must be smaller than next_instance.
    if ((u8*)attr_rec == ((u8*)mft_rec) + attrs_offset) {
        if (!mft_rec->base_mft_record)
            assert_u32_equal(attr_type, 0x10,
                "First attribute type");
        // The following not always holds.
        // attr 0x10 becomes instance 1 and attr 0x40 becomes 0.
        //assert_u32_equal(attr_rec->instance, 0,
        //    "First attribute instance number");
    } else {
        assert_u32_noteq(attr_type, 0x10,
            "Not-first attribute type");
        // The following not always holds.
        //assert_u32_noteq(attr_rec->instance, 0,
        //    "Not-first attribute instance number");
    }
    //if (current_mft_record==938 || current_mft_record==1683 || current_mft_record==3152 || current_mft_record==22410)
    //printf("Attribute %lld:0x%x instance: %u isbase:%d.",
    //        current_mft_record, (int)attr_type, (int)le16_to_cpu(attr_rec->instance), (int)mft_rec->base_mft_record);
    // todo: instance is unique.

    // Check flags.
    if (attr_rec->flags & ~(const_cpu_to_le16(0xc0ff))) {
        C_LOG_WARNING("Attribute %lld:0x%x Unknown flags (0x%x).",
            (long long)gsCurrentMftRecord, (int)attr_type,
            (int)le16_to_cpu(attr_rec->flags));
    }

    if (attr_rec->non_resident>1) {
        C_LOG_WARNING("Attribute %lld:0x%x Unknown non-resident "
            "flag (0x%x).", (long long)gsCurrentMftRecord,
            (int)attr_type, (int)attr_rec->non_resident);
        goto check_attr_record_next_attr;
    }

    name_offset = le16_to_cpu(attr_rec->name_offset);
    /*
     * todo: name must be legal unicode.
     * Not really, information below in urls is about filenames, but I
     * believe it also applies to attribute names.  (Yura)
     *  http://blogs.msdn.com/michkap/archive/2006/09/24/769540.aspx
     *  http://blogs.msdn.com/michkap/archive/2006/09/10/748699.aspx
     */

    if (attr_rec->non_resident) {
        // Non-Resident

        // Make sure all the fields exist.
        if (length<64) {
            C_LOG_WARNING("Non-resident attribute %lld:0x%x too short (%u).",
                (long long)gsCurrentMftRecord, (int)attr_type,
                (int)length);
            goto check_attr_record_next_attr;
        }
        if (attr_rec->compression_unit && (length<72)) {
            C_LOG_WARNING("Compressed attribute %lld:0x%x too short (%u).",
                (long long)gsCurrentMftRecord, (int)attr_type,
                (int)length);
            goto check_attr_record_next_attr;
        }

        // todo: name comes before mapping pairs, and after the header.
        // todo: length==mapping_pairs_offset+length of compressed mapping pairs.
        // todo: mapping_pairs_offset is 8-byte aligned.

        // todo: lowest vcn <= highest_vcn
        // todo: if base record -> lowest vcn==0
        // todo: lowest_vcn!=0 -> attribute list is used.
        // todo: lowest_vcn & highest_vcn are in the drive (0<=x<total clusters)
        // todo: mapping pairs agree with highest_vcn.
        // todo: compression unit == 0 or 4.
        // todo: reserved1 == 0.
        // todo: if not compressed nor sparse, initialized_size <= allocated_size and data_size <= allocated_size.
        // todo: if compressed or sparse, allocated_size <= initialized_size and allocated_size <= data_size
        // todo: if mft_no!=0 and not compressed/sparse, data_size==initialized_size.
        // todo: if mft_no!=0 and compressed/sparse, allocated_size==initialized_size.
        // todo: what about compressed_size if compressed?
        // todo: attribute must not be 0x10, 0x30, 0x40, 0x60, 0x70, 0x90, 0xd0 (not sure about 0xb0, 0xe0, 0xf0)
    } else {
        u16 value_offset = le16_to_cpu(attr_rec->value_offset);
        u32 value_length = le32_to_cpu(attr_rec->value_length);
        // Resident
        if (attr_rec->name_length) {
            if (name_offset < 24)
                C_LOG_WARNING("Resident attribute with "
                    "name intersecting header.");
            if (value_offset < name_offset +
                    attr_rec->name_length)
                C_LOG_WARNING("Named resident attribute "
                    "with value before name.");
        }
        // if resident, length==value_length+value_offset
        //assert_u32_equal(le32_to_cpu(attr_rec->value_length)+
        //    value_offset, length,
        //    "length==value_length+value_offset");
        // if resident, length==value_length+value_offset
        if (value_length+value_offset > length) {
            C_LOG_WARNING("value_length(%d)+value_offset(%d)>length(%d) for attribute 0x%x.", (int)value_length, (int)value_offset, (int)length, (int)attr_type);
            return NULL;
        }

        // Check resident_flags.
        if (attr_rec->resident_flags>0x01) {
            C_LOG_WARNING("Unknown resident flags (0x%x) for attribute 0x%x.", (int)attr_rec->resident_flags, (int)attr_type);
        } else if (attr_rec->resident_flags && (attr_type!=0x30)) {
            C_LOG_WARNING("Resident flags mark attribute 0x%x as indexed.", (int)attr_type);
        }

        // reservedR is 0.
        assert_u32_equal(attr_rec->reservedR, 0, "Resident Reserved");

        // todo: attribute must not be 0xa0 (not sure about 0xb0, 0xe0, 0xf0)
        // todo: check content well-formness per attr_type.
    }
    return 0;
check_attr_record_next_attr:
    return (ATTR_REC *)(((u8 *)attr_rec) + length);
}

static int mft_bitmap_get_bit(s64 mft_no)
{
    if (mft_no >= gsMftBitmapRecords)
        return -1;
    return ntfs_bit_get(gsMftBitmapBuf, mft_no);
}

static int mft_bitmap_load(ntfs_volume *rawvol)
{
    VCN vcn;
    u32 mft_bitmap_length;

    vcn = get_last_vcn(gsMftBitmapRl);
    if (vcn<=LCN_EINVAL) {
        gsMftBitmapBuf = NULL;
        C_LOG_WARNING("get_last_vcn error!");
        /* This case should not happen, not even with on-disk errors */
        goto error;
    }

    mft_bitmap_length = vcn * rawvol->cluster_size;
    gsMftBitmapRecords = 8 * mft_bitmap_length * rawvol->cluster_size / rawvol->mft_record_size;

    gsMftBitmapBuf = (u8*)ntfs_malloc(mft_bitmap_length);
    if (!gsMftBitmapBuf) {
        C_LOG_WARNING("malloc error!");
        goto error;
    }

    if (ntfs_rl_pread(rawvol, gsMftBitmapRl, 0, mft_bitmap_length, gsMftBitmapBuf)!=mft_bitmap_length) {
        C_LOG_WARNING("read error!");
        goto error;
    }

    return 0;

error:
    gsMftBitmapRecords = 0;
    C_LOG_WARNING("Could not load $MFT/Bitmap.");

    return RETURN_OPERATIONAL_ERROR;
}

static VCN get_last_vcn(runlist *rl)
{
    VCN res;

    if (!rl)
        return LCN_EINVAL;

    res = LCN_EINVAL;
    while (rl->length) {
        C_LOG_VERB("vcn: %lld, length: %lld.", (long long)rl->vcn, (long long)rl->length);
        if (rl->vcn<0)
            res = rl->vcn;
        else
            res = rl->vcn + rl->length;
        rl++;
    }

    return res;
}

static runlist *load_runlist(ntfs_volume *rawvol, s64 offset_to_file_record, ATTR_TYPES attr_type, u32 size_of_file_record)
{
    u8 *buf;
    u16 attrs_offset;
    u32 length;
    ATTR_RECORD *attr_rec;

    if (size_of_file_record<22) // offset to attrs_offset
        return NULL;

    buf = (u8*)ntfs_malloc(size_of_file_record);
    if (!buf)
        return NULL;

    if (ntfs_pread(rawvol->dev, offset_to_file_record, size_of_file_record, buf) != size_of_file_record) {
        C_LOG_WARNING("Failed to read file record at offset %lld (0x%llx).", (long long)offset_to_file_record, (long long)offset_to_file_record);
        return NULL;
    }

    attrs_offset = le16_to_cpu(((MFT_RECORD*)buf)->attrs_offset);
    // first attribute must be after the header.
    if (attrs_offset < 42) {
        C_LOG_WARNING("First attribute must be after the header (%u).", (int)attrs_offset);
    }
    attr_rec = (ATTR_RECORD *)(buf + attrs_offset);
    //printf("uv1.");

    while ((u8*)attr_rec<=buf+size_of_file_record-4) {

        //printf("Attr type: 0x%x.", attr_rec->type);
        // Check attribute record. (Only what is in the buffer)
        if (attr_rec->type==AT_END) {
            C_LOG_WARNING("Attribute 0x%x not found in file record at offset %lld (0x%llx).",
                (int)le32_to_cpu(attr_rec->type), (long long)offset_to_file_record, (long long)offset_to_file_record);
            return NULL;
        }
        if ((u8*)attr_rec>buf+size_of_file_record-8) {
            // not AT_END yet no room for the length field.
            C_LOG_WARNING("Attribute 0x%x is not AT_END, yet no room for the length field.", (int)le32_to_cpu(attr_rec->type));
            return NULL;
        }

        length = le32_to_cpu(attr_rec->length);

        // Check that this attribute does not overflow the mft_record
        if ((u8*)attr_rec+length >= buf+size_of_file_record) {
            C_LOG_WARNING("Attribute (0x%x) is larger than FILE record at offset %lld (0x%llx).",
                    (int)le32_to_cpu(attr_rec->type), (long long)offset_to_file_record, (long long)offset_to_file_record);
            return NULL;
        }
        // todo: what ATTRIBUTE_LIST (0x20)?

        if (attr_rec->type==attr_type) {
            // Eurika!

            // ntfs_mapping_pairs_decompress only use two values from vol. Just fake it.
            // todo: it will also use vol->major_ver if defined(DEBUG). But only for printing purposes.

            // Assume ntfs_boot_sector_parse() was called.
            return ntfs_mapping_pairs_decompress(rawvol, attr_rec, NULL);
        }

        attr_rec = (ATTR_RECORD*)((u8*)attr_rec+length);
    }
    // If we got here, there was an overflow.
    C_LOG_WARNING("file record corrupted at offset %lld (0x%llx).", (long long)offset_to_file_record, (long long)offset_to_file_record);

    return NULL;
}

static int assert_u32_equal(u32 val, u32 ok, const char *name)
{
    if (val!=ok) {
        C_LOG_WARNING("Assertion failed for '%lld:%s'. should be 0x%x, "
            "was 0x%x.", (long long)gsCurrentMftRecord, name,
            (int)ok, (int)val);
        //errors++;
        return 1;
    }
    return 0;
}

static int assert_u32_noteq(u32 val, u32 wrong, const char *name)
{
    if (val==wrong) {
        C_LOG_WARNING("Assertion failed for '%lld:%s'. should not be "
            "0x%x.", (long long)gsCurrentMftRecord, name,
            (int)wrong);
        return 1;
    }
    return 0;
}

static int assert_u32_lesseq(u32 val1, u32 val2, const char *name)
{
    if (val1 > val2) {
        C_LOG_WARNING("Assertion failed for '%s'. 0x%x > 0x%x", name, (int)val1, (int)val2);
        //errors++;
        return 1;
    }
    return 0;
}

static int assert_u32_less(u32 val1, u32 val2, const char *name)
{
    if (val1 >= val2) {
        C_LOG_WARNING("Assertion failed for '%s'. 0x%x >= 0x%x",
            name, (int)val1, (int)val2);
        //errors++;
        return 1;
    }
    return 0;
}

static int64_t align_1024 (int64_t value)
{
    if (0 == (value % 1024)) {
        return value;
    }

    return (1024 * (value / 1024 - 1));
}

static int64_t align_4096 (int64_t value)
{
    if (0 == (value % 4096)) {
        return value;
    }

    return (4096 * (value / 4096 - 1));
}

static int expand_to_beginning(const char* devPath)
{
    expand_t expand;
    struct progress_bar progress;
    int ret;
    ntfs_volume *vol;
    struct ntfs_device *dev;
    int sector_size;
    s64 new_sectors;

    ret = -1;
    dev = ntfs_device_alloc(devPath, 0, &ntfs_device_default_io_ops, NULL);
    if (dev) {
        if (!(*dev->d_ops->open)(dev, O_RDWR)) {
            C_LOG_VERB("device open");
            sector_size = ntfs_device_sector_size_get(dev);
            if (sector_size <= 0) {
                sector_size = 512;
                new_sectors = ntfs_device_size_get(dev, sector_size);
                if (!new_sectors) {
                    sector_size = 4096;
                    new_sectors = ntfs_device_size_get(dev, sector_size);
                }
            }
            else {
                new_sectors = ntfs_device_size_get(dev, sector_size);
            }
            if (new_sectors) {
                new_sectors--; /* last sector not counted */
                expand.new_sectors = new_sectors;
                expand.progress = &progress;
                expand.delayed_runlists = (struct DELAYED*)NULL;
                vol = get_volume_data(&expand, dev, sector_size);
                if (vol) {
                    expand.vol = vol;
                    ret = really_expand(&expand, devPath);
                }
            }
            (*dev->d_ops->close)(dev);
        } else {
            C_LOG_ERROR("Couldn't open volume '%s'!", devPath);
        }
        ntfs_device_free(dev);
    }

    return (ret);
}

static int really_expand(expand_t *expand, const char* devPath)
{
	ntfs_volume *vol;
	struct ntfs_device *dev;
	int res;

	res = -1;

	expand->bitmap = (u8*)ntfs_calloc(expand->bitmap_allocated);
	if (expand->bitmap && get_mft_bitmap(expand)) {
		if (!rebase_all_inodes(expand) && !write_bitmap(expand) && !copy_mftmirr(expand) && !copy_boot(expand)) {
			free(expand->vol);
			expand->vol = (ntfs_volume*)NULL;
			free(expand->mft_bitmap);
			expand->mft_bitmap = (u8*)NULL;
			/* the volume must be dirty, do not check */
			vol = mount_volume(devPath);
			if (vol) {
				dev = vol->dev;
				C_LOG_VERB("Remounting the updated volume");
				expand->vol = vol;
				C_LOG_VERB("Delayed runlist updatings");
				delayed_expand(vol, expand->delayed_runlists, expand->progress);
				expand->delayed_runlists = (struct DELAYED*)NULL;
				expand_index_sizes(expand);
	/* rewriting the backup bootsector, no return ticket now ! */
				res = write_bootsector(expand);
				if (dev->d_ops->sync(dev) == -1) {
					printf("Could not sync");
					res = -1;
				}
				ntfs_umount(vol,0);
				if (!res) {
					printf("Resizing completed successfully");
				}
			}

			free(expand->bootsector);
			free(expand->mrec);
		}
		free(expand->bitmap);
	}
    else {
		C_LOG_ERROR("Failed to allocate memory");
	}
	return (res);
}

static ntfs_volume *get_volume_data(expand_t *expand, struct ntfs_device *dev, s32 sector_size)
{
    s64 br;
    ntfs_volume *vol;
    le16 sector_size_le;
    NTFS_BOOT_SECTOR *bs;
    BOOL ok;

    ok = FALSE;
    vol = (ntfs_volume*)ntfs_malloc(sizeof(ntfs_volume));
    expand->bootsector = (char*)ntfs_malloc(sector_size);
    if (vol && expand->bootsector) {
        expand->vol = vol;
        vol->dev = dev;
        br = ntfs_pread(dev, expand->new_sectors*sector_size, sector_size, expand->bootsector);
        if (br != sector_size) {
            if (br != -1) {
                errno = EINVAL;
            }
            if (!br) {
                C_LOG_ERROR("Failed to read the backup bootsector (size=0)");
            }
            else {
                C_LOG_ERROR("Error reading the backup bootsector");
            }
        }
        else {
            bs = (NTFS_BOOT_SECTOR*)expand->bootsector;
            /* alignment problem on Sparc, even doing memcpy() */
            sector_size_le = cpu_to_le16(sector_size);
            // C_LOG_VERB("memcmp: %d", memcmp(&sector_size_le, &bs->bpb.bytes_per_sector,2));
            // C_LOG_VERB("boot sector is ntfs: %d", ntfs_boot_sector_is_ntfs(bs));
            // C_LOG_VERB("ntfs boot sector parse: %d", ntfs_boot_sector_parse(vol, bs));
            if (!memcmp(&sector_size_le, &bs->bpb.bytes_per_sector,2) && ntfs_boot_sector_is_ntfs(bs) && !ntfs_boot_sector_parse(vol, bs)) {
                expand->original_sectors = sle64_to_cpu(bs->number_of_sectors);
                expand->mrec = (MFT_RECORD*) ntfs_malloc(vol->mft_record_size);
                if (expand->mrec && can_expand(expand,vol)) {
                    C_LOG_VERB("Resizing is possible");
                    ok = TRUE;
                }
            }
            else {
                C_LOG_ERROR("Could not get the old volume parameters from the backup bootsector");
            }
        }
        if (!ok) {
            free(vol);
            free(expand->bootsector);
        }
    }

    return (ok ? vol : (ntfs_volume*)NULL);
}

static int rebase_all_inodes(expand_t *expand)
{
    ntfs_volume *vol;
    MFT_RECORD *mrec;
    s64 inum;
    s64 jnum;
    s64 inodecnt;
    s64 pos;
    s64 got;
    int res;
    runlist_element *mft_rl;
    runlist_element *prl;

    res = 0;
    mft_rl = (runlist_element*)NULL;
    vol = expand->vol;
    mrec = expand->mrec;
    inum = 0;
    pos = (vol->mft_lcn + expand->cluster_increment)
                << vol->cluster_size_bits;
    got = ntfs_mst_pread(vol->dev, pos, 1,
            vol->mft_record_size, mrec);
    if ((got == 1) && (mrec->flags & MFT_RECORD_IN_USE)) {
        pos = expand->mft_lcn << vol->cluster_size_bits;
        mft_rl = rebase_runlists_meta(expand, FILE_MFT);
        if (!mft_rl || (ntfs_mst_pwrite(vol->dev, pos, 1, vol->mft_record_size, mrec) != 1)) {
            res = -1;
        }
        else {
            for (prl=mft_rl; prl->length; prl++) { }
            inodecnt = (prl->vcn << vol->cluster_size_bits) >> vol->mft_record_size_bits;
            progress_init(expand->progress, 0, inodecnt, 0);
            prl = mft_rl;
            jnum = 0;
            do {
                inum++;
                while (prl->length
                    && ((inum << vol->mft_record_size_bits)
                    >= ((prl->vcn + prl->length)
                        << vol->cluster_size_bits))) {
                    prl++;
                    jnum = inum;
                        }
                progress_update(expand->progress, inum);
                if (prl->length) {
                    res = rebase_inode(expand,
                        prl,inum,jnum);
                }
            } while (!res && prl->length);
            free(mft_rl);
        }
    }
    else {
        C_LOG_ERROR("Could not read the old $MFT");
        res = -1;
    }
    return (res);
}

static int rebase_inode(expand_t *expand, const runlist_element *prl, s64 inum, s64 jnum)
{
    MFT_RECORD *mrec;
    runlist_element *rl;
    ntfs_volume *vol;
    s64 pos;
    int res;

    res = 0;
    vol = expand->vol;
    mrec = expand->mrec;
    if (expand->mft_bitmap[inum >> 3] & (1 << (inum & 7))) {
        pos = (prl->lcn << vol->cluster_size_bits)
            + ((inum - jnum) << vol->mft_record_size_bits);
        if ((ntfs_mst_pread(vol->dev, pos, 1,
                    vol->mft_record_size, mrec) == 1)
            && (mrec->flags & MFT_RECORD_IN_USE)) {
            switch (inum) {
            case FILE_Bitmap :
            case FILE_Boot :
            case FILE_BadClus :
                rl = rebase_runlists_meta(expand, inum);
                if (rl)
                    free(rl);
                else
                    res = -1;
                break;
            default :
                res = rebase_runlists(expand, inum);
                break;
            }
        }
        else {
            C_LOG_ERROR("Could not read the $MFT entry %lld", (long long)inum);
            res = -1;
        }
    } else {
        /*
         * Replace unused records (possibly uninitialized)
         * by minimal valid records, not marked in use
         */
        res = minimal_record(expand,mrec);
    }
    if (!res) {
        pos = (expand->mft_lcn << vol->cluster_size_bits) + (inum << vol->mft_record_size_bits);
        if ((ntfs_mst_pwrite(vol->dev, pos, 1, vol->mft_record_size, mrec) != 1)) {
            C_LOG_ERROR("Could not write the $MFT entry %lld", (long long)inum);
            res = -1;
        }
    }
    return (res);
}

static runlist_element *rebase_runlists_meta(expand_t *expand, s64 inum)
{
	MFT_RECORD *mrec;
	ATTR_RECORD *a;
	ntfs_volume *vol;
	runlist_element *rl;
	runlist_element *old_rl;
	runlist_element *prl;
	runlist_element new_rl[2];
	s64 data_size;
	s64 allocated_size;
	s64 lcn;
	u64 lth;
	u32 offset;
	BOOL keeprl;
	int res;

	res = 0;
	old_rl = (runlist_element*)NULL;
	vol = expand->vol;
	mrec = expand->mrec;
	switch (inum) {
	case FILE_Boot :
		lcn = 0;
		lth = expand->boot_size >> vol->cluster_size_bits;
		data_size = expand->boot_size;
		break;
	case FILE_Bitmap :
		lcn = expand->boot_size >> vol->cluster_size_bits;
		lth = expand->bitmap_allocated >> vol->cluster_size_bits;
		data_size = expand->bitmap_size;
		break;
	case FILE_MFT :
		lcn = (expand->boot_size + expand->bitmap_allocated)
				>> vol->cluster_size_bits;
		lth = expand->mft_size >> vol->cluster_size_bits;
		data_size = expand->mft_size;
		break;
	case FILE_BadClus :
		lcn = 0; /* not used */
		lth = vol->nr_clusters + expand->cluster_increment;
		data_size = lth << vol->cluster_size_bits;
		break;
	default :
		lcn = lth = data_size = 0;
		res = -1;
	}
	allocated_size = lth << vol->cluster_size_bits;
	offset = le16_to_cpu(mrec->attrs_offset);
	a = (ATTR_RECORD*)((char*)mrec + offset);
	while (!res && (a->type != AT_END)
			&& (offset < le32_to_cpu(mrec->bytes_in_use))) {
		if (a->non_resident) {
			keeprl = FALSE;
			rl = ntfs_mapping_pairs_decompress(vol, a,
						(runlist_element*)NULL);
			if (rl) {
				/* rebase the old runlist */
				for (prl=rl; prl->length; prl++)
					if (prl->lcn >= 0) {
						prl->lcn += expand->cluster_increment;
						if ((a->type != AT_DATA)
						    && set_bitmap(expand,prl))
							res = -1;
					}
				/* relocated unnamed data (not $BadClus) */
				if ((a->type == AT_DATA)
				    && !a->name_length
				    && (inum != FILE_BadClus)) {
					old_rl = rl;
					rl = new_rl;
					keeprl = TRUE;
					rl[0].vcn = 0;
					rl[0].lcn = lcn;
					rl[0].length = lth;
					rl[1].vcn = lth;
					rl[1].lcn = LCN_ENOENT;
					rl[1].length = 0;
					if (set_bitmap(expand,rl))
						res = -1;
					a->data_size = cpu_to_sle64(data_size);
					a->initialized_size = a->data_size;
					a->allocated_size
						= cpu_to_sle64(allocated_size);
					a->highest_vcn = cpu_to_sle64(lth - 1);
				}
				/* expand the named data for $BadClus */
				if ((a->type == AT_DATA)
				    && a->name_length
				    && (inum == FILE_BadClus)) {
					old_rl = rl;
					keeprl = TRUE;
					prl = rl;
					if (prl->length) {
						while (prl[1].length)
							prl++;
						prl->length = lth - prl->vcn;
						prl[1].vcn = lth;
					} else
						prl->vcn = lth;
					a->data_size = cpu_to_sle64(data_size);
					/* do not change the initialized size */
					a->allocated_size
						= cpu_to_sle64(allocated_size);
					a->highest_vcn = cpu_to_sle64(lth - 1);
				}
				if (!res && update_runlist(expand,inum,a,rl))
					res = -1;
				if (!keeprl)
					free(rl);
			} else {
				C_LOG_WARNING("Could not get the data runlist of inode %lld", (long long)inum);
				res = -1;
			}
		}
		offset += le32_to_cpu(a->length);
		a = (ATTR_RECORD*)((char*)mrec + offset);
	}
	if (res && old_rl) {
		free(old_rl);
		old_rl = (runlist_element*)NULL;
	}
	return (old_rl);
}

static int rebase_runlists(expand_t *expand, s64 inum)
{
    MFT_RECORD *mrec;
    ATTR_RECORD *a;
    runlist_element *rl;
    runlist_element *prl;
    u32 offset;
    int res;

    res = 0;
    mrec = expand->mrec;
    offset = le16_to_cpu(mrec->attrs_offset);
    a = (ATTR_RECORD*)((char*)mrec + offset);
    while (!res && (a->type != AT_END)
            && (offset < le32_to_cpu(mrec->bytes_in_use))) {
        if (a->non_resident) {
            rl = ntfs_mapping_pairs_decompress(expand->vol, a,
                        (runlist_element*)NULL);
            if (rl) {
                for (prl=rl; prl->length; prl++)
                    if (prl->lcn >= 0) {
                        prl->lcn += expand->cluster_increment;
                        if (set_bitmap(expand,prl))
                            res = -1;
                    }
                if (update_runlist(expand,inum,a,rl)) {
                    C_LOG_VERB("Runlist updating has to be delayed");
                }
                else {
                    free(rl);
                }
            } else {
                C_LOG_WARNING("Could not get a runlist of inode %lld", (long long)inum);
                res = -1;
            }
        }
        offset += le32_to_cpu(a->length);
        a = (ATTR_RECORD*)((char*)mrec + offset);
            }
    return (res);
}

static int minimal_record(expand_t *expand, MFT_RECORD *mrec)
{
    int usa_count;
    u32 bytes_in_use;

    memset(mrec,0,expand->vol->mft_record_size);
    mrec->magic = magic_FILE;
    mrec->usa_ofs = const_cpu_to_le16(sizeof(MFT_RECORD));
    usa_count = expand->vol->mft_record_size / NTFS_BLOCK_SIZE + 1;
    mrec->usa_count = cpu_to_le16(usa_count);
    bytes_in_use = (sizeof(MFT_RECORD) + 2*usa_count + 7) & -8;
    memset(((char*)mrec) + bytes_in_use, 255, 4);  /* AT_END */
    bytes_in_use += 8;
    mrec->bytes_in_use = cpu_to_le32(bytes_in_use);
    mrec->bytes_allocated = cpu_to_le32(expand->vol->mft_record_size);
    return (0);
}

static int update_runlist(expand_t *expand, s64 inum, ATTR_RECORD *a, runlist_element *rl)
{
	ntfs_resize_t resize;
	ntfs_attr_search_ctx ctx;
	ntfs_volume *vol;
	MFT_RECORD *mrec;
	runlist *head_rl;
	int mp_size;
	int l;
	int must_delay;
	void *mp;

	vol = expand->vol;
	mrec = expand->mrec;
	head_rl = rl;
	rl_fixup(&rl);
	if ((mp_size = ntfs_get_size_for_mapping_pairs(vol, rl, 0, INT_MAX)) == -1) {
		C_LOG_WARNING("ntfs_get_size_for_mapping_pairs");
	}

	if (a->name_length) {
		u16 name_offs = le16_to_cpu(a->name_offset);
		u16 mp_offs = le16_to_cpu(a->mapping_pairs_offset);
		if (name_offs >= mp_offs) {
			C_LOG_WARNING("Attribute name is after mapping pairs! Please report!");
		}
	}

	/* CHECKME: don't trust mapping_pairs is always the last item in the
	   attribute, instead check for the real size/space */
	l = (int)le32_to_cpu(a->length) - le16_to_cpu(a->mapping_pairs_offset);
	must_delay = 0;
	if (mp_size > l) {
		s32 remains_size;
		char *next_attr;

		C_LOG_VERB("Enlarging attribute header ...");

		mp_size = (mp_size + 7) & ~7;

		C_LOG_VERB("Old mp size      : %d", l);
		C_LOG_VERB("New mp size      : %d", mp_size);
		C_LOG_VERB("Bytes in use     : %u", (unsigned int)
				 le32_to_cpu(mrec->bytes_in_use));

		next_attr = (char *)a + le32_to_cpu(a->length);
		l = mp_size - l;

		C_LOG_VERB("Bytes in use new : %u", l + (unsigned int)
				 le32_to_cpu(mrec->bytes_in_use));
		C_LOG_VERB("Bytes allocated  : %u", (unsigned int)
				 le32_to_cpu(mrec->bytes_allocated));

		remains_size = le32_to_cpu(mrec->bytes_in_use);
		remains_size -= (next_attr - (char *)mrec);

		C_LOG_VERB("increase         : %d", l);
		C_LOG_VERB("shift            : %lld",
				 (long long)remains_size);
		if (le32_to_cpu(mrec->bytes_in_use) + l >
				le32_to_cpu(mrec->bytes_allocated)) {
			C_LOG_VERB("Queuing expansion for later processing");
				/* hack for reusing unmodified old code ! */
			resize.ctx = &ctx;
			ctx.attr = a;
			ctx.mrec = mrec;
			resize.mref = inum;
			resize.delayed_runlists = expand->delayed_runlists;
			resize.mirr_from = MIRR_OLD;
			must_delay = 1;
			replace_later(&resize,rl,head_rl);
			expand->delayed_runlists = resize.delayed_runlists;
		} else {
			memmove(next_attr + l, next_attr, remains_size);
			mrec->bytes_in_use = cpu_to_le32(l +
					le32_to_cpu(mrec->bytes_in_use));
			a->length = cpu_to_le32(le32_to_cpu(a->length) + l);
		}
	}

	if (!must_delay) {
		mp = ntfs_calloc(mp_size);
		if (!mp) {
			C_LOG_WARNING("calloc couldn't get memory");
		}

		if (ntfs_mapping_pairs_build(vol, (u8*)mp, mp_size, rl, 0, NULL)) {
			C_LOG_WARNING("mapping_pairs_build");
		}

		memmove((u8*)a + le16_to_cpu(a->mapping_pairs_offset), mp, mp_size);

		free(mp);
	}
	return (must_delay);
}

static int expand_index_sizes(expand_t *expand)
{
    ntfs_inode *ni;
    int res;

    res = -1;
    ni = ntfs_inode_open(expand->vol, FILE_Bitmap);
    if (ni) {
        NInoSetDirty(ni);
        NInoFileNameSetDirty(ni);
        ntfs_inode_close(ni);
        res = 0;
    }
    return (res);
}

static void delayed_expand(ntfs_volume *vol, struct DELAYED *delayed, struct progress_bar *progress)
{
    unsigned long count;
    struct DELAYED *current;
    int step = 100;

    if (delayed) {
        count = 0;
        /* count by steps because of inappropriate resolution */
        for (current=delayed; current; current=current->next)
            count += step;
        progress_init(progress, 0, count, 0);
        current = delayed;
        count = 0;
        while (current) {
            delayed = current;
            expand_attribute_runlist(vol, delayed);
            count += step;
            progress_update(progress, count);
            current = current->next;
            if (delayed->attr_name)
                free(delayed->attr_name);
            free(delayed->head_rl);
            free(delayed);
        }
    }
}

static int copy_boot(expand_t *expand)
{
    NTFS_BOOT_SECTOR *bs;
    char *buf;
    ntfs_volume *vol;
    s64 mftmirr_lcn;
    s64 written;
    u32 boot_cnt;
    u32 hidden_sectors;
    le32 hidden_sectors_le;
    int res;

    vol = expand->vol;
    res = 0;
    buf = (char*)ntfs_malloc(vol->cluster_size);
    if (buf) {
        /* set the new volume parameters in the bootsector */
        bs = (NTFS_BOOT_SECTOR*)expand->bootsector;
        bs->number_of_sectors = cpu_to_sle64(expand->new_sectors);
        bs->mft_lcn = cpu_to_sle64(expand->mft_lcn);
        mftmirr_lcn = vol->mftmirr_lcn + expand->cluster_increment;
        bs->mftmirr_lcn = cpu_to_sle64(mftmirr_lcn);
        /* the hidden sectors are needed to boot into windows */
        memcpy(&hidden_sectors_le,&bs->bpb.hidden_sectors,4);
        /* alignment messed up on the Sparc */
        if (hidden_sectors_le) {
            hidden_sectors = le32_to_cpu(hidden_sectors_le);
            if (hidden_sectors >= expand->sector_increment)
                hidden_sectors -= expand->sector_increment;
            else
                hidden_sectors = 0;
            hidden_sectors_le = cpu_to_le32(hidden_sectors);
            memcpy(&bs->bpb.hidden_sectors,&hidden_sectors_le,4);
        }
        written = 0;
        boot_cnt = expand->boot_size >> vol->cluster_size_bits;
        while (!res && (written < boot_cnt)) {
            lseek_to_cluster(vol, expand->cluster_increment + written);
            if (!read_all(vol->dev, buf, vol->cluster_size)) {
                if (!written)
                    memcpy(buf, expand->bootsector, vol->sector_size);
                lseek_to_cluster(vol, written);
                if (write_all(vol->dev, buf, vol->cluster_size)) {
                    C_LOG_ERROR("Failed to write the new $Boot");
                    res = -1;
                    } else {
                        written++;
                    }
            }
            else {
                C_LOG_ERROR("Failed to read the old $Boot");
                res = -1;
            }
        }
        free(buf);
    }
    else {
        C_LOG_ERROR("Failed to allocate buffer");
        res = -1;
    }
    return (res);
}

static int copy_mftmirr(expand_t *expand)
{
    ntfs_volume *vol;
    s64 pos;
    s64 inum;
    int res;
    u16 usa_ofs;
    le16 *pusn;
    u16 usn;

    vol = expand->vol;
    res = 0;
    for (inum=FILE_MFT; !res && (inum<=FILE_Volume); inum++) {
        /* read the new $MFT */
        pos = (expand->mft_lcn << vol->cluster_size_bits) + (inum << vol->mft_record_size_bits);
        if (ntfs_mst_pread(vol->dev, pos, 1, vol->mft_record_size, expand->mrec) == 1) {
            /* overwrite the old $MFTMirr */
            pos = (vol->mftmirr_lcn << vol->cluster_size_bits) + (inum << vol->mft_record_size_bits) + expand->byte_increment;
            usa_ofs = le16_to_cpu(expand->mrec->usa_ofs);
            pusn = (le16*)((u8*)expand->mrec + usa_ofs);
            usn = le16_to_cpu(*pusn) - 1;
            if (!usn || (usn == 0xffff)) {
                usn = -2;
            }
            *pusn = cpu_to_le16(usn);
            if (ntfs_mst_pwrite(vol->dev, pos, 1, vol->mft_record_size, expand->mrec) != 1) {
                C_LOG_ERROR("Failed to overwrite the old $MFTMirr");
                res = -1;
            }
        } else {
            C_LOG_WARNING("Failed to write the new $MFT");
            res = -1;
        }
    }
    return (res);
}


static int write_bitmap(expand_t *expand)
{
    ntfs_volume *vol;
    s64 bw;
    u64 cluster;
    int res;

    res = -1;
    vol = expand->vol;
    cluster = vol->nr_clusters + expand->cluster_increment;
    while (cluster < (expand->bitmap_size << 3)) {
        expand->bitmap[cluster >> 3] |= 1 << (cluster & 7);
        cluster++;
    }
    /* write the full allocation (to avoid having to read) */
    bw = ntfs_pwrite(vol->dev, expand->boot_size, expand->bitmap_allocated, expand->bitmap);
    if (bw == (s64)expand->bitmap_allocated)
        res = 0;
    else {
        if (bw != -1)
            errno = EINVAL;
        if (!bw)
            C_LOG_WARNING("Failed to write the bitmap (size=0)");
        else
            C_LOG_WARNING("Error rewriting the bitmap");
    }
    return (res);
}

static int write_bootsector(expand_t *expand)
{
    ntfs_volume *vol;
    s64 bw;
    int res;

    res = -1;
    vol = expand->vol;
    bw = ntfs_pwrite(vol->dev, expand->new_sectors*vol->sector_size, vol->sector_size, expand->bootsector);
    if (bw == vol->sector_size)
        res = 0;
    else {
        if (bw != -1)
            errno = EINVAL;
        if (!bw)
            C_LOG_WARNING("Failed to rewrite the bootsector (size=0)");
        else
            C_LOG_WARNING("Error rewriting the bootsector");
    }
    return (res);
}

static int set_bitmap(expand_t *expand, runlist_element *rl)
{
    int res;
    s64 lcn;
    s64 lcn_end;
    BOOL reallocated;

    res = -1;
    reallocated = FALSE;
    if ((rl->lcn >= 0) && (rl->length > 0) && ((rl->lcn + rl->length) <= (expand->vol->nr_clusters + expand->cluster_increment))) {
        lcn = rl->lcn;
        lcn_end = lcn + rl->length;
        while ((lcn & 7) && (lcn < lcn_end)) {
            if (expand->bitmap[lcn >> 3] & 1 << (lcn & 7)) {
                reallocated = TRUE;
            }
            expand->bitmap[lcn >> 3] |= 1 << (lcn & 7);
            lcn++;
        }
        while ((lcn_end - lcn) >= 8) {
            if (expand->bitmap[lcn >> 3]) {
                reallocated = TRUE;
            }
            expand->bitmap[lcn >> 3] = 255;
            lcn += 8;
        }
        while (lcn < lcn_end) {
            if (expand->bitmap[lcn >> 3] & 1 << (lcn & 7)) {
                reallocated = TRUE;
            }
            expand->bitmap[lcn >> 3] |= 1 << (lcn & 7);
            lcn++;
        }
        if (reallocated) {
            C_LOG_WARNING("Reallocated cluster found in run" " lcn 0x%llx length %lld", (long long)rl->lcn,(long long)rl->length);
        }
        else {
            res = 0;
        }
    } else {
        C_LOG_WARNING("Bad run : lcn 0x%llx length %lld", (long long)rl->lcn,(long long)rl->length);
    }
    return (res);
}

static BOOL can_expand(expand_t *expand, ntfs_volume *vol)
{
	s64 old_sector_count;
	s64 sectors_needed;
	s64 clusters;
	s64 minimum_size;
	s64 got;
	s64 advice;
	s64 bitmap_bits;
	BOOL ok;

	ok = TRUE;
	old_sector_count = vol->nr_clusters << (vol->cluster_size_bits - vol->sector_size_bits);

    /* do not include the space lost near the end */
	expand->cluster_increment = (expand->new_sectors >> (vol->cluster_size_bits - vol->sector_size_bits)) - vol->nr_clusters;
	expand->byte_increment = expand->cluster_increment << vol->cluster_size_bits;
	expand->sector_increment = expand->byte_increment >> vol->sector_size_bits;
	C_LOG_VERB("Sectors allocated to volume :  old %lld current %lld difference %lld",
			(long long)old_sector_count,
			(long long)(old_sector_count + expand->sector_increment),
			(long long)expand->sector_increment);
	C_LOG_VERB("Clusters allocated to volume : old %lld current %lld difference %lld",
			(long long)vol->nr_clusters,
			(long long)(vol->nr_clusters + expand->cluster_increment),
			(long long)expand->cluster_increment);
		/* the new size must be bigger */
	if ((expand->sector_increment < 0) || (!expand->sector_increment)) {
		C_LOG_WARNING("Cannot expand volume : the partition has not been expanded");
		ok = FALSE;
	}

    /* the old bootsector must match the backup */
	got = ntfs_pread(expand->vol->dev, expand->byte_increment, vol->sector_size, expand->mrec);
	if ((got != vol->sector_size) || memcmp(expand->bootsector,expand->mrec,vol->sector_size)) {
		C_LOG_WARNING("The backup bootsector does not match the old bootsector");
		ok = FALSE;
	}
	if (ok) {
	    /* read the first MFT record, to get the MFT size */
		expand->mft_size = get_data_size(expand, FILE_MFT);

	    /* read the 6th MFT record, to get the $Boot size */
		expand->boot_size = get_data_size(expand, FILE_Boot);
		if (!expand->mft_size || !expand->boot_size) {
			ok = FALSE;
		}
	    else {
			/*
			 * The bitmap is one bit per full cluster,
			 * accounting for the backup bootsector.
			 * When evaluating the minimal size, the bitmap
			 * size must be adapted to the minimal size :
			 *  bits = clusters + ceil(clusters/clustersize)
			 */
	        {
				bitmap_bits = (expand->new_sectors + 1)
			    		>> (vol->cluster_size_bits
						- vol->sector_size_bits);
			}
			/* byte size must be a multiple of 8 */
			expand->bitmap_size = ((bitmap_bits + 63) >> 3) & -8;
			expand->bitmap_allocated = ((expand->bitmap_size - 1)
				| (vol->cluster_size - 1)) + 1;
			expand->mft_lcn = (expand->boot_size
					+ expand->bitmap_allocated)
						>> vol->cluster_size_bits;
			/*
			 * Check whether $Boot, $Bitmap and $MFT can fit
			 * into the expanded space.
			 */
			sectors_needed = (expand->boot_size + expand->mft_size + expand->bitmap_allocated) >> vol->sector_size_bits;
			if ((sectors_needed >= expand->sector_increment)) {
				ok = FALSE;
			}
		}
	}
	if (ok) {
		advice = expand->byte_increment;
		/* the increment must be an integral number of clusters */
		if (expand->byte_increment & (vol->cluster_size - 1)) {
			advice = expand->byte_increment & ~vol->cluster_size;
			ok = FALSE;
		}
	}
	if (ok)
		ok = !check_expand_constraints(expand);
	if (ok) {
		minimum_size = (expand->original_sectors
						<< vol->sector_size_bits)
					+ expand->boot_size
					+ expand->mft_size
					+ expand->bitmap_allocated;

		printf("You must expand the partition to at least %lld bytes,",
			(long long)(minimum_size + vol->sector_size));
		printf("and you may add a multiple of %ld bytes to this size.",
			(long)vol->cluster_size);
		printf("The minimum NTFS volume size is %lld bytes",
			(long long)minimum_size);
		ok = FALSE;
	}
	return (ok);
}

static int check_expand_constraints(expand_t *expand)
{
    static ntfschar bad[] = {
        const_cpu_to_le16('$'), const_cpu_to_le16('B'),
        const_cpu_to_le16('a'), const_cpu_to_le16('d')
} ;
    ATTR_RECORD *a;
    runlist_element *rl;
    VOLUME_INFORMATION *volinfo;
    VOLUME_FLAGS flags;
    int res;

    res = 0;
    /* extents for $MFT are not supported */
    if (get_unnamed_attr(expand, AT_ATTRIBUTE_LIST, FILE_MFT)) {
        C_LOG_WARNING("The $MFT is too much fragmented");
        res = -1;
    }
    /* fragmented $MFTMirr is not supported */
    a = get_unnamed_attr(expand, AT_DATA, FILE_MFTMirr);
    if (a) {
        rl = ntfs_mapping_pairs_decompress(expand->vol, a, NULL);
        if (!rl || !rl[0].length || rl[1].length) {
            C_LOG_WARNING("$MFTMirr is bad or fragmented");
            res = -1;
        }
        free(rl);
    }
    /* fragmented $Boot is not supported */
    a = get_unnamed_attr(expand, AT_DATA, FILE_Boot);
    if (a) {
        rl = ntfs_mapping_pairs_decompress(expand->vol, a, NULL);
        if (!rl || !rl[0].length || rl[1].length) {
            C_LOG_WARNING("$Boot is bad or fragmented");
            res = -1;
        }
        free(rl);
    }
    /* Volume should not be marked dirty */
    a = get_unnamed_attr(expand, AT_VOLUME_INFORMATION, FILE_Volume);
    if (a) {
        volinfo = (VOLUME_INFORMATION*)
                (le16_to_cpu(a->value_offset) + (char*)a);
        flags = volinfo->flags;
        if (flags & VOLUME_IS_DIRTY) {
            C_LOG_WARNING("Volume is scheduled for check.Run chkdsk /f"
             " and please try again, or see option -f.");
            res = -1;
        }
    } else {
        C_LOG_WARNING("Could not get Volume flags");
        res = -1;
    }

    /* There should not be too many bad clusters */
    a = read_and_get_attr(expand, AT_DATA, FILE_BadClus, bad, 4);
    if (!a || !a->non_resident) {
        C_LOG_WARNING("Resident attribute in $BadClust! Please report to %s", NTFS_DEV_LIST);
        res = -1;
    } else
        if (check_expand_bad_sectors(expand,a))
            res = -1;
    return (res);
}

static int check_expand_bad_sectors(expand_t *expand, ATTR_RECORD *a)
{
    runlist *rl;
    int res;
    s64 i, badclusters = 0;

    res = 0;
    C_LOG_VERB("Checking for bad sectors ...");

    if (find_attr(expand->mrec, AT_ATTRIBUTE_LIST, NULL, 0)) {
        C_LOG_WARNING("Hopelessly many bad sectors have been detected!");
        res = -1;
    } else {

        /*
         * FIXME: The below would be partial for non-base records in the
         * not yet supported multi-record case. Alternatively use audited
         * ntfs_attr_truncate after an umount & mount.
         */
        rl = ntfs_mapping_pairs_decompress(expand->vol, a, NULL);
        if (!rl) {
            C_LOG_WARNING("Decompressing $BadClust: $Bad mapping pairs failed");
            res = -1;
        } else {
            for (i = 0; rl[i].length; i++) {
                /* CHECKME: LCN_RL_NOT_MAPPED check isn't needed */
                if (rl[i].lcn == LCN_HOLE
                    || rl[i].lcn == LCN_RL_NOT_MAPPED)
                    continue;

                badclusters += rl[i].length;
                C_LOG_VERB("Bad cluster: %#8llx - %#llx"
                        "    (%lld)",
                        (long long)rl[i].lcn,
                        (long long)rl[i].lcn
                            + rl[i].length - 1,
                        (long long)rl[i].length);
            }

            if (badclusters) {
                C_LOG_WARNING("%sThis software has detected that the disk has at least %lld bad sector%s.", "WARNING: ", (long long)badclusters, badclusters - 1 ? "s" : "");
                C_LOG_WARNING("WARNING: Bad sectors can cause reliability problems and massive data loss!!!");
            }
            free(rl);
        }
    }
    return (res);
}

static u8 *get_mft_bitmap(expand_t *expand)
{
    ATTR_RECORD *a;
    ntfs_volume *vol;
    runlist_element *rl;
    runlist_element *prl;
    u32 bitmap_size;
    BOOL ok;

    expand->mft_bitmap = (u8*)NULL;
    vol = expand->vol;
    /* get the runlist of unnamed bitmap */
    a = get_unnamed_attr(expand, AT_BITMAP, FILE_MFT);
    ok = TRUE;
    bitmap_size = sle64_to_cpu(a->allocated_size);
    if (a
        && a->non_resident
        && ((bitmap_size << (vol->mft_record_size_bits + 3))
            >= expand->mft_size)) {
        // rl in extent not implemented
        rl = ntfs_mapping_pairs_decompress(expand->vol, a,
                        (runlist_element*)NULL);
        expand->mft_bitmap = (u8*)ntfs_calloc(bitmap_size);
        if (rl && expand->mft_bitmap) {
            for (prl=rl; prl->length && ok; prl++) {
                lseek_to_cluster(vol, prl->lcn + expand->cluster_increment);
                ok = !read_all(vol->dev, expand->mft_bitmap + (prl->vcn << vol->cluster_size_bits), prl->length << vol->cluster_size_bits);
            }
            if (!ok) {
                C_LOG_WARNING("Could not read the MFT bitmap");
                free(expand->mft_bitmap);
                expand->mft_bitmap = (u8*)NULL;
            }
            free(rl);
        } else {
            C_LOG_WARNING("Could not get the MFT bitmap");
        }
            } else
                C_LOG_WARNING("Invalid MFT bitmap");
    return (expand->mft_bitmap);
}

static s64 get_data_size(expand_t *expand, s64 inum)
{
    ATTR_RECORD *a;
    s64 size;

    size = 0;
    /* get the size of unnamed $DATA */
    a = get_unnamed_attr(expand, AT_DATA, inum);
    if (a && a->non_resident)
        size = sle64_to_cpu(a->allocated_size);
    if (!size) {
        C_LOG_WARNING("Bad record %lld, could not get its size", (long long)inum);
    }
    return (size);
}


static ATTR_RECORD *read_and_get_attr(expand_t *expand, ATTR_TYPES type, s64 inum, ntfschar *name, int namelen)
{
    ntfs_volume *vol;
    ATTR_RECORD *a;
    MFT_RECORD *mrec;
    s64 pos;
    int got;

    a = (ATTR_RECORD*)NULL;
    mrec = expand->mrec;
    vol = expand->vol;
    pos = (vol->mft_lcn << vol->cluster_size_bits)
        + (inum << vol->mft_record_size_bits)
        + expand->byte_increment;
    got = ntfs_mst_pread(vol->dev, pos, 1, vol->mft_record_size, mrec);
    if ((got == 1) && (mrec->flags & MFT_RECORD_IN_USE)) {
        a = find_attr(expand->mrec, type, name, namelen);
    }
    if (!a) {
        C_LOG_WARNING("Could not find attribute 0x%lx in inode %lld", (long)le32_to_cpu(type), (long long)inum);
    }
    return (a);
}

static ATTR_RECORD *get_unnamed_attr(expand_t *expand, ATTR_TYPES type, s64 inum)
{
    ntfs_volume *vol;
    ATTR_RECORD *a;
    MFT_RECORD *mrec;
    s64 pos;
    BOOL found;
    int got;

    found = FALSE;
    a = (ATTR_RECORD*)NULL;
    mrec = expand->mrec;
    vol = expand->vol;
    pos = (vol->mft_lcn << vol->cluster_size_bits)
        + (inum << vol->mft_record_size_bits)
        + expand->byte_increment;
    got = ntfs_mst_pread(vol->dev, pos, 1, vol->mft_record_size, mrec);
    if ((got == 1) && (mrec->flags & MFT_RECORD_IN_USE)) {
        a = find_attr(expand->mrec, type, NULL, 0);
        found = a && (a->type == type) && !a->name_length;
    }
    /* not finding the attribute list is not an error */
    if (!found && (type != AT_ATTRIBUTE_LIST)) {
        C_LOG_WARNING("Could not find attribute 0x%lx in inode %lld", (long)le32_to_cpu(type), (long long)inum);
        a = (ATTR_RECORD*)NULL;
    }
    return (a);
}

static ATTR_RECORD *find_attr(MFT_RECORD *mrec, ATTR_TYPES type, ntfschar *name, int namelen)
{
    ATTR_RECORD *a;
    u32 offset;
    ntfschar *attrname;

    /* fetch the requested attribute */
    offset = le16_to_cpu(mrec->attrs_offset);
    a = (ATTR_RECORD*)((char*)mrec + offset);
    attrname = (ntfschar*)((char*)a + le16_to_cpu(a->name_offset));
    while ((a->type != AT_END)
        && ((a->type != type)
        || (a->name_length != namelen)
        || (namelen && memcmp(attrname,name,2*namelen)))
        && (offset < le32_to_cpu(mrec->bytes_in_use))) {
        offset += le32_to_cpu(a->length);
        a = (ATTR_RECORD*)((char*)mrec + offset);
        if (namelen)
            attrname = (ntfschar*)((char*)a
                + le16_to_cpu(a->name_offset));
        }
    if ((a->type != type)
        || (a->name_length != namelen)
        || (namelen && memcmp(attrname,name,2*namelen)))
        a = (ATTR_RECORD*)NULL;
    return (a);
}

static void progress_init(struct progress_bar *p, u64 start, u64 stop, int flags)
{
    p->start = start;
    p->stop = stop;
    p->unit = 100.0 / (stop - start);
    p->resolution = 100;
    p->flags = flags;
}

static ntfs_volume *mount_volume(const char* volume)
{
    unsigned long mntflag;
    ntfs_volume *vol = NULL;

    if (ntfs_check_if_mounted(volume, &mntflag)) {
        C_LOG_WARNING("Failed to check '%s' mount state", volume);
        printf("Probably /etc/mtab is missing. It's too risky to continue. You might try an another Linux distro.");
        exit(1);
    }
    if (mntflag & NTFS_MF_MOUNTED) {
        if (!(mntflag & NTFS_MF_READONLY)) {
            C_LOG_WARNING("Device '%s' is mounted read-write. You must 'umount' it first.", volume);
        }
    }

    vol = check_volume_dev(volume);
    if (NTFS_MAX_CLUSTER_SIZE < vol->cluster_size)
        C_LOG_WARNING("Cluster size %u is too large!", (unsigned int)vol->cluster_size);

    if (ntfs_volume_get_free_space(vol)) {
        C_LOG_WARNING("Failed to update the free space");
    }

    C_LOG_VERB("Device name         : %s", volume);
    C_LOG_VERB("volume version      : %d.%d", vol->major_ver, vol->minor_ver);
    if (ntfs_version_is_supported(vol)) {
        C_LOG_WARNING("Unknown FS version");
    }

    C_LOG_VERB("Cluster size        : %u bytes", (unsigned int)vol->cluster_size);
    C_LOG_VERB("Current volume size", vol_size(vol, vol->nr_clusters));

    return vol;
}

static ntfs_volume* check_volume_dev (const char* dev)
{
    ntfs_volume *myvol = NULL;

    /*
     * Pass NTFS_MNT_FORENSIC so that the mount process does not modify the
     * volume at all.  We will do the logfile emptying and dirty setting
     * later if needed.
     */
    if (!(myvol = ntfs_mount(dev, NTFS_MNT_FORENSIC))) {
        int err = errno;

        C_LOG_DEBUG("Opening '%s' as FS failed", dev);
        switch (err) {
        case EINVAL :
            C_LOG_DEBUG("invalid '%s'", dev);
            break;
        case EIO :
            printf("error EIO: %s", dev);
            break;
        case EPERM :
            printf("EPERM: %s", dev);
            break;
        case EOPNOTSUPP :
            printf("EOPNOTSUPP: %s", dev);
            break;
        case EBUSY :
            printf("EBUSY: %s", dev);
            break;
        default :
            break;
        }
        exit(1);
    }
    return myvol;
}

static s64 vol_size(ntfs_volume *v, s64 nr_clusters)
{
    /* add one sector_size for the backup boot sector */
    return nr_clusters * v->cluster_size + v->sector_size;
}

static void progress_update(struct progress_bar *p, u64 current)
{
    float percent;

    if (!(p->flags & NTFS_PROGBAR))
        return;
    if (p->flags & NTFS_PROGBAR_SUPPRESS)
        return;

    /* WARNING: don't modify the texts, external tools grep for them */
    percent = p->unit * current;
    if (current != p->stop) {
        if ((current - p->start) % p->resolution)
            return;
        printf("%6.2f percent completed\r", percent);
    } else
        printf("100.00 percent completed");
    fflush(stdout);
}

static void rl_fixup(runlist **rl)
{
    runlist *tmp = *rl;

    if (tmp->lcn == LCN_RL_NOT_MAPPED) {
        s64 unmapped_len = tmp->length;
        C_LOG_VERB("Skip unmapped run at the beginning...");

        if (!tmp->length) {
            C_LOG_WARNING("Empty unmapped runlist! Please report!");
        }
        (*rl)++;
        for (tmp = *rl; tmp->length; tmp++)
            tmp->vcn -= unmapped_len;
    }

    for (tmp = *rl; tmp->length; tmp++) {
        if (tmp->lcn == LCN_RL_NOT_MAPPED) {
            C_LOG_VERB("Skip unmapped run at the end...");
            if (tmp[1].length) {
                C_LOG_WARNING("Unmapped runlist in the middle! Please report!");
            }
            tmp->lcn = LCN_ENOENT;
            tmp->length = 0;
        }
    }
}

static void replace_later(ntfs_resize_t *resize, runlist *rl, runlist *head_rl)
{
    struct DELAYED *delayed;
    struct DELAYED *previous;
    ATTR_RECORD *a;
    MFT_REF mref;
    leMFT_REF lemref;
    int name_len;
    ntfschar *attr_name;

    /* save the attribute parameters, to be able to find it later */
    a = resize->ctx->attr;
    name_len = a->name_length;
    attr_name = (ntfschar*)NULL;
    if (name_len) {
        attr_name = (ntfschar*)ntfs_malloc(name_len*sizeof(ntfschar));
        if (attr_name)
            memcpy(attr_name,(u8*)a + le16_to_cpu(a->name_offset),
                    name_len*sizeof(ntfschar));
    }
    delayed = (struct DELAYED*)ntfs_malloc(sizeof(struct DELAYED));
    if (delayed && (attr_name || !name_len)) {
        lemref = resize->ctx->mrec->base_mft_record;
        if (lemref)
            mref = le64_to_cpu(lemref);
        else
            mref = resize->mref;
        delayed->mref = MREF(mref);
        delayed->type = a->type;
        delayed->attr_name = attr_name;
        delayed->name_len = name_len;
        delayed->lowest_vcn = sle64_to_cpu(a->lowest_vcn);
        delayed->rl = rl;
        delayed->head_rl = head_rl;
        /* Queue ahead of list if this is MFT or head is not MFT */
        if ((delayed->mref == FILE_MFT)
            || !resize->delayed_runlists
            || (resize->delayed_runlists->mref != FILE_MFT)) {
            delayed->next = resize->delayed_runlists;
            resize->delayed_runlists = delayed;
            } else {
                /* Queue after all MFTs is this is not MFT */
                previous = resize->delayed_runlists;
                while (previous->next
                    && (previous->next->mref == FILE_MFT))
                    previous = previous->next;
                delayed->next = previous->next;
                previous->next = delayed;
            }
    }
    else {
        C_LOG_WARNING("Could not store delayed update data");
    }
}

static void lseek_to_cluster(ntfs_volume *vol, s64 lcn)
{
    off_t pos = (off_t)(lcn * vol->cluster_size);
    if (vol->dev->d_ops->seek(vol->dev, pos, SEEK_SET) == (off_t)-1) {
        C_LOG_WARNING("seek failed to position %lld", (long long)lcn);
    }
}

static void expand_attribute_runlist(ntfs_volume *vol, struct DELAYED *delayed)
{
    ntfs_inode *ni;
    ntfs_attr *na;
    ATTR_TYPES type;
    MFT_REF mref;
    runlist_element *rl;

    /* open the inode */
    mref = delayed->mref;
#ifndef BAN_NEW_TEXT
    C_LOG_VERB("Processing a delayed update for inode %lld", (long long)mref);
#endif
    type = delayed->type;
    rl = delayed->rl;

    /* The MFT inode is permanently open, do not reopen or close */
    if (mref == FILE_MFT)
        ni = vol->mft_ni;
    else
        ni = ntfs_inode_open(vol,mref);
    if (ni) {
        if (mref == FILE_MFT)
            na = (type == AT_DATA ? vol->mft_na : vol->mftbmp_na);
        else
            na = ntfs_attr_open(ni, type,
                    delayed->attr_name, delayed->name_len);
        if (na) {
            /*
             * The runlist is first updated in memory, and
             * the updated one is used for updating on device
             */
            if (!ntfs_attr_map_whole_runlist(na)) {
                if (replace_runlist(na,rl,delayed->lowest_vcn) || ntfs_attr_update_mapping_pairs(na,0)) {
                    C_LOG_WARNING("Could not update runlist for attribute 0x%lx in inode %lld", (long)le32_to_cpu(type),(long long)mref);
                }
            }
            else {
                C_LOG_WARNING("Could not map attribute 0x%lx in inode %lld", (long)le32_to_cpu(type),(long long)mref);
            }
            if (mref != FILE_MFT)
                ntfs_attr_close(na);
        }
        else {
            C_LOG_WARNING("Could not open attribute 0x%lx in inode %lld", (long)le32_to_cpu(type),(long long)mref);
        }
        ntfs_inode_mark_dirty(ni);
        if ((mref != FILE_MFT) && ntfs_inode_close(ni)) {
            C_LOG_WARNING("Failed to close inode %lld through the library", (long long)mref);
        }
    }
    else {
        C_LOG_WARNING("Could not open inode %lld through the library", (long long)mref);
    }
}

static int read_all(struct ntfs_device *dev, void *buf, int count)
{
    int i;

    while (count > 0) {
        i = count;
        if (!NDevReadOnly(dev)) {
            i = dev->d_ops->read(dev, buf, count);
        }

        if (i < 0) {
            if (errno != EAGAIN && errno != EINTR)
                return -1;
        }
        else if (i > 0) {
            count -= i;
            buf = i + (char *)buf;
        }
        else {
            C_LOG_WARNING("Unexpected end of file!");
        }
    }
    return 0;
}

static int write_all(struct ntfs_device *dev, void *buf, int count)
{
    int i;

    while (count > 0) {
        i = count;
        if (!NDevReadOnly(dev)) {
            i = dev->d_ops->write(dev, buf, count);
        }

        if (i < 0) {
            if (errno != EAGAIN && errno != EINTR) {
                return -1;
            }
        }
        else {
            count -= i;
            buf = i + (char *)buf;
        }
    }
    return 0;
}

static int replace_runlist(ntfs_attr *na, const runlist_element *reprl, VCN lowest_vcn)
{
    const runlist_element *prep;
    const runlist_element *pold;
    runlist_element *pnew;
    runlist_element *newrl;
    VCN nextvcn;
    s32 oldcnt, newcnt;
    s32 newsize;
    int r;

    r = -1; /* default return */
    /* allocate a new runlist able to hold both */
    oldcnt = 0;
    while (na->rl[oldcnt].length)
        oldcnt++;
    newcnt = 0;
    while (reprl[newcnt].length)
        newcnt++;
    newsize = ((oldcnt + newcnt)*sizeof(runlist_element) + 4095) & -4096;
    newrl = (runlist_element*)malloc(newsize);
    if (newrl) {
        /* copy old runs until reaching replaced ones */
        pnew = newrl;
        pold = na->rl;
        while (pold->length
            && ((pold->vcn + pold->length)
                 <= (reprl[0].vcn + lowest_vcn))) {
            *pnew = *pold;
            pnew++;
            pold++;
                 }
        /* split a possible old run partially overlapped */
        if (pold->length
            && (pold->vcn < (reprl[0].vcn + lowest_vcn))) {
            pnew->vcn = pold->vcn;
            pnew->lcn = pold->lcn;
            pnew->length = reprl[0].vcn + lowest_vcn - pold->vcn;
            pnew++;
            }
        /* copy new runs */
        prep = reprl;
        nextvcn = prep->vcn + lowest_vcn;
        while (prep->length) {
            pnew->vcn = prep->vcn + lowest_vcn;
            pnew->lcn = prep->lcn;
            pnew->length = prep->length;
            nextvcn = pnew->vcn + pnew->length;
            pnew++;
            prep++;
        }
        /* locate the first fully replaced old run */
        while (pold->length
            && ((pold->vcn + pold->length) <= nextvcn)) {
            pold++;
            }
        /* split a possible old run partially overlapped */
        if (pold->length
            && (pold->vcn < nextvcn)) {
            pnew->vcn = nextvcn;
            pnew->lcn = pold->lcn + nextvcn - pold->vcn;
            pnew->length = pold->length - nextvcn + pold->vcn;
            pnew++;
            }
        /* copy old runs beyond replaced ones */
        while (pold->length) {
            *pnew = *pold;
            pnew++;
            pold++;
        }
        /* the terminator is same as the old one */
        *pnew = *pold;
        /* deallocate the old runlist and replace */
        free(na->rl);
        na->rl = newrl;
        r = 0;
    }
    return (r);
}

static int check_bad_sectors(ntfs_volume *vol)
{
    ntfs_attr_search_ctx *ctx;
    ntfs_attr *na;
    runlist *rl;
    s64 i, badclusters = 0;

    C_LOG_VERB("Checking for bad sectors ...");

    lookup_data_attr(vol, FILE_BadClus, "$Bad", &ctx);

    na = open_badclust_bad_attr(ctx);
    if (!na) {
        C_LOG_WARNING("Could not access the bad sector list");
        return -1;
    }
    rl = na->rl;
    for (i = 0; rl[i].length; i++) {
        /* CHECKME: LCN_RL_NOT_MAPPED check isn't needed */
        if (rl[i].lcn == LCN_HOLE || rl[i].lcn == LCN_RL_NOT_MAPPED) {
            continue;
        }

        badclusters += rl[i].length;
        C_LOG_VERB("Bad cluster: %#8llx - %#llx    (%lld)", (long long)rl[i].lcn, (long long)rl[i].lcn + rl[i].length - 1, (long long)rl[i].length);
    }

    if (badclusters) {
        C_LOG_WARNING("%sThis software has detected that the disk has at least %lld bad sector%s.", "WARNING: ", (long long)badclusters, badclusters - 1 ? "s" : "");
        C_LOG_WARNING("WARNING: Bad sectors can cause reliability problems and massive data loss!!!");
    }

    ntfs_attr_close(na);
#if CLEAN_EXIT
    close_inode_and_context(ctx);
#else
    ntfs_attr_put_search_ctx(ctx);
#endif

    return badclusters;
}

static void print_disk_usage(ntfs_volume *vol, s64 nr_used_clusters)
{
    s64 total, used;

    total = vol->nr_clusters * vol->cluster_size;
    used = nr_used_clusters * vol->cluster_size;

    /* WARNING: don't modify the text, external tools grep for it */
    printf("Space in use       : %lld MB (%.1f%%)\n", (long long)rounded_up_division(used, NTFS_MBYTE), 100.0 * ((float)used / total));
}

static void set_resize_constraints(ntfs_resize_t *resize)
{
    s64 nr_mft_records, inode;
    ntfs_inode *ni;

    nr_mft_records = resize->vol->mft_na->initialized_size >> resize->vol->mft_record_size_bits;
    for (inode = 0; inode < nr_mft_records; inode++) {
        ni = ntfs_inode_open(resize->vol, (MFT_REF)inode);
        if (ni == NULL) {
            if (errno == EIO || errno == ENOENT) {
                continue;
            }
            C_LOG_WARNING("Reading inode %lld failed", (long long)inode);
            return;
        }

        if (ni->mrec->base_mft_record) {
            goto close_inode;
        }

        resize->ni = ni;
        resize_constraints_by_attributes(resize);
close_inode:
        if (inode_close(ni) != 0) exit(1);
    }
}

static void set_disk_usage_constraint(ntfs_resize_t *resize)
{
    /* last lcn for a filled up volume (no empty space) */
    s64 last = resize->inuse - 1;

    if (resize->last_unsupp < last) {
        resize->last_unsupp = last;
    }
}

static void check_resize_constraints(ntfs_resize_t *resize)
{
    s64 new_size = resize->new_volume_size;

    /* FIXME: resize.shrink true also if only -i is used */
    if (!resize->shrink) {
        return;
    }

    if (resize->inuse == resize->vol->nr_clusters) {
        C_LOG_WARNING("Volume is full. To shrink it, delete unused files.");
    }

    /* FIXME: reserve some extra space so Windows can boot ... */
    if (new_size < resize->inuse) {
        C_LOG_WARNING("New size can't be less than the space already"
             " occupied by data.\nYou either need to delete unused"
             " files or see the -i option.\n");
    }

    if (new_size <= resize->last_unsupp) {
        C_LOG_WARNING("The fragmentation type, you have, isn't "
             "supported yet. Rerun ntfsresize\nwith "
             "the -i option to estimate the smallest "
             "shrunken volume size supported.\n");
    }

    print_num_of_relocations(resize);
}

static void prepare_volume_fixup(ntfs_volume *vol)
{
    C_LOG_VERB("Schedule chkdsk for NTFS consistency check at Windows boot time ...");
    vol->flags |= VOLUME_IS_DIRTY;
    if (ntfs_volume_write_flags(vol, vol->flags)) {
        C_LOG_WARNING("Failed to set the volume dirty");
        return;
    }

    /* Porting note: This flag does not exist in libntfs-3g. The dirty flag
     * is never modified by libntfs-3g on unmount and we set it above. We
     * can safely comment out this statement. */
    /* NVolSetWasDirty(vol); */

    if (vol->dev->d_ops->sync(vol->dev) == -1) {
        C_LOG_WARNING("Failed to sync device");
        return;
    }

    C_LOG_VERB("Resetting $LogFile ... (this might take a while)");
    if (ntfs_logfile_reset(vol)) {
        C_LOG_WARNING("Failed to reset $LogFile");
        return;
    }
    if (vol->dev->d_ops->sync(vol->dev) == -1) {
        C_LOG_WARNING("Failed to sync device");
        return;
    }
}

static void relocate_inodes(ntfs_resize_t *resize)
{
	s64 nr_mft_records;
	MFT_REF mref;
	VCN highest_vcn;
	s64 length;

	printf("Relocating needed data ...\n");

	progress_init(&resize->progress, 0, resize->relocations, resize->progress.flags);
	resize->relocations = 0;

	resize->mrec = ntfs_malloc(resize->vol->mft_record_size);
	if (!resize->mrec) {
		C_LOG_WARNING("ntfs_malloc failed");
	    return;
	}

	nr_mft_records = resize->vol->mft_na->initialized_size >> resize->vol->mft_record_size_bits;

	/*
	 * If we need to relocate the first run of the MFT DATA,
	 * do it now, to have a better chance of getting at least
	 * 16 records in the first chunk. This is mandatory to be
	 * later able to read an MFT extent in record 15.
	 * Should this fail, we can stop with no damage, the volume
	 * is still in its initial state.
	 */
	if (!resize->vol->mft_na->rl) {
		C_LOG_WARNING("Internal error : no runlist for $MFT");
	}

	if ((resize->vol->mft_na->rl->lcn + resize->vol->mft_na->rl->length) >= resize->new_volume_size) {
		/*
		 * The length of the first run is normally found in
		 * mft_na. However in some rare circumstance, this is
		 * merged with the first run of an extent of MFT,
		 * which implies there is a single run in the base record.
		 * So we have to make sure not to overflow from the
		 * runs present in the base extent.
		 */
		length = resize->vol->mft_na->rl->length;
		if (ntfs_file_record_read(resize->vol, FILE_MFT, &resize->mrec, NULL) || !(resize->ctx = attr_get_search_ctx(NULL, resize->mrec))) {
			C_LOG_WARNING("Could not read the base record of MFT");
		    return;
		}
		while (!ntfs_attrs_walk(resize->ctx) && (resize->ctx->attr->type != AT_DATA)) {}
		if (resize->ctx->attr->type == AT_DATA) {
			sle64 high_le = resize->ctx->attr->highest_vcn;
			if (sle64_to_cpu(high_le) < length) {
				length = sle64_to_cpu(high_le) + 1;
			}
		}
	    else {
			C_LOG_WARNING("Could not find the DATA of MFT");
	        return;
		}
		ntfs_attr_put_search_ctx(resize->ctx);
		resize->new_mft_start = alloc_cluster(&resize->lcn_bitmap, length, resize->new_volume_size, 0);
		if (!resize->new_mft_start
		    || (((resize->new_mft_start->length
			<< resize->vol->cluster_size_bits)
			    >> resize->vol->mft_record_size_bits) < 16)) {
			C_LOG_WARNING("Could not allocate 16 records in the first MFT chunk");
		    return;
		}
		resize->mirr_from = MIRR_NEWMFT;
	}

	for (mref = 0; mref < (MFT_REF)nr_mft_records; mref++) {
		relocate_inode(resize, mref, 0);
	}

	while (1) {
		highest_vcn = resize->mft_highest_vcn;
		mref = nr_mft_records;
		do {
			relocate_inode(resize, --mref, 1);
			if (resize->mft_highest_vcn == 0) {
				goto done;
			}
		} while (mref);

		if (highest_vcn == resize->mft_highest_vcn) {
			C_LOG_WARNING("Sanity check failed! Highest_vcn = %lld. Please report!", (long long)highest_vcn);
		    return;
		}
	}
done:
	free(resize->mrec);
}

static void truncate_badclust_file(ntfs_resize_t *resize)
{
    C_LOG_VERB("Updating $BadClust file ...");

    lookup_data_attr(resize->vol, FILE_BadClus, "$Bad", &resize->ctx);
    /* FIXME: sanity_check_attr(ctx->attr); */
    resize->mref = FILE_BadClus;
    truncate_badclust_bad_attr(resize);

    close_inode_and_context(resize->ctx);
}

static void check_cluster_allocation(ntfs_volume *vol, ntfsck_t *fsck)
{
    memset(fsck, 0, sizeof(ntfsck_t));

    if (setup_lcn_bitmap(&fsck->lcn_bitmap, vol->nr_clusters) != 0) {
        C_LOG_WARNING("Failed to setup allocation bitmap");
    }
    if (build_allocation_bitmap(vol, fsck) != 0) {
        C_LOG_WARNING("");
        return;
    }
    if (fsck->outsider || fsck->multi_ref) {
        C_LOG_VERB("Filesystem check failed!");
        if (fsck->outsider) {
            C_LOG_WARNING("%d clusters are referenced outside  of the volume.", fsck->outsider);
            return;
        }
        if (fsck->multi_ref) {
            C_LOG_WARNING("%d clusters are referenced multiple times.", fsck->multi_ref);
            return;
        }
        C_LOG_WARNING("%s", "corrupt_volume_msg");
        return;
    }
    compare_bitmaps(vol, &fsck->lcn_bitmap);
}

static void delayed_updates(ntfs_resize_t *resize)
{
	struct DELAYED *delayed;
	struct DELAYED *delayed_mft_data;
	int nr_extents;

	if (ntfs_volume_get_free_space(resize->vol)) {
		C_LOG_WARNING("Failed to determine free space");
	    return;
	}

	delayed_mft_data = (struct DELAYED*)NULL;
	if (resize->delayed_runlists && reload_mft(resize)) {
		C_LOG_WARNING("Failed to reload the MFT for delayed updates");
	}

	/*
	 * Important : updates to MFT must come first, so that
	 * the new location of MFT is used for adding needed extents.
	 * Now, there are runlists in the MFT bitmap and MFT data.
	 * Extents to MFT bitmap have to be stored in the new MFT
	 * data, and extents to MFT data have to be recorded in
	 * the MFT bitmap.
	 * So we update MFT data first, and we record the MFT
	 * extents again in the MFT bitmap if they were recorded
	 * in the old location.
	 *
	 * However, if we are operating in "no action" mode, the
	 * MFT records to update are not written to their new location
	 * and the MFT data runlist has to be updated last in order
	 * to have the entries read from their old location.
	 * In this situation the MFT bitmap is never written to
	 * disk, so the same extents are reallocated repeatedly,
	 * which is not what would be done in a real resizing.
	 */

	if (resize->delayed_runlists && (resize->delayed_runlists->mref == FILE_MFT) && (resize->delayed_runlists->type == AT_DATA)) {
		delayed_mft_data = resize->delayed_runlists;
		resize->delayed_runlists = resize->delayed_runlists->next;
	}

	while (resize->delayed_runlists) {
		delayed = resize->delayed_runlists;
		expand_attribute_runlist(resize->vol, delayed);
		if (delayed->mref == FILE_MFT) {
			if (delayed->type == AT_BITMAP) {
				record_mft_in_bitmap(resize);
			}
			if (delayed->type == AT_DATA) {
				resize->mirr_from = MIRR_MFT;
			}
		}
		resize->delayed_runlists = resize->delayed_runlists->next;
		if (delayed->attr_name) {
			free(delayed->attr_name);
		}
		free(delayed->head_rl);
		free(delayed);
	}
	if (false && delayed_mft_data) {
		/* in "no action" mode, check updating the MFT runlist now */
		expand_attribute_runlist(resize->vol, delayed_mft_data);
		resize->mirr_from = MIRR_MFT;
		if (delayed_mft_data->attr_name) {
			free(delayed_mft_data->attr_name);
		}
		free(delayed_mft_data->head_rl);
		free(delayed_mft_data);
	}
	/* Beware of MFT fragmentation when the target size is too small */
	nr_extents = resize->vol->mft_ni->nr_extents;
	if (nr_extents > 2) {
		C_LOG_WARNING("WARNING: The MFT is now severely fragmented (%d extents)", nr_extents);
	}
}

static void update_bootsector(ntfs_resize_t *r)
{
	NTFS_BOOT_SECTOR *bs;
	ntfs_volume *vol = r->vol;
	s64  bs_size = vol->sector_size;

	C_LOG_VERB("Updating Boot record ...");

	bs = (NTFS_BOOT_SECTOR*)ntfs_malloc(vol->sector_size);
	if (!bs) {
		C_LOG_WARNING("malloc");
	    return;
	}

	if (vol->dev->d_ops->seek(vol->dev, 0, SEEK_SET) == (off_t)-1) {
		C_LOG_WARNING("lseek");
	    return;
	}

	if (vol->dev->d_ops->read(vol->dev, bs, bs_size) == -1) {
		C_LOG_WARNING("read() error");
	    return;
	}

	if (bs->bpb.sectors_per_cluster > 128) {
		bs->number_of_sectors = cpu_to_sle64(r->new_volume_size << (256 - bs->bpb.sectors_per_cluster));
	}
	else {
		bs->number_of_sectors = cpu_to_sle64(r->new_volume_size * bs->bpb.sectors_per_cluster);
	}

	if (r->mftmir_old || (r->mirr_from == MIRR_MFT)) {
		r->progress.flags |= NTFS_PROGBAR_SUPPRESS;
		/* Be sure the MFTMirr holds the updated MFT runlist */
		switch (r->mirr_from) {
		case MIRR_MFT :
			/* The late updates of MFT have not been synced */
			ntfs_inode_sync(vol->mft_ni);
			copy_clusters(r, r->mftmir_rl.lcn, vol->mft_na->rl->lcn, r->mftmir_rl.length);
			break;
		case MIRR_NEWMFT :
			copy_clusters(r, r->mftmir_rl.lcn, r->new_mft_start->lcn, r->mftmir_rl.length);
			break;
		default :
			copy_clusters(r, r->mftmir_rl.lcn, r->mftmir_old, r->mftmir_rl.length);
			break;
		}
		if (r->mftmir_old) {
			bs->mftmirr_lcn = cpu_to_sle64(r->mftmir_rl.lcn);
		}
		r->progress.flags &= ~NTFS_PROGBAR_SUPPRESS;
	}
		/* Set the start of the relocated MFT */
	if (r->new_mft_start) {
		bs->mft_lcn = cpu_to_sle64(r->new_mft_start->lcn);
	    /* no more need for the new MFT start */
		free(r->new_mft_start);
		r->new_mft_start = (runlist_element*)NULL;
	}

	if (vol->dev->d_ops->seek(vol->dev, 0, SEEK_SET) == (off_t)-1) {
		C_LOG_WARNING("lseek");
	    return;
	}

    if (vol->dev->d_ops->write(vol->dev, bs, bs_size) == -1) {
        C_LOG_WARNING("write() error");
        return;
    }
	/*
	 * Set the backup boot sector, if the target size is
	 * either not defined or is defined with no multiplier
	 * suffix and is a multiple of the sector size.
	 * With these conditions we can be confident enough that
	 * the partition size is already defined or it will be
	 * later defined with the same exact value.
	 */
#if 0
	if (opt.ro_flag && opt.reliable_size && !(opt.bytes % vol->sector_size)) {
		if (vol->dev->d_ops->seek(vol->dev, opt.bytes
				- vol->sector_size, SEEK_SET) == (off_t)-1)
			perr_exit("lseek");
		if (vol->dev->d_ops->write(vol->dev, bs, bs_size) == -1)
			perr_exit("write() error");
	}
#endif
    C_LOG_WARNING("");

	free(bs);
}

static void lookup_data_attr(ntfs_volume *vol, MFT_REF mref, const char *aname, ntfs_attr_search_ctx **ctx)
{
    ntfs_inode *ni;
    ntfschar *ustr;
    int len = 0;

    if (!(ni = ntfs_inode_open(vol, mref))) {
        C_LOG_WARNING("fs_open_inode");
        return;
    }

    if (!(*ctx = attr_get_search_ctx(ni, NULL))) {
        C_LOG_WARNING("");
        return;
    }

    if ((ustr = ntfs_str2ucs(aname, &len)) == NULL) {
        C_LOG_WARNING("Couldn't convert '%s' to Unicode", aname);
        return;
    }

    if (ntfs_attr_lookup(AT_DATA, ustr, len, CASE_SENSITIVE, 0, NULL, 0, *ctx)) {
        C_LOG_WARNING("fs_lookup_attr");
        return;
    }

    ntfs_ucsfree(ustr);
}

static void truncate_bitmap_file(ntfs_resize_t *resize)
{
    ntfs_volume *vol = resize->vol;

    printf("Updating $Bitmap file ...\n");

    lookup_data_attr(resize->vol, FILE_Bitmap, NULL, &resize->ctx);
    resize->mref = FILE_Bitmap;
    truncate_bitmap_data_attr(resize);

    if (resize->new_mft_start) {
        s64 pos;

        /* write the MFT record at its new location */
        pos = (resize->new_mft_start->lcn << vol->cluster_size_bits)
            + (FILE_Bitmap << vol->mft_record_size_bits);
        if (ntfs_mst_pwrite(vol->dev, pos, 1, vol->mft_record_size, resize->ctx->mrec) != 1) {
            C_LOG_WARNING("Couldn't update $Bitmap at new location");
            return;
        }
    }
    else {
        if (write_mft_record(vol, resize->ctx->ntfs_ino->mft_no, resize->ctx->mrec)) {
            C_LOG_WARNING("Couldn't update $Bitmap");
            return;
        }
    }

    /* If successful, update cache and sync $Bitmap */
    memcpy(vol->lcnbmp_ni->mrec,resize->ctx->mrec,vol->mft_record_size);
    ntfs_inode_mark_dirty(vol->lcnbmp_ni);
    NInoFileNameSetDirty(vol->lcnbmp_ni);
    ntfs_inode_sync(vol->lcnbmp_ni);

#if CLEAN_EXIT
    close_inode_and_context(resize->ctx);
#else
    ntfs_attr_put_search_ctx(resize->ctx);
#endif
}

static ntfs_attr *open_badclust_bad_attr(ntfs_attr_search_ctx *ctx)
{
    ntfs_inode *base_ni;
    ntfs_attr *na;
    static ntfschar Bad[4] = {
        const_cpu_to_le16('$'), const_cpu_to_le16('B'),
        const_cpu_to_le16('a'), const_cpu_to_le16('d')
    } ;

    base_ni = ctx->base_ntfs_ino;
    if (!base_ni)
        base_ni = ctx->ntfs_ino;

    na = ntfs_attr_open(base_ni, AT_DATA, Bad, 4);
    if (!na) {
        C_LOG_WARNING("Could not access the bad sector list\n");
    } else {
        if (ntfs_attr_map_whole_runlist(na) || !na->rl) {
            C_LOG_WARNING("Could not decode the bad sector list\n");
            ntfs_attr_close(na);
            ntfs_inode_close(base_ni);
            na = (ntfs_attr*)NULL;
        }
    }
    return (na);
}

static s64 rounded_up_division(s64 numer, s64 denom)
{
    return (numer + (denom - 1)) / denom;
}

static void resize_constraints_by_attributes(ntfs_resize_t *resize)
{
    if (!(resize->ctx = attr_get_search_ctx(resize->ni, NULL)))
        exit(1);

    while (!ntfs_attrs_walk(resize->ctx)) {
        if (resize->ctx->attr->type == AT_END) {
            break;
        }
        build_resize_constraints(resize);
    }

    ntfs_attr_put_search_ctx(resize->ctx);
}

static int inode_close(ntfs_inode *ni)
{
    if (ntfs_inode_close(ni)) {
        C_LOG_WARNING("ntfs_inode_close for inode %llu", (unsigned long long)ni->mft_no);
        return -1;
    }
    return 0;
}

static void print_num_of_relocations(ntfs_resize_t *resize)
{
    s64 relocations = resize->relocations * resize->vol->cluster_size;

    C_LOG_VERB("Needed relocations : %lld (%lld MB)", (long long)resize->relocations, (long long) rounded_up_division(relocations, NTFS_MBYTE));
}

static ntfs_attr_search_ctx *attr_get_search_ctx(ntfs_inode *ni, MFT_RECORD *mrec)
{
    ntfs_attr_search_ctx *ret;

    if ((ret = ntfs_attr_get_search_ctx(ni, mrec)) == NULL) {
        C_LOG_WARNING("fs_attr_get_search_ctx");
    }

    return ret;
}

static runlist *alloc_cluster(struct bitmap *bm, s64 items, s64 nr_vol_clusters, int hint)
{
    runlist_element rle;
    runlist *rl = NULL;
    int rl_size, runs = 0;
    s64 vcn = 0;

    if (items <= 0) {
        errno = EINVAL;
        return NULL;
    }

    while (items > 0) {

        if (runs)
            hint = 0;
        rle.length = items;
        if (find_free_cluster(bm, &rle, nr_vol_clusters, hint) == -1)
            return NULL;

        rl_size = (runs + 2) * sizeof(runlist_element);
        if (!(rl = (runlist *)realloc(rl, rl_size))) {
            return NULL;
        }

        rl_set(rl + runs, vcn, rle.lcn, rle.length);

        vcn += rle.length;
        items -= rle.length;
        runs++;
    }

    rl_set(rl + runs, vcn, -1LL, 0LL);

    if (runs > 1) {
        ntfs_log_verbose("Multi-run allocation:    \n");
        dump_runlist(rl);
    }
    return rl;
}

static void relocate_inode(ntfs_resize_t *resize, MFT_REF mref, int do_mftdata)
{
    ntfs_volume *vol = resize->vol;

    if (ntfs_file_record_read(vol, mref, &resize->mrec, NULL)) {
        /* FIXME: continue only if it make sense, e.g.
           MFT record not in use based on $MFT bitmap */
        if (errno == EIO || errno == ENOENT) {
            return;
        }
        C_LOG_WARNING("ntfs_file_record_record");
        return;
    }

    if (!(resize->mrec->flags & MFT_RECORD_IN_USE))
        return;

    resize->mref = mref;
    resize->dirty_inode = DIRTY_NONE;

    relocate_attributes(resize, do_mftdata);

    /* relocate MFT during second step, even if not dirty */
    if ((mref == FILE_MFT) && do_mftdata && resize->new_mft_start) {
        s64 pos;

        /* write the MFT own record at its new location */
        pos = (resize->new_mft_start->lcn << vol->cluster_size_bits) + (FILE_MFT << vol->mft_record_size_bits);
        if (ntfs_mst_pwrite(vol->dev, pos, 1, vol->mft_record_size, resize->mrec) != 1) {
            C_LOG_WARNING("Couldn't update MFT own record");
        }
    }
    else {
        if ((resize->dirty_inode == DIRTY_INODE) && write_mft_record(vol, mref, resize->mrec)) {
            C_LOG_WARNING("Couldn't update record %llu", (unsigned long long)mref);
            return;
       }
    }
}

static void truncate_badclust_bad_attr(ntfs_resize_t *resize)
{
    ntfs_inode *base_ni;
    ntfs_attr *na;
    ntfs_attr_search_ctx *ctx;
    s64 nr_clusters = resize->new_volume_size;
    ntfs_volume *vol = resize->vol;

    na = open_badclust_bad_attr(resize->ctx);
    if (!na) {
        C_LOG_WARNING("Could not access the bad sector list");
        return;
    }
    base_ni = na->ni;
    if (ntfs_attr_truncate(na,nr_clusters << vol->cluster_size_bits)) {
        C_LOG_WARNING("Could not adjust the bad sector list");
        return;
    }
    /* Clear the sparse flags, even if there are bad clusters */
    na->ni->flags &= ~FILE_ATTR_SPARSE_FILE;
    na->data_flags &= ~ATTR_IS_SPARSE;
    ctx = resize->ctx;
    ctx->attr->data_size = cpu_to_sle64(na->data_size);
    ctx->attr->initialized_size = cpu_to_sle64(na->initialized_size);
    ctx->attr->flags = na->data_flags;
    ctx->attr->compression_unit = 0;
    ntfs_inode_mark_dirty(ctx->ntfs_ino);
    NInoFileNameSetDirty(na->ni);
    NInoFileNameSetDirty(na->ni);

    ntfs_attr_close(na);
    ntfs_inode_mark_dirty(base_ni);
}

static void close_inode_and_context(ntfs_attr_search_ctx *ctx)
{
    ntfs_inode *ni;

    ni = ctx->base_ntfs_ino;
    if (!ni) {
        ni = ctx->ntfs_ino;
    }
    ntfs_attr_put_search_ctx(ctx);
    if (ni) {
        ntfs_inode_close(ni);
    }
}

static int setup_lcn_bitmap(struct bitmap *bm, s64 nr_clusters)
{
    /* Determine lcn bitmap byte size and allocate it. */
    bm->size = rounded_up_division(nr_clusters, 8);

    bm->bm = ntfs_calloc(bm->size);
    if (!bm->bm) {
        return -1;
    }

    bitmap_file_data_fixup(nr_clusters, bm);
    return 0;
}

static int build_allocation_bitmap(ntfs_volume *vol, ntfsck_t *fsck)
{
    s64 nr_mft_records, inode = 0;
    ntfs_inode *ni;
    struct progress_bar progress;
    int pb_flags = 0;	/* progress bar flags */

    /* WARNING: don't modify the text, external tools grep for it */
    if (fsck->flags & NTFSCK_PROGBAR)
        pb_flags |= NTFS_PROGBAR;

    nr_mft_records = vol->mft_na->initialized_size >> vol->mft_record_size_bits;
    progress_init(&progress, inode, nr_mft_records - 1, pb_flags);

    for (; inode < nr_mft_records; inode++) {
        if ((ni = ntfs_inode_open(vol, (MFT_REF)inode)) == NULL) {
            /* FIXME: continue only if it make sense, e.g.
               MFT record not in use based on $MFT bitmap */
            if (errno == EIO || errno == ENOENT)
                continue;
            C_LOG_WARNING("Reading inode %lld failed", (long long)inode);
            return -1;
        }

        if (ni->mrec->base_mft_record)
            goto close_inode;

        fsck->ni = ni;
        if (walk_attributes(vol, fsck) != 0) {
            inode_close(ni);
            return -1;
        }
close_inode:
        if (inode_close(ni) != 0) {
            return -1;
        }
    }
    return 0;
}

static void compare_bitmaps(ntfs_volume *vol, struct bitmap *a)
{
    s64 i, pos, count;
    int mismatch = 0;
    int backup_boot = 0;
    u8 bm[NTFS_BUF_SIZE];

    pos = 0;
    while (1) {
        count = ntfs_attr_pread(vol->lcnbmp_na, pos, NTFS_BUF_SIZE, bm);
        if (count == -1) {
            C_LOG_WARNING("Couldn't get $Bitmap $DATA");
        }

        if (count == 0) {
            if (a->size > pos) {
                C_LOG_WARNING("$Bitmap size is smaller than expected (%lld != %lld)\n", (long long)a->size, (long long)pos);
            }
            break;
        }

        for (i = 0; i < count; i++, pos++) {
            s64 cl;  /* current cluster */

            if (a->size <= pos) {
                goto done;
            }

            if (a->bm[pos] == bm[i]) {
                continue;
            }

            for (cl = pos * 8; cl < (pos + 1) * 8; cl++) {
                char bit;

                bit = ntfs_bit_get(a->bm, cl);
                if (bit == ntfs_bit_get(bm, i * 8 + cl % 8)) {
                    continue;
                }

                if (!mismatch && !bit && !backup_boot && cl == vol->nr_clusters / 2) {
                    /* FIXME: call also boot sector check */
                    backup_boot = 1;
                    C_LOG_VERB("Found backup boot sector in the middle of the volume.\n");
                    continue;
                }

                if (++mismatch > 10) {
                    continue;
                }
            }
        }
    }
done:
    if (mismatch) {
        C_LOG_VERB("Filesystem check failed! Totally %d cluster accounting mismatches.", mismatch);
        return;
    }
}

static int reload_mft(ntfs_resize_t *resize)
{
    ntfs_inode *ni;
    ntfs_attr *na;
    int r;
    int xi;

    r = 0;
    /* get the base inode */
    ni = resize->vol->mft_ni;
    if (!ntfs_file_record_read(resize->vol, FILE_MFT, &ni->mrec, NULL)) {
        for (xi=0; !r && xi<resize->vol->mft_ni->nr_extents; xi++) {
            r = ntfs_file_record_read(resize->vol,
                    ni->extent_nis[xi]->mft_no,
                    &ni->extent_nis[xi]->mrec, NULL);
        }

        if (!r) {
            /* reopen the MFT bitmap, and swap vol->mftbmp_na */
            na = ntfs_attr_open(resize->vol->mft_ni,
                        AT_BITMAP, NULL, 0);
            if (na && !ntfs_attr_map_whole_runlist(na)) {
                ntfs_attr_close(resize->vol->mftbmp_na);
                resize->vol->mftbmp_na = na;
            } else
                r = -1;
        }

        if (!r) {
            /* reopen the MFT data, and swap vol->mft_na */
            na = ntfs_attr_open(resize->vol->mft_ni,
                        AT_DATA, NULL, 0);
            if (na && !ntfs_attr_map_whole_runlist(na)) {
                ntfs_attr_close(resize->vol->mft_na);
                resize->vol->mft_na = na;
            } else
                r = -1;
        }
    } else
        r = -1;
    return (r);
}

static int record_mft_in_bitmap(ntfs_resize_t *resize)
{
    ntfs_inode *ni;
    int r;
    int xi;

    r = 0;
    /* get the base inode */
    ni = resize->vol->mft_ni;
    for (xi=0; !r && xi<resize->vol->mft_ni->nr_extents; xi++) {
        r = ntfs_bitmap_set_run(resize->vol->mftbmp_na,
                    ni->extent_nis[xi]->mft_no, 1);
    }
    return (r);
}

static void copy_clusters(ntfs_resize_t *resize, s64 dest, s64 src, s64 len)
{
    s64 i;
    char *buff;
    ntfs_volume *vol = resize->vol;

    buff = (char*)ntfs_malloc(vol->cluster_size);
    if (!buff) {
        C_LOG_WARNING("malloc");
        return;
    }

    for (i = 0; i < len; i++) {
        lseek_to_cluster(vol, src + i);
        if (read_all(vol->dev, buff, vol->cluster_size) == -1) {
            C_LOG_WARNING("Failed to read from the disk");
            if (errno == EIO) {
                C_LOG_WARNING("%s", "bad_sectors_warning_msg");
            }
            return;
        }

        lseek_to_cluster(vol, dest + i);

        if (write_all(vol->dev, buff, vol->cluster_size) == -1) {
            C_LOG_WARNING("Failed to write to the disk");
            if (errno == EIO) {
                C_LOG_WARNING("%s", "bad_sectors_warning_msg");
            }
            return;
        }

        resize->relocations++;
        progress_update(&resize->progress, resize->relocations);
    }
    free(buff);
}

static void truncate_bitmap_data_attr(ntfs_resize_t *resize)
{
    ATTR_RECORD *a;
    runlist *rl;
    ntfs_attr *lcnbmp_na;
    s64 bm_bsize, size;
    s64 nr_bm_clusters;
    int truncated;
    ntfs_volume *vol = resize->vol;

    a = resize->ctx->attr;
    if (!a->non_resident) {
        C_LOG_WARNING("Resident attribute in $Bitmap isn't supported!");
        return;
    }

    bm_bsize = nr_clusters_to_bitmap_byte_size(resize->new_volume_size);
    nr_bm_clusters = rounded_up_division(bm_bsize, vol->cluster_size);

    if (resize->shrink) {
        realloc_bitmap_data_attr(resize, &rl, nr_bm_clusters);
        realloc_lcn_bitmap(resize, bm_bsize);
    } else {
        realloc_lcn_bitmap(resize, bm_bsize);
        realloc_bitmap_data_attr(resize, &rl, nr_bm_clusters);
    }
    /*
     * Delayed relocations may require cluster allocations
     * through the library, to hold added attribute lists,
     * be sure they will be within the new limits.
     */
    lcnbmp_na = resize->vol->lcnbmp_na;
    lcnbmp_na->data_size = bm_bsize;
    lcnbmp_na->initialized_size = bm_bsize;
    lcnbmp_na->allocated_size = nr_bm_clusters << vol->cluster_size_bits;
    vol->lcnbmp_ni->data_size = bm_bsize;
    vol->lcnbmp_ni->allocated_size = lcnbmp_na->allocated_size;
    a->highest_vcn = cpu_to_sle64(nr_bm_clusters - 1LL);
    a->allocated_size = cpu_to_sle64(nr_bm_clusters * vol->cluster_size);
    a->data_size = cpu_to_sle64(bm_bsize);
    a->initialized_size = cpu_to_sle64(bm_bsize);

    truncated = !replace_attribute_runlist(resize, rl);

    /*
     * FIXME: update allocated/data sizes and timestamps in $FILE_NAME
     * attribute too, for now chkdsk will do this for us.
     */

    size = ntfs_rl_pwrite(vol, rl, 0, 0, bm_bsize, resize->lcn_bitmap.bm);
    if (bm_bsize != size) {
        if (size == -1) {
            C_LOG_WARNING("Couldn't write $Bitmap");
            return;
        }
        C_LOG_WARNING("Couldn't write full $Bitmap file (%lld from %lld)\n", (long long)size, (long long)bm_bsize);
        return;
    }

    if (truncated) {
        /* switch to the new bitmap runlist */
        free(lcnbmp_na->rl);
        lcnbmp_na->rl = rl;
    }
}

static int write_mft_record(ntfs_volume *v, const MFT_REF mref, MFT_RECORD *buf)
{
    if (ntfs_mft_record_write(v, mref, buf)) {
        C_LOG_WARNING("fs_mft_record_write");
    }

    return 0;
}

static void build_resize_constraints(ntfs_resize_t *resize)
{
    s64 i;
    runlist *rl;

    if (!resize->ctx->attr->non_resident) {
        return;
    }

    if (!(rl = ntfs_mapping_pairs_decompress(resize->vol, resize->ctx->attr, NULL))) {
        C_LOG_WARNING("ntfs_decompress_mapping_pairs");
        return;
    }

    for (i = 0; rl[i].length; i++) {
        /* CHECKME: LCN_RL_NOT_MAPPED check isn't needed */
        if (rl[i].lcn == LCN_HOLE || rl[i].lcn == LCN_RL_NOT_MAPPED) {
            continue;
        }

        collect_resize_constraints(resize, rl + i);
        if (resize->shrink) {
            collect_relocation_info(resize, rl + i);
        }
    }
    free(rl);
}

static int find_free_cluster(struct bitmap *bm, runlist_element *rle, s64 nr_vol_clusters, int hint)
{
    /* FIXME: get rid of this 'static' variable */
    static s64 pos = 0;
    s64 i, items = rle->length;
    s64 free_zone = 0;

    if (pos >= nr_vol_clusters)
        pos = 0;
    if (!max_free_cluster_range)
        max_free_cluster_range = nr_vol_clusters;
    rle->lcn = rle->length = 0;
    if (hint)
        pos = nr_vol_clusters / 2;
    i = pos;

    do {
        if (!ntfs_bit_get(bm->bm, i)) {
            if (++free_zone == items) {
                set_max_free_zone(free_zone, i + 1, rle);
                break;
            }
        } else {
            set_max_free_zone(free_zone, i, rle);
            free_zone = 0;
        }
        if (++i == nr_vol_clusters) {
            set_max_free_zone(free_zone, i, rle);
            i = free_zone = 0;
        }
        if (rle->length == max_free_cluster_range)
            break;
    } while (i != pos);

    if (i)
        set_max_free_zone(free_zone, i, rle);

    if (!rle->lcn) {
        errno = ENOSPC;
        return -1;
    }
    if (rle->length < items && rle->length < max_free_cluster_range) {
        max_free_cluster_range = rle->length;
        C_LOG_VERB("Max free range: %7lld     \n", (long long)max_free_cluster_range);
    }
    pos = rle->lcn + items;
    if (pos == nr_vol_clusters)
        pos = 0;

    set_bitmap_range(bm, rle->lcn, rle->length, 1);
    return 0;
}

static void rl_set(runlist *rl, VCN vcn, LCN lcn, s64 len)
{
    rl->vcn = vcn;
    rl->lcn = lcn;
    rl->length = len;
}

static int rl_items(runlist *rl)
{
    int i = 0;

    while (rl[i++].length)
        ;

    return i;
}

static void dump_run(runlist_element *r)
{
    C_LOG_VERB(" %8lld  %8lld (0x%08llx)  %lld\n", (long long)r->vcn,
             (long long)r->lcn, (long long)r->lcn,
             (long long)r->length);
}

static void dump_runlist(runlist *rl)
{
    while (rl->length)
        dump_run(rl++);
}

static s64 nr_clusters_to_bitmap_byte_size(s64 nr_clusters)
{
    s64 bm_bsize;

    bm_bsize = rounded_up_division(nr_clusters, 8);
    bm_bsize = (bm_bsize + 7) & ~7;

    return bm_bsize;
}

static void realloc_lcn_bitmap(ntfs_resize_t *resize, s64 bm_bsize)
{
    u8 *tmp;

    if (!(tmp = realloc(resize->lcn_bitmap.bm, bm_bsize))) {
        C_LOG_ERROR("realloc");
    }

    resize->lcn_bitmap.bm = tmp;
    resize->lcn_bitmap.size = bm_bsize;
    bitmap_file_data_fixup(resize->new_volume_size, &resize->lcn_bitmap);
}

static void set_max_free_zone(s64 length, s64 end, runlist_element *rle)
{
    if (length > rle->length) {
        rle->lcn = end - length;
        rle->length = length;
    }
}

static void realloc_bitmap_data_attr(ntfs_resize_t *resize, runlist **rl, s64 nr_bm_clusters)
{
    s64 i;
    ntfs_volume *vol = resize->vol;
    ATTR_RECORD *a = resize->ctx->attr;
    s64 new_size = resize->new_volume_size;
    struct bitmap *bm = &resize->lcn_bitmap;

    if (!(*rl = ntfs_mapping_pairs_decompress(vol, a, NULL))) {
        C_LOG_WARNING("fs_mapping_pairs_decompress");
        return;
    }

    release_bitmap_clusters(bm, *rl);
    free(*rl);

    for (i = vol->nr_clusters; i < new_size; i++)
        ntfs_bit_set(bm->bm, i, 0);

    if (!(*rl = alloc_cluster(bm, nr_bm_clusters, new_size, 0))) {
        C_LOG_WARNING("Couldn't allocate $Bitmap clusters");
        return;
    }
}

static void bitmap_file_data_fixup(s64 cluster, struct bitmap *bm)
{
    for (; cluster < bm->size << 3; cluster++)
        ntfs_bit_set(bm->bm, (u64)cluster, 1);
}

static void release_bitmap_clusters(struct bitmap *bm, runlist *rl)
{
    max_free_cluster_range = 0;
    set_bitmap_clusters(bm, rl, 0);
}

static void set_bitmap_clusters(struct bitmap *bm, runlist *rl, u8 bit)
{
    for (; rl->length; rl++)
        set_bitmap_range(bm, rl->lcn, rl->length, bit);
}

static void set_bitmap_range(struct bitmap *bm, s64 pos, s64 length, u8 bit)
{
    while (length--)
        ntfs_bit_set(bm->bm, pos++, bit);
}

static void collect_resize_constraints(ntfs_resize_t *resize, runlist *rl)
{
    s64 inode, last_lcn;
    ATTR_FLAGS flags;
    ATTR_TYPES atype;
    struct llcn_t *llcn = NULL;
    int ret, supported = 0;

    last_lcn = rl->lcn + (rl->length - 1);

    inode = resize->ni->mft_no;
    flags = resize->ctx->attr->flags;
    atype = resize->ctx->attr->type;

    if ((ret = ntfs_inode_badclus_bad(inode, resize->ctx->attr)) != 0) {
        if (ret == -1) {
            C_LOG_WARNING("Bad sector list check failed");
            return;
        }
        return;
    }

    if (inode == FILE_Bitmap) {
        llcn = &resize->last_lcn;
        if (atype == AT_DATA && NInoAttrList(resize->ni)) {
            C_LOG_WARNING("Highly fragmented $Bitmap isn't supported yet.");
            return;
        }

        supported = 1;

    } else if (NInoAttrList(resize->ni)) {
        llcn = &resize->last_multi_mft;

        if (inode != FILE_MFTMirr)
            supported = 1;

    } else if (flags & ATTR_IS_SPARSE) {
        llcn = &resize->last_sparse;
        supported = 1;

    } else if (flags & ATTR_IS_COMPRESSED) {
        llcn = &resize->last_compressed;
        supported = 1;

    } else if (inode == FILE_MFTMirr) {
        llcn = &resize->last_mftmir;
        supported = 1;

        /* Fragmented $MFTMirr DATA attribute isn't supported yet */
        if (atype == AT_DATA)
            if (rl[1].length != 0 || rl->vcn)
                supported = 0;
    } else {
        llcn = &resize->last_lcn;
        supported = 1;
    }

    if (llcn->lcn < last_lcn) {
        llcn->lcn = last_lcn;
        llcn->inode = inode;
    }

    if (supported)
        return;

    if (resize->last_unsupp < last_lcn)
        resize->last_unsupp = last_lcn;
}

static void relocate_attributes(ntfs_resize_t *resize, int do_mftdata)
{
    int ret;
    leMFT_REF lemref;
    MFT_REF base_mref;

    if (!(resize->ctx = attr_get_search_ctx(NULL, resize->mrec)))
        exit(1);

    lemref = resize->mrec->base_mft_record;
    if (lemref)
        base_mref = MREF(le64_to_cpu(lemref));
    else
        base_mref = resize->mref;
    while (!ntfs_attrs_walk(resize->ctx)) {
        if (resize->ctx->attr->type == AT_END)
            break;

        if (handle_mftdata(resize, do_mftdata) == 0)
            continue;

        ret = ntfs_inode_badclus_bad(resize->mref, resize->ctx->attr);
        if (ret == -1) {
            C_LOG_WARNING("Bad sector list check failed");
            return;
        }
        else if (ret == 1)
            continue;

        if (resize->mref == FILE_Bitmap &&
            resize->ctx->attr->type == AT_DATA)
            continue;

        /* Do not relocate bad clusters */
        if ((base_mref == FILE_BadClus)
            && (resize->ctx->attr->type == AT_DATA))
            continue;

        relocate_attribute(resize);
    }

    ntfs_attr_put_search_ctx(resize->ctx);
}

static int walk_attributes(ntfs_volume *vol, ntfsck_t *fsck)
{
    if (!(fsck->ctx = attr_get_search_ctx(fsck->ni, NULL)))
        return -1;

    while (!ntfs_attrs_walk(fsck->ctx)) {
        if (fsck->ctx->attr->type == AT_END)
            break;
        build_lcn_usage_bitmap(vol, fsck);
    }

    ntfs_attr_put_search_ctx(fsck->ctx);
    return 0;
}

static int replace_attribute_runlist(ntfs_resize_t *resize, runlist *rl)
{
	int mp_size, l;
	int must_delay;
	void *mp;
	runlist *head_rl;
	ntfs_volume *vol;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;

	vol = resize->vol;
	ctx = resize->ctx;
	a = ctx->attr;
	head_rl = rl;
	rl_fixup(&rl);

	if ((mp_size = ntfs_get_size_for_mapping_pairs(vol, rl, 0, INT_MAX)) == -1) {
		C_LOG_WARNING("ntfs_get_size_for_mapping_pairs");
	    return -1;
	}

	if (a->name_length) {
		u16 name_offs = le16_to_cpu(a->name_offset);
		u16 mp_offs = le16_to_cpu(a->mapping_pairs_offset);

		if (name_offs >= mp_offs) {
			C_LOG_WARNING("Attribute name is after mapping pairs! "
				 "Please report!\n");
		    return -1;
		}
	}

	/* CHECKME: don't trust mapping_pairs is always the last item in the
	   attribute, instead check for the real size/space */
	l = (int)le32_to_cpu(a->length) - le16_to_cpu(a->mapping_pairs_offset);
	must_delay = 0;
	if (mp_size > l) {
		s32 remains_size;
		char *next_attr;

		ntfs_log_verbose("Enlarging attribute header ...\n");

		mp_size = (mp_size + 7) & ~7;

		ntfs_log_verbose("Old mp size      : %d\n", l);
		ntfs_log_verbose("New mp size      : %d\n", mp_size);
		ntfs_log_verbose("Bytes in use     : %u\n", (unsigned int)
				 le32_to_cpu(ctx->mrec->bytes_in_use));

		next_attr = (char *)a + le32_to_cpu(a->length);
		l = mp_size - l;

		ntfs_log_verbose("Bytes in use new : %u\n", l + (unsigned int)
				 le32_to_cpu(ctx->mrec->bytes_in_use));
		ntfs_log_verbose("Bytes allocated  : %u\n", (unsigned int)
				 le32_to_cpu(ctx->mrec->bytes_allocated));

		remains_size = le32_to_cpu(ctx->mrec->bytes_in_use);
		remains_size -= (next_attr - (char *)ctx->mrec);

		ntfs_log_verbose("increase         : %d\n", l);
		ntfs_log_verbose("shift            : %lld\n",
				 (long long)remains_size);
		if (le32_to_cpu(ctx->mrec->bytes_in_use) + l >
				le32_to_cpu(ctx->mrec->bytes_allocated)) {
#ifndef BAN_NEW_TEXT
			ntfs_log_verbose("Queuing expansion for later processing\n");
#endif
			must_delay = 1;
			replace_later(resize,rl,head_rl);
		} else {
			memmove(next_attr + l, next_attr, remains_size);
			ctx->mrec->bytes_in_use = cpu_to_le32(l +
					le32_to_cpu(ctx->mrec->bytes_in_use));
			a->length = cpu_to_le32(le32_to_cpu(a->length) + l);
		}
	}

	if (!must_delay) {
		mp = ntfs_calloc(mp_size);
		if (!mp) {
			C_LOG_WARNING("ntfsc_calloc couldn't get memory");
		    return -1;
		}

		if (ntfs_mapping_pairs_build(vol, (u8*)mp, mp_size, rl, 0, NULL)) {
			C_LOG_WARNING("ntfs_mapping_pairs_build");
		    return -1;
		}

		memmove((u8*)a + le16_to_cpu(a->mapping_pairs_offset), mp, mp_size);

		free(mp);
	}
	return (must_delay);
}

static void collect_relocation_info(ntfs_resize_t *resize, runlist *rl)
{
    s64 lcn, lcn_length, start, len, inode;
    s64 new_vol_size;	/* (last LCN on the volume) + 1 */

    lcn = rl->lcn;
    lcn_length = rl->length;
    inode = resize->ni->mft_no;
    new_vol_size = resize->new_volume_size;

    if (lcn + lcn_length <= new_vol_size)
        return;

    if (inode == FILE_Bitmap && resize->ctx->attr->type == AT_DATA)
        return;

    start = lcn;
    len = lcn_length;

    if (lcn < new_vol_size) {
        start = new_vol_size;
        len = lcn_length - (new_vol_size - lcn);

        if (inode == FILE_MFTMirr) {
            C_LOG_WARNING("$MFTMirr can't be split up yet. Please try "
                   "a different size.\n");
            // print_advise(resize->vol, lcn + lcn_length - 1);
            return;
        }
    }

    resize->relocations += len;

    printf("Relocation needed for inode %8lld attr 0x%x LCN 0x%08llx "
            "length %6lld\n", (long long)inode,
            (unsigned int)le32_to_cpu(resize->ctx->attr->type),
            (unsigned long long)start, (long long)len);
}

static void build_lcn_usage_bitmap(ntfs_volume *vol, ntfsck_t *fsck)
{
    s64 inode;
    ATTR_RECORD *a;
    runlist *rl;
    int i, j;
    struct bitmap *lcn_bitmap = &fsck->lcn_bitmap;

    a = fsck->ctx->attr;
    inode = fsck->ni->mft_no;

    if (!a->non_resident)
        return;

    if (!(rl = ntfs_mapping_pairs_decompress(vol, a, NULL))) {
        int err = errno;
        C_LOG_WARNING("ntfs_decompress_mapping_pairs");
        return;
    }


    for (i = 0; rl[i].length; i++) {
        s64 lcn = rl[i].lcn;
        s64 lcn_length = rl[i].length;

        /* CHECKME: LCN_RL_NOT_MAPPED check isn't needed */
        if (lcn == LCN_HOLE || lcn == LCN_RL_NOT_MAPPED)
            continue;

        /* FIXME: ntfs_mapping_pairs_decompress should return error */
        if (lcn < 0 || lcn_length <= 0) {
            C_LOG_WARNING("Corrupt runlist in inode %lld attr %x LCN "
                 "%llx length %llx\n", (long long)inode,
                 (unsigned int)le32_to_cpu(a->type),
                 (long long)lcn, (long long)lcn_length);
            return;
        }

        for (j = 0; j < lcn_length; j++) {
            u64 k = (u64)lcn + j;

            if (k >= (u64)vol->nr_clusters) {
                long long outsiders = lcn_length - j;

                fsck->outsider += outsiders;

                if (++fsck->show_outsider <= 10 )
                    printf("Outside of the volume reference"
                           " for inode %lld at %lld:%lld\n",
                           (long long)inode, (long long)k,
                           (long long)outsiders);

                break;
            }

            if (ntfs_bit_get_and_set(lcn_bitmap->bm, k, 1)) {
                if (++fsck->multi_ref <= 10 )
                    printf("Cluster %lld is referenced "
                           "multiple times!\n",
                           (long long)k);
                continue;
            }
        }
        fsck->inuse += lcn_length;
    }
    free(rl);
}

static int handle_mftdata(ntfs_resize_t *resize, int do_mftdata)
{
    ATTR_RECORD *attr = resize->ctx->attr;
    VCN highest_vcn, lowest_vcn;

    if (do_mftdata) {

        if (!is_mftdata(resize))
            return 0;

        highest_vcn = sle64_to_cpu(attr->highest_vcn);
        lowest_vcn  = sle64_to_cpu(attr->lowest_vcn);

        if (resize->mft_highest_vcn != highest_vcn)
            return 0;

        if (lowest_vcn == 0)
            resize->mft_highest_vcn = lowest_vcn;
        else
            resize->mft_highest_vcn = lowest_vcn - 1;

    } else if (is_mftdata(resize)) {

        highest_vcn = sle64_to_cpu(attr->highest_vcn);

        if (resize->mft_highest_vcn < highest_vcn)
            resize->mft_highest_vcn = highest_vcn;

        return 0;
    }

    return 1;
}

static int is_mftdata(ntfs_resize_t *resize)
{
    /*
     * We must update the MFT own DATA record at the end of the second
     * step, because the old MFT must be kept available for processing
     * the other files.
     */

    if (resize->ctx->attr->type != AT_DATA)
        return 0;

    if (resize->mref == 0)
        return 1;

    if (MREF_LE(resize->mrec->base_mft_record) == 0 &&
        MSEQNO_LE(resize->mrec->base_mft_record) != 0)
        return 1;

    return 0;
}

static void relocate_attribute(ntfs_resize_t *resize)
{
    ATTR_RECORD *a;
    runlist *rl;
    int i;

    a = resize->ctx->attr;

    if (!a->non_resident)
        return;

    if (!(rl = ntfs_mapping_pairs_decompress(resize->vol, a, NULL))) {
        C_LOG_WARNING("ntfs_decompress_mapping_pairs");
        return;
    }

    for (i = 0; rl[i].length; i++) {
        s64 lcn = rl[i].lcn;
        s64 lcn_length = rl[i].length;

        if (lcn == LCN_HOLE || lcn == LCN_RL_NOT_MAPPED)
            continue;

        /* FIXME: ntfs_mapping_pairs_decompress should return error */
        if (lcn < 0 || lcn_length <= 0) {
            C_LOG_WARNING("Corrupt runlist in MTF %llu attr %x LCN "
                 "%llx length %llx\n",
                 (unsigned long long)resize->mref,
                 (unsigned int)le32_to_cpu(a->type),
                 (long long)lcn, (long long)lcn_length);
            return;
        }

        relocate_run(resize, &rl, i);
    }

    if (resize->dirty_inode == DIRTY_ATTRIB) {
        if (!replace_attribute_runlist(resize, rl))
            free(rl);
        resize->dirty_inode = DIRTY_INODE;
    } else
        free(rl);
}

static void relocate_run(ntfs_resize_t *resize, runlist **rl, int run)
{
    s64 lcn, lcn_length;
    s64 new_vol_size;	/* (last LCN on the volume) + 1 */
    runlist *relocate_rl;	/* relocate runlist to relocate_rl */
    int hint;

    lcn = (*rl + run)->lcn;
    lcn_length = (*rl + run)->length;
    new_vol_size = resize->new_volume_size;

    if (lcn + lcn_length <= new_vol_size)
        return;

    if (lcn < new_vol_size) {
        rl_split_run(rl, run, new_vol_size);
        return;
    }

    hint = (resize->mref == FILE_MFTMirr) ? 1 : 0;
    if ((resize->mref == FILE_MFT)
        && (resize->ctx->attr->type == AT_DATA)
        && !run
        && resize->new_mft_start) {
        relocate_rl = resize->new_mft_start;
        } else
            if (!(relocate_rl = alloc_cluster(&resize->lcn_bitmap, lcn_length, new_vol_size, hint))) {
                C_LOG_WARNING("Cluster allocation failed for %llu:%lld",
                      (unsigned long long)resize->mref,
                      (long long)lcn_length);
                return;
            }

    /* FIXME: check $MFTMirr DATA isn't multi-run (or support it) */
    ntfs_log_verbose("Relocate record %7llu:0x%x:%08lld:0x%08llx:0x%08llx "
             "--> 0x%08llx\n", (unsigned long long)resize->mref,
             (unsigned int)le32_to_cpu(resize->ctx->attr->type),
             (long long)lcn_length,
             (unsigned long long)(*rl + run)->vcn,
             (unsigned long long)lcn,
             (unsigned long long)relocate_rl->lcn);

    relocate_clusters(resize, relocate_rl, lcn);
    rl_insert_at_run(rl, run, relocate_rl);

    /* We don't release old clusters in the bitmap, that area isn't
       used by the allocator and will be truncated later on */

    /* Do not free the relocated MFT start */
    if ((resize->mref != FILE_MFT)
        || (resize->ctx->attr->type != AT_DATA)
        || run
        || !resize->new_mft_start)
        free(relocate_rl);

    resize->dirty_inode = DIRTY_ATTRIB;
}

static void rl_split_run(runlist **rl, int run, s64 pos)
{
    runlist *rl_new, *rle_new, *rle;
    int items, new_size, size_head, size_tail;
    s64 len_head, len_tail;

    items = rl_items(*rl);
    new_size = (items + 1) * sizeof(runlist_element);
    size_head = run * sizeof(runlist_element);
    size_tail = (items - run - 1) * sizeof(runlist_element);

    rl_new = ntfs_malloc(new_size);
    if (!rl_new) {
        C_LOG_WARNING("ntfs_malloc");
        return;
    }

    rle_new = rl_new + run;
    rle = *rl + run;

    memmove(rl_new, *rl, size_head);
    memmove(rle_new + 2, rle + 1, size_tail);

    len_tail = rle->length - (pos - rle->lcn);
    len_head = rle->length - len_tail;

    rl_set(rle_new, rle->vcn, rle->lcn, len_head);
    rl_set(rle_new + 1, rle->vcn + len_head, rle->lcn + len_head, len_tail);

    ntfs_log_verbose("Splitting run at cluster %lld:\n", (long long)pos);
    dump_run(rle); dump_run(rle_new); dump_run(rle_new + 1);

    free(*rl);
    *rl = rl_new;
}

static void rl_insert_at_run(runlist **rl, int run, runlist *ins)
{
    int items, ins_items;
    int new_size, size_tail;
    runlist *rle;
    s64 vcn;

    items  = rl_items(*rl);
    ins_items = rl_items(ins) - 1;
    new_size = ((items - 1) + ins_items) * sizeof(runlist_element);
    size_tail = (items - run - 1) * sizeof(runlist_element);

    if (!(*rl = (runlist *)realloc(*rl, new_size))) {
        C_LOG_WARNING("realloc");
        return;
    }

    rle = *rl + run;

    memmove(rle + ins_items, rle + 1, size_tail);

    for (vcn = rle->vcn; ins->length; rle++, vcn += ins->length, ins++) {
        rl_set(rle, vcn, ins->lcn, ins->length);
        //		dump_run(rle);
    }

    return;

    /* FIXME: fast path if ins_items = 1 */
    //	(*rl + run)->lcn = ins->lcn;
}

static void relocate_clusters(ntfs_resize_t *r, runlist *dest_rl, s64 src_lcn)
{
    /* collect_shrink_constraints() ensured $MFTMir DATA is one run */
    if (r->mref == FILE_MFTMirr && r->ctx->attr->type == AT_DATA) {
        if (!r->mftmir_old) {
            r->mftmir_rl.lcn = dest_rl->lcn;
            r->mftmir_rl.length = dest_rl->length;
            r->mftmir_old = src_lcn;
        } else {
            C_LOG_WARNING("Multi-run $MFTMirr. Please report!\n");
            return;
        }
    }

    for (; dest_rl->length; src_lcn += dest_rl->length, dest_rl++)
        copy_clusters(r, dest_rl->lcn, src_lcn, dest_rl->length);
}

static int ntfs_fuse_init(void)
{
    ctx = ntfs_calloc(sizeof(ntfs_fuse_context_t));
    if (!ctx) {
        return -1;
    }

    *ctx = (ntfs_fuse_context_t) {
        .uid     = getuid(),
        .gid     = getgid(),
#if defined(linux)
        .streams = NF_STREAMS_INTERFACE_XATTR,
#else
        .streams = NF_STREAMS_INTERFACE_NONE,
#endif
        .atime   = ATIME_RELATIVE,
        .silent  = TRUE,
        .recover = TRUE
    };
    return 0;
}

static fuse_fstype get_fuse_fstype(void)
{
    char buf[256];
    fuse_fstype fstype = FSTYPE_NONE;

    FILE *f = fopen("/proc/filesystems", "r");
    if (!f) {
        C_LOG_WARNING("Failed to open /proc/filesystems");
        return FSTYPE_UNKNOWN;
    }

    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "fuseblk\n")) {
            fstype = FSTYPE_FUSEBLK;
            break;
        }
        if (strstr(buf, "fuse\n"))
            fstype = FSTYPE_FUSE;
    }

    fclose(f);

    return fstype;
}

static fuse_fstype load_fuse_module(void)
{
    int i;
    struct stat st;
    pid_t pid;
    const char *cmd = "/sbin/modprobe";
    char *env = (char*)NULL;
    struct timespec req = { 0, 100000000 };   /* 100 msec */
    fuse_fstype fstype;

    if (!stat(cmd, &st) && !geteuid()) {
        pid = fork();
        if (!pid) {
            execle(cmd, cmd, "fuse", (char*)NULL, &env);
            _exit(1);
        }
        else if (pid != -1) {
            waitpid(pid, NULL, 0);
        }
    }

    for (i = 0; i < 10; i++) {
        /*
         * We sleep first because despite the detection of the loaded
         * FUSE kernel module, fuse_mount() can still fail if it's not
         * fully functional/initialized. Note, of course this is still
         * unreliable but usually helps.
         */
        nanosleep(&req, NULL);
        fstype = get_fuse_fstype();
        if (fstype != FSTYPE_NONE) {
            break;
        }
    }
    return fstype;
}

static void create_dev_fuse(void)
{
    mknod_dev_fuse("/dev/fuse");

#ifdef __UCLIBC__
    {
        struct stat st;
        /* The fuse device is under /dev/misc using devfs. */
        if (stat("/dev/misc", &st) && (errno == ENOENT)) {
            mode_t mask = umask(0);
            mkdir("/dev/misc", 0775);
            umask(mask);
        }
        mknod_dev_fuse("/dev/misc/fuse");
    }
#endif
}

static int drop_privs(void)
{
    if (!getegid()) {

        gid_t new_gid = getgid();

        if (setresgid(-1, new_gid, getegid()) < 0) {
            perror("priv drop: setresgid failed");
            return -1;
        }
        if (getegid() != new_gid){
            perror("dropping group privilege failed");
            return -1;
        }
    }

    if (!geteuid()) {

        uid_t new_uid = getuid();

        if (setresuid(-1, new_uid, geteuid()) < 0) {
            perror("priv drop: setresuid failed");
            return -1;
        }
        if (geteuid() != new_uid){
            perror("dropping user privilege failed");
            return -1;
        }
    }

    return 0;
}

static int ntfs_open(const char *device)
{
    unsigned long flags = 0;

    if (!ctx->blkdev) {
        flags |= NTFS_MNT_EXCLUSIVE;
    }

    if (ctx->ro) {
        flags |= NTFS_MNT_RDONLY;
    }
    else {
        if (!ctx->hiberfile) {
            flags |= NTFS_MNT_MAY_RDONLY;
        }
    }

    if (ctx->recover) {
        flags |= NTFS_MNT_RECOVER;
    }

    if (ctx->hiberfile) {
        flags |= NTFS_MNT_IGNORE_HIBERFILE;
    }

    ctx->vol = ntfs_mount(device, flags);
    if (!ctx->vol) {
        C_LOG_WARNING("Failed to mount '%s'", device);
        goto err_out;
    }

    if (ctx->sync && ctx->vol->dev) {
        NDevSetSync(ctx->vol->dev);
    }

    if (ctx->compression) {
        NVolSetCompression(ctx->vol);
    }
    else {
        NVolClearCompression(ctx->vol);
    }

#ifdef HAVE_SETXATTR
    /* archivers must see hidden files */
    if (ctx->efs_raw) {
        ctx->hide_hid_files = FALSE;
    }
#endif

    if (ntfs_set_shown_files(ctx->vol, ctx->show_sys_files, !ctx->hide_hid_files, ctx->hide_dot_files)) {
        goto err_out;
    }

    if (ntfs_volume_get_free_space(ctx->vol)) {
        C_LOG_WARNING("Failed to read NTFS $Bitmap");
        goto err_out;
    }

    ctx->vol->free_mft_records = ntfs_get_nr_free_mft_records(ctx->vol);
    if (ctx->vol->free_mft_records < 0) {
        C_LOG_WARNING("Failed to calculate free MFT records");
        goto err_out;
    }

    if (ctx->hiberfile && ntfs_volume_check_hiberfile(ctx->vol, 0)) {
        if (errno != EPERM) {
            goto err_out;
        }
        if (ntfs_fuse_rm("/hiberfil.sys")) {
            goto err_out;
        }
    }

    errno = 0;
    goto out;

err_out:
    if (!errno) {
        errno = EIO;
    }

out :
    return ntfs_volume_error(errno);
}

static int ntfs_strinsert(char **dest, const char *append)
{
    char *p, *q;
    size_t size_append, size_dest = 0;

    if (!dest) {
        return -1;
    }

    if (!append) {
        return 0;
    }

    size_append = strlen(append);
    if (*dest) {
        size_dest = strlen(*dest);
    }

    if (strappend_is_large(size_dest) || strappend_is_large(size_append)) {
        errno = EOVERFLOW;
        C_LOG_WARNING("Too large input buffer");
        return -1;
    }

    p = (char*)malloc(size_dest + size_append + 1);
    if (!p) {
        C_LOG_WARNING("Memory reallocation failed");
        return -1;
    }

    strcpy(p, *dest);
    q = strstr(p, ",fsname=");
    if (q) {
        strcpy(q, append);
        q = strstr(*dest, ",fsname=");
        if (q) {
            strcat(p, q);
        }
        free(*dest);
        *dest = p;
    }
    else {
        free(*dest);
        *dest = p;
        strcpy(*dest + size_dest, append);
    }

    return 0;
}

static int set_fuseblk_options(char **parsed_options)
{
    char options[64];
    long pagesize;
    u32 blksize = ctx->vol->cluster_size;

    pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize < 1) {
        pagesize = 4096;
    }

    if (blksize > (u32)pagesize) {
        blksize = pagesize;
    }

    snprintf(options, sizeof(options), ",blkdev,blksize=%u", blksize);
    if (ntfs_strappend(parsed_options, options)) {
        return -1;
    }

    return 0;
}

#ifndef DISABLE_PLUGINS
static void register_internal_reparse_plugins(void)
{
    static const plugin_operations_t ops = {
        .getattr = junction_getattr,
        .readlink = junction_readlink,
    } ;
    static const plugin_operations_t wsl_ops = {
        .getattr = wsl_getattr,
    } ;

    register_reparse_plugin(ctx, IO_REPARSE_TAG_MOUNT_POINT, &ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_SYMLINK, &ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_LX_SYMLINK, &ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_LX_SYMLINK, &ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_AF_UNIX, &wsl_ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_LX_FIFO, &wsl_ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_LX_CHR, &wsl_ops, (void*)NULL);
    register_reparse_plugin(ctx, IO_REPARSE_TAG_LX_BLK, &wsl_ops, (void*)NULL);
}
#endif /* DISABLE_PLUGINS */

static int junction_getattr(ntfs_inode *ni, const REPARSE_POINT *reparse __attribute__((unused)), struct stat *stbuf)
{
    char *target;
    int res;

    errno = 0;
    target = ntfs_make_symlink(ni, ctx->abs_mnt_point);
    /*
     * If the reparse point is not a valid
     * directory junction, and there is no error
     * we still display as a symlink
     */
    if (target || (errno == EOPNOTSUPP)) {
        if (target)
            stbuf->st_size = strlen(target);
        else
            stbuf->st_size = ntfs_bad_reparse_lth;
        stbuf->st_blocks = (ni->allocated_size + 511) >> 9;
        stbuf->st_mode = S_IFLNK;
        free(target);
        res = 0;
    } else {
        res = -errno;
    }
    return (res);
}


#ifndef DISABLE_PLUGINS

/*
 *		Get the link defined by a junction or symlink
 *		(internal plugin)
 */

static int junction_readlink(ntfs_inode *ni, const REPARSE_POINT *reparse __attribute__((unused)), char **pbuf)
{
    int res;
    le32 tag;
    int lth;

    errno = 0;
    res = 0;
    *pbuf = ntfs_make_symlink(ni, ctx->abs_mnt_point);
    if (!*pbuf) {
        if (errno == EOPNOTSUPP) {
            *pbuf = (char*)ntfs_malloc(ntfs_bad_reparse_lth + 1);
            if (*pbuf) {
                if (reparse)
                    tag = reparse->reparse_tag;
                else
                    tag = const_cpu_to_le32(0);
                lth = snprintf(*pbuf, ntfs_bad_reparse_lth + 1, ntfs_bad_reparse, (long)le32_to_cpu(tag));
                if (lth != ntfs_bad_reparse_lth) {
                    free(*pbuf);
                    *pbuf = (char*)NULL;
                    res = -errno;
                }
            } else
                res = -ENOMEM;
        } else
            res = -errno;
    }
    return (res);
}
#endif /* DISABLE_PLUGINS */

static int wsl_getattr(ntfs_inode *ni, const REPARSE_POINT *reparse, struct stat *stbuf)
{
    dev_t rdev;
    int res;

    res = ntfs_reparse_check_wsl(ni, reparse);
    if (!res) {
        switch (reparse->reparse_tag) {
        case IO_REPARSE_TAG_AF_UNIX :
            stbuf->st_mode = S_IFSOCK;
            break;
        case IO_REPARSE_TAG_LX_FIFO :
            stbuf->st_mode = S_IFIFO;
            break;
        case IO_REPARSE_TAG_LX_CHR :
            stbuf->st_mode = S_IFCHR;
            res = ntfs_ea_check_wsldev(ni, &rdev);
            stbuf->st_rdev = rdev;
            break;
        case IO_REPARSE_TAG_LX_BLK :
            stbuf->st_mode = S_IFBLK;
            res = ntfs_ea_check_wsldev(ni, &rdev);
            stbuf->st_rdev = rdev;
            break;
        default :
            stbuf->st_size = ntfs_bad_reparse_lth;
            stbuf->st_mode = S_IFLNK;
            break;
        }
    }
    /*
     * If the reparse point is not a valid wsl special file
     * we display as a symlink
     */
    if (res) {
        stbuf->st_size = ntfs_bad_reparse_lth;
        stbuf->st_mode = S_IFLNK;
        res = 0;
    }
    return (res);
}

static struct fuse *mount_fuse(char *parsed_options, const char* mountPoint)
{
    struct fuse *fh = NULL;
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

    ctx->fc = try_fuse_mount(mountPoint, parsed_options);
    if (!ctx->fc) {
        return NULL;
    }

    if (fuse_opt_add_arg(&args, "andsec-sandbox") == -1) {
        goto err;
    }
    if (ctx->ro) {
        char buf[128];
        int len = snprintf(buf, sizeof(buf), "-ouse_ino,kernel_cache,attr_timeout=%d,entry_timeout=%d", (int)TIMEOUT_RO, (int)TIMEOUT_RO);
        if ((len < 0) || (len >= (int)sizeof(buf)) || (fuse_opt_add_arg(&args, buf) == -1)) {
            goto err;
        }
    } else {
#if !CACHEING
        if (fuse_opt_add_arg(&args, "-ouse_ino,kernel_cache"
                ",attr_timeout=0") == -1)
            goto err;
#else
        if (fuse_opt_add_arg(&args, "-ouse_ino,kernel_cache,attr_timeout=1") == -1)
            goto err;
#endif
    }
    if (ctx->debug) {
        if (fuse_opt_add_arg(&args, "-odebug") == -1) {
            goto err;
        }
    }

    fh = fuse_new(ctx->fc, &args , &ntfs_3g_ops, sizeof(ntfs_3g_ops), NULL);
    if (!fh)
        goto err;

    if (fuse_set_signal_handlers(fuse_get_session(fh)))
        goto err_destory;
out:
    fuse_opt_free_args(&args);
    return fh;

err_destory:
    fuse_destroy(fh);
    fh = NULL;

err:
    fuse_unmount(mountPoint, ctx->fc);
    goto out;
}

static int ntfs_fuse_getattr(const char *org_path, struct stat *stbuf)
{
	int res = 0;
	ntfs_inode *ni;
	ntfs_attr *na;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	BOOL withusermapping;
	struct SECURITY_CONTEXT security;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	memset(stbuf, 0, sizeof(struct stat));
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	withusermapping = ntfs_fuse_fill_security_context(&security);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		/*
		 * make sure the parent directory is searchable
		 */
	if (withusermapping
	    && !ntfs_allowed_dir_access(&security,path,
			(!strcmp(org_path,"/") ? ni : (ntfs_inode*)NULL),
			ni, S_IEXEC)) {
               	res = -EACCES;
               	goto exit;
	}
#endif
	stbuf->st_nlink = le16_to_cpu(ni->mrec->link_count);
	if (ctx->posix_nlink
	    && !(ni->flags & FILE_ATTR_REPARSE_POINT))
		stbuf->st_nlink = ntfs_dir_link_cnt(ni);

	if (((ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
		|| (ni->flags & FILE_ATTR_REPARSE_POINT))
	    && !stream_name_len) {
		if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
			const plugin_operations_t *ops;
			REPARSE_POINT *reparse;

			res = CALL_REPARSE_PLUGIN(ni, getattr, stbuf);
			if (!res) {
				apply_umask(stbuf);
				goto ok;
			} else {
				stbuf->st_size = ntfs_bad_reparse_lth;
				stbuf->st_blocks =
					(ni->allocated_size + 511) >> 9;
				stbuf->st_mode = S_IFLNK;
				res = 0;
				goto ok;
			}
			goto exit;
#else /* DISABLE_PLUGINS */
			char *target;

			errno = 0;
			target = ntfs_make_symlink(ni, ctx->abs_mnt_point);
				/*
				 * If the reparse point is not a valid
				 * directory junction, and there is no error
				 * we still display as a symlink
				 */
			if (target || (errno == EOPNOTSUPP)) {
				if (target)
					stbuf->st_size = strlen(target);
				else
					stbuf->st_size = ntfs_bad_reparse_lth;
				stbuf->st_blocks = (ni->allocated_size + 511) >> 9;
				stbuf->st_nlink = le16_to_cpu(ni->mrec->link_count);
				stbuf->st_mode = S_IFLNK;
				free(target);
			} else {
				res = -errno;
				goto exit;
			}
#endif /* DISABLE_PLUGINS */
		} else {
			/* Directory. */
			stbuf->st_mode = S_IFDIR | (0777 & ~ctx->dmask);
			/* get index size, if not known */
			if (!test_nino_flag(ni, KnownSize)) {
				na = ntfs_attr_open(ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
				if (na) {
					ni->data_size = na->data_size;
					ni->allocated_size = na->allocated_size;
					set_nino_flag(ni, KnownSize);
					ntfs_attr_close(na);
				}
			}
			stbuf->st_size = ni->data_size;
			stbuf->st_blocks = ni->allocated_size >> 9;
			if (!ctx->posix_nlink)
				stbuf->st_nlink = 1;	/* Make find(1) work */
		}
	} else {
		/* Regular or Interix (INTX) file. */
		stbuf->st_mode = S_IFREG;
		stbuf->st_size = ni->data_size;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
		/*
		 * return data size rounded to next 512 byte boundary for
		 * encrypted files to include padding required for decryption
		 * also include 2 bytes for padding info
		*/
		if (ctx->efs_raw
		    && (ni->flags & FILE_ATTR_ENCRYPTED)
		    && ni->data_size)
			stbuf->st_size = ((ni->data_size + 511) & ~511) + 2;
#endif /* HAVE_SETXATTR */
		/*
		 * Temporary fix to make ActiveSync work via Samba 3.0.
		 * See more on the ntfs-3g-devel list.
		 */
		stbuf->st_blocks = (ni->allocated_size + 511) >> 9;
		if (ni->flags & FILE_ATTR_SYSTEM || stream_name_len) {
			na = ntfs_attr_open(ni, AT_DATA, stream_name,
					stream_name_len);
			if (!na) {
				if (stream_name_len) {
					res = -ENOENT;
					goto exit;
				} else
					goto nodata;
			}
			if (stream_name_len) {
				stbuf->st_size = na->data_size;
				stbuf->st_blocks = na->allocated_size >> 9;
			}
			/* Check whether it's Interix FIFO or socket. */
			if (!(ni->flags & FILE_ATTR_HIDDEN) &&
					!stream_name_len) {
				/* FIFO. */
				if (na->data_size == 0)
					stbuf->st_mode = S_IFIFO;
				/* Socket link. */
				if (na->data_size == 1)
					stbuf->st_mode = S_IFSOCK;
			}
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* encrypted named stream */
			/* round size up to next 512 byte boundary */
			if (ctx->efs_raw && stream_name_len &&
			    (na->data_flags & ATTR_IS_ENCRYPTED) &&
			    NAttrNonResident(na))
				stbuf->st_size = ((na->data_size+511) & ~511)+2;
#endif /* HAVE_SETXATTR */
			/*
			 * Check whether it's Interix symbolic link, block or
			 * character device.
			 */
			if ((u64)na->data_size <= sizeof(INTX_FILE_TYPES)
					+ sizeof(ntfschar) * PATH_MAX
				&& (u64)na->data_size >
					sizeof(INTX_FILE_TYPES)
				&& !stream_name_len) {

				INTX_FILE *intx_file;

				intx_file = ntfs_malloc(na->data_size);
				if (!intx_file) {
					res = -errno;
					ntfs_attr_close(na);
					goto exit;
				}
				if (ntfs_attr_pread(na, 0, na->data_size,
						intx_file) != na->data_size) {
					res = -errno;
					free(intx_file);
					ntfs_attr_close(na);
					goto exit;
				}
				if (intx_file->magic == INTX_BLOCK_DEVICE &&
						na->data_size == offsetof(
						INTX_FILE, device_end)) {
					stbuf->st_mode = S_IFBLK;
					stbuf->st_rdev = makedev(le64_to_cpu(
							intx_file->major),
							le64_to_cpu(
							intx_file->minor));
				}
				if (intx_file->magic == INTX_CHARACTER_DEVICE &&
						na->data_size == offsetof(
						INTX_FILE, device_end)) {
					stbuf->st_mode = S_IFCHR;
					stbuf->st_rdev = makedev(le64_to_cpu(
							intx_file->major),
							le64_to_cpu(
							intx_file->minor));
				}
				if (intx_file->magic == INTX_SYMBOLIC_LINK) {
					char *target = NULL;
					int len;

					/* st_size should be set to length of
					 * symlink target as multibyte string */
					len = ntfs_ucstombs(
							intx_file->target,
							(na->data_size -
							    offsetof(INTX_FILE,
								     target)) /
							       sizeof(ntfschar),
							     &target, 0);
					if (len < 0) {
						res = -errno;
						free(intx_file);
						ntfs_attr_close(na);
						goto exit;
					}
					free(target);
					stbuf->st_mode = S_IFLNK;
					stbuf->st_size = len;
				}
				free(intx_file);
			}
			ntfs_attr_close(na);
		}
		stbuf->st_mode |= (0777 & ~ctx->fmask);
	}
#ifndef DISABLE_PLUGINS
ok:
#endif /* DISABLE_PLUGINS */
	if (withusermapping) {
		if (ntfs_get_owner_mode(&security,ni,stbuf) < 0)
			set_fuse_error(&res);
	} else {
		stbuf->st_uid = ctx->uid;
       		stbuf->st_gid = ctx->gid;
	}
	if (S_ISLNK(stbuf->st_mode))
		stbuf->st_mode |= 0777;
nodata :
	stbuf->st_ino = ni->mft_no;
#ifdef HAVE_STRUCT_STAT_ST_ATIMESPEC
	stbuf->st_atimespec = ntfs2timespec(ni->last_access_time);
	stbuf->st_ctimespec = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_mtimespec = ntfs2timespec(ni->last_data_change_time);
#elif defined(HAVE_STRUCT_STAT_ST_ATIM)
 	stbuf->st_atim = ntfs2timespec(ni->last_access_time);
 	stbuf->st_ctim = ntfs2timespec(ni->last_mft_change_time);
 	stbuf->st_mtim = ntfs2timespec(ni->last_data_change_time);
#elif defined(HAVE_STRUCT_STAT_ST_ATIMENSEC)
	{
	struct timespec ts;

	ts = ntfs2timespec(ni->last_access_time);
	stbuf->st_atime = ts.tv_sec;
	stbuf->st_atimensec = ts.tv_nsec;
	ts = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_ctime = ts.tv_sec;
	stbuf->st_ctimensec = ts.tv_nsec;
	ts = ntfs2timespec(ni->last_data_change_time);
	stbuf->st_mtime = ts.tv_sec;
	stbuf->st_mtimensec = ts.tv_nsec;
	}
#else
#warning "No known way to set nanoseconds in struct stat !"
	{
	struct timespec ts;

	ts = ntfs2timespec(ni->last_access_time);
	stbuf->st_atime = ts.tv_sec;
	ts = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_ctime = ts.tv_sec;
	ts = ntfs2timespec(ni->last_data_change_time);
	stbuf->st_mtime = ts.tv_sec;
	}
#endif
exit:
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_readlink(const char *org_path, char *buf, size_t buf_size)
{
	char *path = NULL;
	ntfschar *stream_name;
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	INTX_FILE *intx_file = NULL;
	int stream_name_len, res = 0;
	REPARSE_POINT *reparse;
	le32 tag;
	int lth;

	/* Get inode. */
	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	if (stream_name_len > 0) {
		res = -EINVAL;
		goto exit;
	}
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
		/*
		 * Reparse point : analyze as a junction point
		 */
	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
		char *gotlink;
		const plugin_operations_t *ops;

		gotlink = (char*)NULL;
		res = CALL_REPARSE_PLUGIN(ni, readlink, &gotlink);
		if (gotlink) {
			strncpy(buf, gotlink, buf_size);
			free(gotlink);
			res = 0;
		} else {
			errno = EOPNOTSUPP;
			res = -EOPNOTSUPP;
		}
#else /* DISABLE_PLUGINS */
		char *target;

		errno = 0;
		res = 0;
		target = ntfs_make_symlink(ni, ctx->abs_mnt_point);
		if (target) {
			strncpy(buf,target,buf_size);
			free(target);
		} else
			res = -errno;
#endif /* DISABLE_PLUGINS */
		if (res == -EOPNOTSUPP) {
			reparse = ntfs_get_reparse_point(ni);
			if (reparse) {
				tag = reparse->reparse_tag;
				free(reparse);
			} else
				tag = const_cpu_to_le32(0);
			lth = snprintf(buf, ntfs_bad_reparse_lth + 1,
					ntfs_bad_reparse,
					(long)le32_to_cpu(tag));
			res = 0;
			if (lth != ntfs_bad_reparse_lth)
				res = -errno;
		}
		goto exit;
	}
	/* Sanity checks. */
	if (!(ni->flags & FILE_ATTR_SYSTEM)) {
		res = -EINVAL;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		res = -errno;
		goto exit;
	}
	if ((size_t)na->data_size <= sizeof(INTX_FILE_TYPES)) {
		res = -EINVAL;
		goto exit;
	}
	if ((size_t)na->data_size > sizeof(INTX_FILE_TYPES) +
			sizeof(ntfschar) * PATH_MAX) {
		res = -ENAMETOOLONG;
		goto exit;
	}
	/* Receive file content. */
	intx_file = ntfs_malloc(na->data_size);
	if (!intx_file) {
		res = -errno;
		goto exit;
	}
	if (ntfs_attr_pread(na, 0, na->data_size, intx_file) != na->data_size) {
		res = -errno;
		goto exit;
	}
	/* Sanity check. */
	if (intx_file->magic != INTX_SYMBOLIC_LINK) {
		res = -EINVAL;
		goto exit;
	}
	/* Convert link from unicode to local encoding. */
	if (ntfs_ucstombs(intx_file->target, (na->data_size -
			offsetof(INTX_FILE, target)) / sizeof(ntfschar),
			&buf, buf_size) < 0) {
		res = -errno;
		goto exit;
	}
exit:
	if (intx_file)
		free(intx_file);
	if (na)
		ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset __attribute__((unused)), struct fuse_file_info *fi __attribute__((unused)))
{
    ntfs_fuse_fill_context_t fill_ctx;
    ntfs_inode *ni;
    s64 pos = 0;
    int err = 0;

    fill_ctx.filler = filler;
    fill_ctx.buf = buf;
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;

    if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
        const plugin_operations_t *ops;
        REPARSE_POINT *reparse;

        err = CALL_REPARSE_PLUGIN(ni, readdir, &pos, &fill_ctx,
                (ntfs_filldir_t)ntfs_fuse_filler, fi);
#else /* DISABLE_PLUGINS */
        err = -EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
    } else {
        if (ntfs_readdir(ni, &pos, &fill_ctx,
                (ntfs_filldir_t)ntfs_fuse_filler))
            err = -errno;
    }
    ntfs_fuse_update_times(ni, NTFS_UPDATE_ATIME);
    if (ntfs_inode_close(ni))
        set_fuse_error(&err);
    return err;
}

static int ntfs_fuse_open(const char *org_path,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		struct fuse_file_info *fi)
#else
		struct fuse_file_info *fi __attribute__((unused)))
#endif
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	int res = 0;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	int accesstype;
	struct SECURITY_CONTEXT security;
#endif

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (ni) {
		if (!(ni->flags & FILE_ATTR_REPARSE_POINT)) {
			na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
			if (!na) {
				res = -errno;
				goto close;
			}
		}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		if (ntfs_fuse_fill_security_context(&security)) {
			if (fi->flags & O_WRONLY)
				accesstype = S_IWRITE;
			else
				if (fi->flags & O_RDWR)
					 accesstype = S_IWRITE | S_IREAD;
				else
					accesstype = S_IREAD;
			/*
			 * directory must be searchable
			 * and requested access allowed
			 */
			if (!ntfs_allowed_dir_access(&security,
				    path,(ntfs_inode*)NULL,ni,S_IEXEC)
			  || !ntfs_allowed_access(&security,
					ni,accesstype))
				res = -EACCES;
		}
#endif
		if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
			const plugin_operations_t *ops;
			REPARSE_POINT *reparse;

			fi->fh = 0;
			res = CALL_REPARSE_PLUGIN(ni, open, fi);
#else /* DISABLE_PLUGINS */
			res = -EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
			goto close;
		}
		if ((res >= 0)
		    && (fi->flags & (O_WRONLY | O_RDWR))) {
		/* mark a future need to compress the last chunk */
			if (na->data_flags & ATTR_COMPRESSION_MASK)
				fi->fh |= CLOSE_COMPRESSED;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* mark a future need to fixup encrypted inode */
			if (ctx->efs_raw
			    && !(na->data_flags & ATTR_IS_ENCRYPTED)
			    && (ni->flags & FILE_ATTR_ENCRYPTED))
				fi->fh |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
		/* mark a future need to update the mtime */
			if (ctx->dmtime)
				fi->fh |= CLOSE_DMTIME;
		/* deny opening metadata files for writing */
			if (ni->mft_no < FILE_first_user)
				res = -EPERM;
		}
		ntfs_attr_close(na);
close:
		if (ntfs_inode_close(ni))
			set_fuse_error(&res);
	} else
		res = -errno;
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_release(const char *org_path, struct fuse_file_info *fi)
{
    ntfs_inode *ni = NULL;
    ntfs_attr *na = NULL;
    char *path = NULL;
    ntfschar *stream_name;
    int stream_name_len, res;

    if (!fi) {
        res = -EINVAL;
        goto out;
    }

    /* Only for marked descriptors there is something to do */

    if (!fi->fh) {
        res = 0;
        goto out;
    }
    stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
    if (stream_name_len < 0) {
        res = stream_name_len;
        goto out;
    }
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni) {
        res = -errno;
        goto exit;
    }
    if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
        const plugin_operations_t *ops;
        REPARSE_POINT *reparse;

        if (stream_name_len) {
            res = -EINVAL;
            goto exit;
        }
        res = CALL_REPARSE_PLUGIN(ni, release, fi);
        if (!res) {
            goto stamps;
        }
#else /* DISABLE_PLUGINS */
        /* Assume release() was not needed */
        res = 0;
#endif /* DISABLE_PLUGINS */
        goto exit;
    }
    na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
    if (!na) {
        res = -errno;
        goto exit;
    }
    res = 0;
    if (fi->fh & CLOSE_COMPRESSED)
        res = ntfs_attr_pclose(na);
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
    if (fi->fh & CLOSE_ENCRYPTED)
        res = ntfs_efs_fixup_attribute(NULL, na);
#endif /* HAVE_SETXATTR */
#ifndef DISABLE_PLUGINS
    stamps:
    #endif /* DISABLE_PLUGINS */
        if (fi->fh & CLOSE_DMTIME)
            ntfs_inode_update_times(ni,NTFS_UPDATE_MCTIME);
    exit:
        if (na)
            ntfs_attr_close(na);
    if (ntfs_inode_close(ni))
        set_fuse_error(&res);
    free(path);
    if (stream_name_len)
        free(stream_name);
    out:
        return res;
}

static int ntfs_fuse_read(const char *org_path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len, res;
	s64 total = 0;
	s64 max_read;

	if (!size)
		return 0;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
		const plugin_operations_t *ops;
		REPARSE_POINT *reparse;

		if (stream_name_len || !fi) {
			res = -EINVAL;
			goto exit;
		}
		res = CALL_REPARSE_PLUGIN(ni, read, buf, size, offset, fi);
		if (res >= 0) {
			goto stamps;
		}
#else /* DISABLE_PLUGINS */
		res = -EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na) {
		res = -errno;
		goto exit;
	}
	max_read = na->data_size;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	/* limit reads at next 512 byte boundary for encrypted attributes */
	if (ctx->efs_raw
	    && max_read
	    && (na->data_flags & ATTR_IS_ENCRYPTED)
	    && NAttrNonResident(na)) {
		max_read = ((na->data_size+511) & ~511) + 2;
	}
#endif /* HAVE_SETXATTR */
	if (offset + (off_t)size > max_read) {
		if (max_read < offset)
			goto ok;
		size = max_read - offset;
	}
	while (size > 0) {
		s64 ret = ntfs_attr_pread(na, offset, size, buf + total);
		if (ret != (s64)size)
			C_LOG_WARNING("ntfs_attr_pread error reading '%s' at "
				"offset %lld: %lld <> %lld", org_path,
				(long long)offset, (long long)size, (long long)ret);
		if (ret <= 0 || ret > (s64)size) {
			res = (ret < 0) ? -errno : -EIO;
			goto exit;
		}
		size -= ret;
		offset += ret;
		total += ret;
	}
ok:
	res = total;
#ifndef DISABLE_PLUGINS
stamps:
#endif /* DISABLE_PLUGINS */
	ntfs_fuse_update_times(ni, NTFS_UPDATE_ATIME);
exit:
	if (na)
		ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_write(const char *org_path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi __attribute__((unused)))
{
    ntfs_inode *ni = NULL;
    ntfs_attr *na = NULL;
    char *path = NULL;
    ntfschar *stream_name;
    int stream_name_len, res, total = 0;

    stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
    if (stream_name_len < 0) {
        res = stream_name_len;
        goto out;
    }
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni) {
        res = -errno;
        goto exit;
    }
    if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
        const plugin_operations_t *ops;
        REPARSE_POINT *reparse;

        if (stream_name_len || !fi) {
            res = -EINVAL;
            goto exit;
        }
        res = CALL_REPARSE_PLUGIN(ni, write, buf, size, offset, fi);
        if (res >= 0) {
            goto stamps;
        }
#else /* DISABLE_PLUGINS */
        res = -EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
        goto exit;
    }
    na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
    if (!na) {
        res = -errno;
        goto exit;
    }
    while (size) {
        s64 ret = ntfs_attr_pwrite(na, offset, size, buf + total);
        if (ret <= 0) {
            res = -errno;
            goto exit;
        }
        size   -= ret;
        offset += ret;
        total  += ret;
    }
    res = total;
#ifndef DISABLE_PLUGINS
    stamps:
    #endif /* DISABLE_PLUGINS */
        if ((res > 0)
            && (!ctx->dmtime
            || (sle64_to_cpu(ntfs_current_time())
                 - sle64_to_cpu(ni->last_data_change_time)) > ctx->dmtime))
            ntfs_fuse_update_times(ni, NTFS_UPDATE_MCTIME);
    exit:
        if (na)
            ntfs_attr_close(na);
    if (res > 0)
        set_archive(ni);
    if (ntfs_inode_close(ni))
        set_fuse_error(&res);
    free(path);
    if (stream_name_len)
        free(stream_name);
    out:
        return res;
}

static int ntfs_fuse_truncate(const char *org_path, off_t size)
{
    return ntfs_fuse_trunc(org_path, size, TRUE);
}

static int ntfs_fuse_ftruncate(const char *org_path, off_t size, struct fuse_file_info *fi __attribute__((unused)))
{
    /*
     * in ->ftruncate() the file handle is guaranteed
     * to have been opened for write.
     */
    return (ntfs_fuse_trunc(org_path, size, FALSE));
}

static int ntfs_fuse_statfs(const char *path __attribute__((unused)), struct statvfs *sfs)
{
    s64 size;
    int delta_bits;
    ntfs_volume *vol;

    vol = ctx->vol;
    if (!vol)
        return -ENODEV;

    /*
     * File system block size. Used to calculate used/free space by df.
     * Incorrectly documented as "optimal transfer block size".
     */
    sfs->f_bsize = vol->cluster_size;

    /* Fundamental file system block size, used as the unit. */
    sfs->f_frsize = vol->cluster_size;

    /*
     * Total number of blocks on file system in units of f_frsize.
     * Since inodes are also stored in blocks ($MFT is a file) hence
     * this is the number of clusters on the volume.
     */
    sfs->f_blocks = vol->nr_clusters;

    /* Free blocks available for all and for non-privileged processes. */
    size = vol->free_clusters;
    if (size < 0)
        size = 0;
    sfs->f_bavail = sfs->f_bfree = size;

    /* Free inodes on the free space */
    delta_bits = vol->cluster_size_bits - vol->mft_record_size_bits;
    if (delta_bits >= 0)
        size <<= delta_bits;
    else
        size >>= -delta_bits;

    /* Number of inodes at this point in time. */
    sfs->f_files = (vol->mftbmp_na->allocated_size << 3) + size;

    /* Free inodes available for all and for non-privileged processes. */
    size += vol->free_mft_records;
    if (size < 0)
        size = 0;
    sfs->f_ffree = sfs->f_favail = size;

    /* Maximum length of filenames. */
    sfs->f_namemax = NTFS_MAX_NAME_LEN;
    return 0;
}

static int ntfs_fuse_chmod(const char *path, mode_t mode)
{
    int res = 0;
    ntfs_inode *ni;
    struct SECURITY_CONTEXT security;

    if (ntfs_fuse_is_named_data_stream(path))
        return -EINVAL; /* n/a for named data streams. */

    /*
     * Return unsupported if no user mapping has been defined
     * or enforcing Windows-type inheritance
     */
    if (ctx->inherit
        || !ntfs_fuse_fill_security_context(&security)) {
        if (ctx->silent)
            res = 0;
        else
            res = -EOPNOTSUPP;
        } else {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
            /* parent directory must be executable */
            if (ntfs_allowed_dir_access(&security,path,
                    (ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
#endif
                ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
                if (!ni)
                    res = -errno;
                else {
                    if (ntfs_set_mode(&security,ni,mode))
                        res = -errno;
                    else
                        ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
                    NInoSetDirty(ni);
                    if (ntfs_inode_close(ni))
                        set_fuse_error(&res);
                }
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
                    } else
                        res = -errno;
#endif
        }
    return res;
}

static int ntfs_fuse_chown(const char *path, uid_t uid, gid_t gid)
{
    ntfs_inode *ni;
    int res;
    struct SECURITY_CONTEXT security;

    if (ntfs_fuse_is_named_data_stream(path))
        return -EINVAL; /* n/a for named data streams. */
    /*
     * Return unsupported if no user mapping has been defined
     * or enforcing Windows-type inheritance
     */
    if (ctx->inherit
        || !ntfs_fuse_fill_security_context(&security)) {
        if (ctx->silent)
            return 0;
        if (uid == ctx->uid && gid == ctx->gid)
            return 0;
        return -EOPNOTSUPP;
        } else {
            res = 0;
            if (((int)uid != -1) || ((int)gid != -1)) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
                /* parent directory must be executable */

                if (ntfs_allowed_dir_access(&security,path,
                    (ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
#endif
                    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
                    if (!ni)
                        res = -errno;
                    else {
                        if (ntfs_set_owner(&security,
                                ni,uid,gid))
                            res = -errno;
                        else
                            ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
                        if (ntfs_inode_close(ni))
                            set_fuse_error(&res);
                    }
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
                    } else
                        res = -errno;
#endif
            }
        }
    return (res);
}

static int ntfs_fuse_create_file(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    return ntfs_fuse_mknod_common(path, mode, 0, fi);
}

static int ntfs_fuse_mknod(const char *path, mode_t mode, dev_t dev)
{
    return ntfs_fuse_mknod_common(path, mode, dev, (struct fuse_file_info*)NULL);
}

static int ntfs_fuse_symlink(const char *to, const char *from)
{
    if (ntfs_fuse_is_named_data_stream(from)) {
        return -EINVAL; /* n/a for named data streams. */
    }

    return ntfs_fuse_create(from, S_IFLNK, 0, to, (struct fuse_file_info*)NULL);
}

static int ntfs_fuse_link(const char *old_path, const char *new_path)
{
	char *name;
	ntfschar *uname = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *path;
	int res = 0, uname_len;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	BOOL samedir;
	struct SECURITY_CONTEXT security;
#endif

	if (ntfs_fuse_is_named_data_stream(old_path))
		return -EINVAL; /* n/a for named data streams. */
	if (ntfs_fuse_is_named_data_stream(new_path))
		return -EINVAL; /* n/a for named data streams. */
	path = strdup(new_path);
	if (!path)
		return -errno;
	/* Open file for which create hard link. */
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, old_path);
	if (!ni) {
		res = -errno;
		goto exit;
	}

	/* Generate unicode filename. */
	name = strrchr(path, '/');
	name++;
	uname_len = ntfs_mbstoucs(name, &uname);
	if ((uname_len < 0)
	    || (ctx->windows_names
		&& ntfs_forbidden_names(ctx->vol,uname,uname_len,TRUE))) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	*--name = 0;
	dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!dir_ni) {
		res = -errno;
		goto exit;
	}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	samedir = !strncmp(old_path, path, strlen(path))
			&& (old_path[strlen(path)] == '/');
		/* JPA make sure the parent directories are writeable */
	if (ntfs_fuse_fill_security_context(&security)
	   && ((!samedir && !ntfs_allowed_dir_access(&security,old_path,
			(ntfs_inode*)NULL,ni,S_IWRITE + S_IEXEC))
	      || !ntfs_allowed_access(&security,dir_ni,S_IWRITE + S_IEXEC)))
		res = -EACCES;
	else
#endif
	{
		if (dir_ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
			const plugin_operations_t *ops;
			REPARSE_POINT *reparse;

			res = CALL_REPARSE_PLUGIN(dir_ni, link,
					ni, uname, uname_len);
#else /* DISABLE_PLUGINS */
			errno = EOPNOTSUPP;
			res = -errno;
#endif /* DISABLE_PLUGINS */
			if (res)
				goto exit;
		} else
			if (ntfs_link(ni, dir_ni, uname, uname_len)) {
					res = -errno;
				goto exit;
			}

		set_archive(ni);
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		ntfs_fuse_update_times(dir_ni, NTFS_UPDATE_MCTIME);
	}
exit:
	/*
	 * Must close dir_ni first otherwise ntfs_inode_sync_file_name(ni)
	 * may fail because ni may not be in parent's index on the disk yet.
	 */
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(uname);
	free(path);
	return res;
}

static int ntfs_fuse_unlink(const char *org_path)
{
    char *path = NULL;
    ntfschar *stream_name;
    int stream_name_len;
    int res = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    struct SECURITY_CONTEXT security;
#endif

    stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
    if (stream_name_len < 0)
        return stream_name_len;
    if (!stream_name_len)
        res = ntfs_fuse_rm(path);
    else {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
        /*
         * JPA deny unlinking stream if directory is not
         * writable and executable (debatable)
         */
        if (!ntfs_fuse_fill_security_context(&security)
           || ntfs_allowed_dir_access(&security, path,
                (ntfs_inode*)NULL, (ntfs_inode*)NULL,
                S_IEXEC + S_IWRITE + S_ISVTX))
            res = ntfs_fuse_rm_stream(path, stream_name,
                    stream_name_len);
        else
            res = -errno;
#else
        res = ntfs_fuse_rm_stream(path, stream_name, stream_name_len);
#endif
    }
    free(path);
    if (stream_name_len)
        free(stream_name);
    return res;
}

static int ntfs_fuse_rename(const char *old_path, const char *new_path)
{
    int ret, stream_name_len;
    char *path = NULL;
    ntfschar *stream_name;
    ntfs_inode *ni;
    u64 inum;
    BOOL same;

    ntfs_log_debug("rename: old: '%s'  new: '%s'\n", old_path, new_path);

    /*
     *  FIXME: Rename should be atomic.
     */
    stream_name_len = ntfs_fuse_parse_path(new_path, &path, &stream_name);
    if (stream_name_len < 0)
        return stream_name_len;

    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (ni) {
        ret = ntfs_check_empty_dir(ni);
        if (ret < 0) {
            ret = -errno;
            ntfs_inode_close(ni);
            goto out;
        }

        inum = ni->mft_no;
        if (ntfs_inode_close(ni)) {
            set_fuse_error(&ret);
            goto out;
        }

        free(path);
        path = (char*)NULL;
        if (stream_name_len)
            free(stream_name);

        /* silently ignore a rename to same inode */
        stream_name_len = ntfs_fuse_parse_path(old_path,
                        &path, &stream_name);
        if (stream_name_len < 0)
            return stream_name_len;

        ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
        if (ni) {
            same = ni->mft_no == inum;
            if (ntfs_inode_close(ni))
                ret = -errno;
            else
                if (!same)
                    ret = ntfs_fuse_rename_existing_dest(
                            old_path, new_path);
        } else
            ret = -errno;
        goto out;
    }

    ret = ntfs_fuse_link(old_path, new_path);
    if (ret)
        goto out;

    ret = ntfs_fuse_unlink(old_path);
    if (ret)
        ntfs_fuse_unlink(new_path);
    out:
        free(path);
    if (stream_name_len)
        free(stream_name);
    return ret;
}

static int ntfs_fuse_mkdir(const char *path, mode_t mode)
{
    if (ntfs_fuse_is_named_data_stream(path))
        return -EINVAL; /* n/a for named data streams. */

    return ntfs_fuse_create(path, S_IFDIR | (mode & 07777), 0, NULL, (struct fuse_file_info*)NULL);
}

static int ntfs_fuse_rmdir(const char *path)
{
    if (ntfs_fuse_is_named_data_stream(path))
        return -EINVAL; /* n/a for named data streams. */
    return ntfs_fuse_rm(path);
}

#ifdef HAVE_UTIMENSAT

static int ntfs_fuse_utimens(const char *path, const struct timespec tv[2])
{
    ntfs_inode *ni;
    int res = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    struct SECURITY_CONTEXT security;
#endif

    if (ntfs_fuse_is_named_data_stream(path))
        return -EINVAL; /* n/a for named data streams. */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    /* parent directory must be executable */
    if (ntfs_fuse_fill_security_context(&security)
        && !ntfs_allowed_dir_access(&security,path,
            (ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
        return (-errno);
            }
#endif
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;

    /* no check or update if both UTIME_OMIT */
    if ((tv[0].tv_nsec != UTIME_OMIT) || (tv[1].tv_nsec != UTIME_OMIT)) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
        if (ntfs_allowed_as_owner(&security, ni)
            || ((tv[0].tv_nsec == UTIME_NOW)
            && (tv[1].tv_nsec == UTIME_NOW)
            && ntfs_allowed_access(&security, ni, S_IWRITE))) {
#endif
            ntfs_time_update_flags mask = NTFS_UPDATE_CTIME;

            if (tv[0].tv_nsec == UTIME_NOW)
                mask |= NTFS_UPDATE_ATIME;
            else
                if (tv[0].tv_nsec != UTIME_OMIT)
                    ni->last_access_time
                        = timespec2ntfs(tv[0]);
            if (tv[1].tv_nsec == UTIME_NOW)
                mask |= NTFS_UPDATE_MTIME;
            else
                if (tv[1].tv_nsec != UTIME_OMIT)
                    ni->last_data_change_time
                        = timespec2ntfs(tv[1]);
            ntfs_inode_update_times(ni, mask);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
            } else
                res = -errno;
#endif
    }
    if (ntfs_inode_close(ni))
        set_fuse_error(&res);
    return res;
}

#else /* HAVE_UTIMENSAT */

static int ntfs_fuse_utime(const char *path, struct utimbuf *buf)
{
	ntfs_inode *ni;
	int res = 0;
	struct timespec actime;
	struct timespec modtime;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	BOOL ownerok;
	BOOL writeok;
	struct SECURITY_CONTEXT security;
#endif

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,
			(ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ownerok = ntfs_allowed_as_owner(&security, ni);
	if (buf) {
		/*
		 * fuse never calls with a NULL buf and we do not
		 * know whether the specific condition can be applied
		 * So we have to accept updating by a non-owner having
		 * write access.
		 */
		writeok = !ownerok
			&& (buf->actime == buf->modtime)
			&& ntfs_allowed_access(&security, ni, S_IWRITE);
			/* Must be owner */
		if (!ownerok && !writeok)
			res = (buf->actime == buf->modtime ? -EACCES : -EPERM);
		else {
			actime.tv_sec = buf->actime;
			actime.tv_nsec = 0;
			modtime.tv_sec = buf->modtime;
			modtime.tv_nsec = 0;
			ni->last_access_time = timespec2ntfs(actime);
			ni->last_data_change_time = timespec2ntfs(modtime);
			ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		}
	} else {
			/* Must be owner or have write access */
		writeok = !ownerok
			&& ntfs_allowed_access(&security, ni, S_IWRITE);
		if (!ownerok && !writeok)
			res = -EACCES;
		else
			ntfs_inode_update_times(ni, NTFS_UPDATE_AMCTIME);
	}
#else
	if (buf) {
		actime.tv_sec = buf->actime;
		actime.tv_nsec = 0;
		modtime.tv_sec = buf->modtime;
		modtime.tv_nsec = 0;
		ni->last_access_time = timespec2ntfs(actime);
		ni->last_data_change_time = timespec2ntfs(modtime);
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
	} else
		ntfs_inode_update_times(ni, NTFS_UPDATE_AMCTIME);
#endif

	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#endif /* HAVE_UTIMENSAT */

static int ntfs_fuse_fsync(const char *path __attribute__((unused)), int type __attribute__((unused)), struct fuse_file_info *fi __attribute__((unused)))
{
    int ret;

    /* sync the full device */
    ret = ntfs_device_sync(ctx->vol->dev);
    if (ret)
        ret = -errno;
    return (ret);
}

static int ntfs_fuse_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    ntfs_inode *ni;
    ntfs_attr *na;
    LCN lcn;
    int ret = 0;
    int cl_per_bl = ctx->vol->cluster_size / blocksize;

    if (blocksize > ctx->vol->cluster_size)
        return -EINVAL;

    if (ntfs_fuse_is_named_data_stream(path))
        return -EINVAL;

    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;

    na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
    if (!na) {
        ret = -errno;
        goto close_inode;
    }

    if ((na->data_flags & (ATTR_COMPRESSION_MASK | ATTR_IS_ENCRYPTED))
             || !NAttrNonResident(na)) {
        ret = -EINVAL;
        goto close_attr;
             }

    if (ntfs_attr_map_whole_runlist(na)) {
        ret = -errno;
        goto close_attr;
    }

    lcn = ntfs_rl_vcn_to_lcn(na->rl, *idx / cl_per_bl);
    *idx = (lcn > 0) ? lcn * cl_per_bl + *idx % cl_per_bl : 0;

    close_attr:
        ntfs_attr_close(na);
    close_inode:
        if (ntfs_inode_close(ni))
            set_fuse_error(&ret);
    return ret;
}

static void ntfs_fuse_destroy2(void *unused __attribute__((unused)))
{
    ntfs_close();
}

#if defined(FUSE_INTERNAL) || (FUSE_VERSION >= 28)
static int ntfs_fuse_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi __attribute__((unused)), unsigned int flags, void *data)
{
    ntfs_inode *ni;
    int ret;

    if (flags & FUSE_IOCTL_COMPAT)
        return -ENOSYS;

    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;

    /*
     * Linux defines the request argument of ioctl() to be an
     * unsigned long, which fuse 2.x forwards as a signed int into
     * which the request sometimes does not fit.
     * So we must expand the value and make sure it is not sign-extended.
     */
    ret = ntfs_ioctl(ni, (unsigned int)cmd, arg, flags, data);

    if (ntfs_inode_close (ni))
        set_fuse_error(&ret);
    return ret;
}
#endif /* defined(FUSE_INTERNAL) || (FUSE_VERSION >= 28) */

#if defined(__APPLE__) || defined(__DARWIN__)
static int ntfs_fuse_getxattr(const char *path, const char *name, char *value, size_t size, uint32_t position)
#else
static int ntfs_fuse_getxattr(const char *path, const char *name, char *value, size_t size)
#endif
{
#if !(defined(__APPLE__) || defined(__DARWIN__))
	static const unsigned int position = 0U;
#endif

	ntfs_inode *ni;
	ntfs_inode *dir_ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res, lename_len;
	s64 rsize;
	enum SYSTEMXATTRS attr;
	int namespace;
	struct SECURITY_CONTEXT security;

#if defined(__APPLE__) || defined(__DARWIN__)
	/* If the attribute is not a resource fork attribute and the position
	 * parameter is non-zero, we return with EINVAL as requesting position
	 * is not permitted for non-resource fork attributes. */
	if (position && strcmp(name, XATTR_RESOURCEFORK_NAME)) {
		return -EINVAL;
	}
#endif

	attr = ntfs_xattr_system_type(name,ctx->vol);
	if (attr != XATTR_UNMAPPED) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * hijack internal data and ACL retrieval, whatever
			 * mode was selected for xattr (from the user's
			 * point of view, ACLs are not xattr)
			 */
		ni = ntfs_check_access_xattr(&security, path, attr, FALSE);
		if (ni) {
			if (ntfs_allowed_access(&security,ni,S_IREAD)) {
				if (attr == XATTR_NTFS_DOS_NAME)
					dir_ni = get_parent_dir(path);
				else
					dir_ni = (ntfs_inode*)NULL;
				res = ntfs_xattr_system_getxattr(&security,
					attr, ni, dir_ni, value, size);
				if (dir_ni && ntfs_inode_close(dir_ni))
					set_fuse_error(&res);
			} else {
				res = -errno;
                        }
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
#else
			/*
			 * Only hijack NTFS ACL retrieval if POSIX ACLS
			 * option is not selected
			 * Access control is done by fuse
			 */
		if (ntfs_fuse_is_named_data_stream(path))
			res = -EINVAL; /* n/a for named data streams. */
		else {
			ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
			if (ni) {
					/* user mapping not mandatory */
				ntfs_fuse_fill_security_context(&security);
				if (attr == XATTR_NTFS_DOS_NAME)
					dir_ni = get_parent_dir(path);
				else
					dir_ni = (ntfs_inode*)NULL;
				res = ntfs_xattr_system_getxattr(&security,
					attr, ni, dir_ni, value, size);
				if (dir_ni && ntfs_inode_close(dir_ni))
					set_fuse_error(&res);
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			} else
				res = -errno;
		}
#endif
		return (res);
	}
	if (ctx->streams == NF_STREAMS_INTERFACE_WINDOWS)
		return ntfs_fuse_getxattr_windows(path, name, value, size);
	if (ctx->streams == NF_STREAMS_INTERFACE_NONE)
		return -EOPNOTSUPP;
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE)
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
		/* trusted only readable by root */
	if ((namespace == XATTRNS_TRUSTED)
	    && security.uid)
		    return -NTFS_NOXATTR_ERRNO;
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
		/* Return with no result for symlinks, fifo, etc. */
	if (!user_xattrs_allowed(ctx, ni)) {
		res = -NTFS_NOXATTR_ERRNO;
		goto exit;
	}
		/* otherwise file must be readable */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	if (!ntfs_allowed_access(&security, ni, S_IREAD)) {
		res = -errno;
		goto exit;
	}
#endif
	lename_len = fix_xattr_prefix(name, namespace, &lename);
	if (lename_len == -1) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
	if (!na) {
		res = -NTFS_NOXATTR_ERRNO;
		goto exit;
	}
	rsize = na->data_size;
	if (ctx->efs_raw
	    && rsize
	    && (na->data_flags & ATTR_IS_ENCRYPTED)
	    && NAttrNonResident(na))
		rsize = ((na->data_size + 511) & ~511) + 2;
	rsize -= position;
	if (size) {
		if (size >= (size_t)rsize) {
			res = ntfs_attr_pread(na, position, rsize, value);
			if (res != rsize)
				res = -errno;
		} else
			res = -ERANGE;
	} else
		res = rsize;
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#if defined(__APPLE__) || defined(__DARWIN__)
static int ntfs_fuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t position)
#else
static int ntfs_fuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
#endif
{
#if !(defined(__APPLE__) || defined(__DARWIN__))
	static const unsigned int position = 0U;
#else
	BOOL is_resource_fork;
#endif

	ntfs_inode *ni;
	ntfs_inode *dir_ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res, lename_len;
	size_t total;
	s64 part;
	enum SYSTEMXATTRS attr;
	int namespace;
	struct SECURITY_CONTEXT security;

#if defined(__APPLE__) || defined(__DARWIN__)
	/* If the attribute is not a resource fork attribute and the position
	 * parameter is non-zero, we return with EINVAL as requesting position
	 * is not permitted for non-resource fork attributes. */
	is_resource_fork = strcmp(name, XATTR_RESOURCEFORK_NAME) ? FALSE : TRUE;
	if (position && !is_resource_fork) {
		return -EINVAL;
	}
#endif

	attr = ntfs_xattr_system_type(name,ctx->vol);
	if (attr != XATTR_UNMAPPED) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * hijack internal data and ACL setting, whatever
			 * mode was selected for xattr (from the user's
			 * point of view, ACLs are not xattr)
			 * Note : ctime updated on successful settings
			 */
		ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
		if (ni) {
			if (ntfs_allowed_as_owner(&security,ni)) {
				if (attr == XATTR_NTFS_DOS_NAME)
					dir_ni = get_parent_dir(path);
				else
					dir_ni = (ntfs_inode*)NULL;
				res = ntfs_xattr_system_setxattr(&security,
					attr, ni, dir_ni, value, size, flags);
				/* never have to close dir_ni */
				if (res)
					res = -errno;
			} else
				res = -errno;
			if (attr != XATTR_NTFS_DOS_NAME) {
				if (!res)
					ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			}
		} else
			res = -errno;
#else
			/*
			 * Only hijack NTFS ACL setting if POSIX ACLS
			 * option is not selected
			 * Access control is partially done by fuse
			 */
		if (ntfs_fuse_is_named_data_stream(path))
			res = -EINVAL; /* n/a for named data streams. */
		else {
			/* creation of a new name is not controlled by fuse */
			if (attr == XATTR_NTFS_DOS_NAME)
				ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
			else
				ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
			if (ni) {
					/*
					 * user mapping is not mandatory
					 * if defined, only owner is allowed
					 */
				if (!ntfs_fuse_fill_security_context(&security)
				   || ntfs_allowed_as_owner(&security,ni)) {
					if (attr == XATTR_NTFS_DOS_NAME)
						dir_ni = get_parent_dir(path);
					else
						dir_ni = (ntfs_inode*)NULL;
					res = ntfs_xattr_system_setxattr(&security,
						attr, ni, dir_ni, value,
						size, flags);
					/* never have to close dir_ni */
					if (res)
						res = -errno;
				} else
					res = -errno;
				if (attr != XATTR_NTFS_DOS_NAME) {
					if (!res)
						ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
					if (ntfs_inode_close(ni))
						set_fuse_error(&res);
				}
			} else
				res = -errno;
		}
#endif
		return (res);
	}
	if ((ctx->streams != NF_STREAMS_INTERFACE_XATTR)
	    && (ctx->streams != NF_STREAMS_INTERFACE_OPENXATTR))
		return -EOPNOTSUPP;
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE)
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
		/* security and trusted only settable by root */
	if (((namespace == XATTRNS_SECURITY)
	   || (namespace == XATTRNS_TRUSTED))
		&& security.uid)
		    return -EPERM;
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	switch (namespace) {
	case XATTRNS_SECURITY :
	case XATTRNS_TRUSTED :
		if (security.uid) {
			res = -EPERM;
			goto exit;
		}
		break;
	case XATTRNS_SYSTEM :
		if (!ntfs_allowed_as_owner(&security,ni)) {
			res = -EACCES;
			goto exit;
		}
		break;
	default :
		/* User xattr not allowed for symlinks, fifo, etc. */
		if (!user_xattrs_allowed(ctx, ni)) {
			res = -EPERM;
			goto exit;
		}
		if (!ntfs_allowed_access(&security,ni,S_IWRITE)) {
			res = -EACCES;
			goto exit;
		}
		break;
	}
#else
		/* User xattr not allowed for symlinks, fifo, etc. */
	if ((namespace == XATTRNS_USER)
	    && !user_xattrs_allowed(ctx, ni)) {
		res = -EPERM;
		goto exit;
	}
#endif
	lename_len = fix_xattr_prefix(name, namespace, &lename);
	if ((lename_len == -1)
	    || (ctx->windows_names
		&& ntfs_forbidden_chars(lename,lename_len,TRUE))) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
	if (na && flags == XATTR_CREATE) {
		res = -EEXIST;
		goto exit;
	}
	if (!na) {
		if (flags == XATTR_REPLACE) {
			res = -NTFS_NOXATTR_ERRNO;
			goto exit;
		}
		if (ntfs_attr_add(ni, AT_DATA, lename, lename_len, NULL, 0)) {
			res = -errno;
			goto exit;
		}
		if (!(ni->flags & FILE_ATTR_ARCHIVE)) {
			set_archive(ni);
			NInoFileNameSetDirty(ni);
		}
		na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
		if (!na) {
			res = -errno;
			goto exit;
		}
#if defined(__APPLE__) || defined(__DARWIN__)
	} else if (is_resource_fork) {
		/* In macOS, the resource fork is a special case. It doesn't
		 * ever shrink (it would have to be removed and re-added). */
#endif
	} else {
			/* currently compressed streams can only be wiped out */
		if (ntfs_attr_truncate(na, (s64)0 /* size */)) {
			res = -errno;
			goto exit;
		}
	}
	total = 0;
	res = 0;
	if (size) {
		do {
			part = ntfs_attr_pwrite(na, position + total,
					 size - total, &value[total]);
			if (part > 0)
				total += part;
		} while ((part > 0) && (total < size));
	}
	if ((total != size) || ntfs_attr_pclose(na))
		res = -errno;
	else {
		if (ctx->efs_raw
		   && (ni->flags & FILE_ATTR_ENCRYPTED)) {
			if (ntfs_efs_fixup_attribute(NULL,na))
				res = -errno;
		}
	}
	if (!res) {
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		if (!(ni->flags & FILE_ATTR_ARCHIVE)) {
			set_archive(ni);
			NInoFileNameSetDirty(ni);
		}
	}
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

static int ntfs_fuse_removexattr(const char *path, const char *name)
{
	ntfs_inode *ni;
	ntfs_inode *dir_ni;
	ntfschar *lename = NULL;
	int res = 0, lename_len;
	enum SYSTEMXATTRS attr;
	int namespace;
	struct SECURITY_CONTEXT security;

	attr = ntfs_xattr_system_type(name,ctx->vol);
	if (attr != XATTR_UNMAPPED) {
		switch (attr) {
			/*
			 * Removal of NTFS ACL, ATTRIB, EFSINFO or TIMES
			 * is never allowed
			 */
		case XATTR_NTFS_ACL :
		case XATTR_NTFS_ATTRIB :
		case XATTR_NTFS_ATTRIB_BE :
		case XATTR_NTFS_EFSINFO :
		case XATTR_NTFS_TIMES :
		case XATTR_NTFS_TIMES_BE :
		case XATTR_NTFS_CRTIME :
		case XATTR_NTFS_CRTIME_BE :
			res = -EPERM;
			break;
		default :
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * hijack internal data and ACL removal, whatever
			 * mode was selected for xattr (from the user's
			 * point of view, ACLs are not xattr)
			 * Note : ctime updated on successful settings
			 */
			ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
			if (ni) {
				if (ntfs_allowed_as_owner(&security,ni)) {
					if (attr == XATTR_NTFS_DOS_NAME)
						dir_ni = get_parent_dir(path);
					else
						dir_ni = (ntfs_inode*)NULL;
					res = ntfs_xattr_system_removexattr(&security,
						attr, ni, dir_ni);
					/* never have to close dir_ni */
					if (res)
						res = -errno;
				} else
					res = -errno;
				if (attr != XATTR_NTFS_DOS_NAME) {
					if (!res)
						ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
					if (ntfs_inode_close(ni))
						set_fuse_error(&res);
				}
			} else
				res = -errno;
#else
			/*
			 * Only hijack NTFS ACL setting if POSIX ACLS
			 * option is not selected
			 * Access control is partially done by fuse
			 */
			/* creation of a new name is not controlled by fuse */
			if (attr == XATTR_NTFS_DOS_NAME)
				ni = ntfs_check_access_xattr(&security,
							path, attr, TRUE);
			else {
				if (ntfs_fuse_is_named_data_stream(path)) {
					ni = (ntfs_inode*)NULL;
					errno = EINVAL; /* n/a for named data streams. */
				} else
					ni = ntfs_pathname_to_inode(ctx->vol,
							NULL, path);
			}
			if (ni) {
				/*
				 * user mapping is not mandatory
				 * if defined, only owner is allowed
				 */
				if (!ntfs_fuse_fill_security_context(&security)
				   || ntfs_allowed_as_owner(&security,ni)) {
					if (attr == XATTR_NTFS_DOS_NAME)
						dir_ni = get_parent_dir(path);
					else
						dir_ni = (ntfs_inode*)NULL;
					res = ntfs_xattr_system_removexattr(&security,
						attr, ni, dir_ni);
					/* never have to close dir_ni */
					if (res)
						res = -errno;
				} else
					res = -errno;
				if (attr != XATTR_NTFS_DOS_NAME) {
					if (!res)
						ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
					if (ntfs_inode_close(ni))
						set_fuse_error(&res);
				}
			} else
				res = -errno;
#endif
			break;
		}
		return (res);
	}
	if ((ctx->streams != NF_STREAMS_INTERFACE_XATTR)
	    && (ctx->streams != NF_STREAMS_INTERFACE_OPENXATTR))
		return -EOPNOTSUPP;
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE)
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
		/* security and trusted only settable by root */
	if (((namespace == XATTRNS_SECURITY)
	   || (namespace == XATTRNS_TRUSTED))
		&& security.uid)
		    return -EACCES;
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	switch (namespace) {
	case XATTRNS_SECURITY :
	case XATTRNS_TRUSTED :
		if (security.uid) {
			res = -EPERM;
			goto exit;
		}
		break;
	case XATTRNS_SYSTEM :
		if (!ntfs_allowed_as_owner(&security,ni)) {
			res = -EACCES;
			goto exit;
		}
		break;
	default :
		/* User xattr not allowed for symlinks, fifo, etc. */
		if (!user_xattrs_allowed(ctx, ni)) {
			res = -EPERM;
			goto exit;
		}
		if (!ntfs_allowed_access(&security,ni,S_IWRITE)) {
			res = -EACCES;
			goto exit;
		}
		break;
	}
#else
		/* User xattr not allowed for symlinks, fifo, etc. */
	if ((namespace == XATTRNS_USER)
	    && !user_xattrs_allowed(ctx, ni)) {
		res = -EPERM;
		goto exit;
	}
#endif
	lename_len = fix_xattr_prefix(name, namespace, &lename);
	if (lename_len == -1) {
		res = -errno;
		goto exit;
	}
	if (ntfs_attr_remove(ni, AT_DATA, lename, lename_len)) {
		if (errno == ENOENT)
			errno = NTFS_NOXATTR_ERRNO;
		res = -errno;
	}
	if (!res) {
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		if (!(ni->flags & FILE_ATTR_ARCHIVE)) {
			set_archive(ni);
			NInoFileNameSetDirty(ni);
		}
	}
exit:
	free(lename);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

static int ntfs_fuse_listxattr(const char *path, char *list, size_t size)
{
    ntfs_attr_search_ctx *actx = NULL;
    ntfs_inode *ni;
    int ret = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    struct SECURITY_CONTEXT security;
#endif
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    /* parent directory must be executable */
    if (ntfs_fuse_fill_security_context(&security)
        && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
            (ntfs_inode*)NULL,S_IEXEC)) {
        return (-errno);
            }
#endif
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;
    /* Return with no result for symlinks, fifo, etc. */
    if (!user_xattrs_allowed(ctx, ni))
        goto exit;
    /* otherwise file must be readable */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    if (!ntfs_allowed_access(&security,ni,S_IREAD)) {
        ret = -EACCES;
        goto exit;
    }
#endif
    actx = ntfs_attr_get_search_ctx(ni, NULL);
    if (!actx) {
        ret = -errno;
        goto exit;
    }

    if ((ctx->streams == NF_STREAMS_INTERFACE_XATTR)
        || (ctx->streams == NF_STREAMS_INTERFACE_OPENXATTR)) {
        ret = ntfs_fuse_listxattr_common(ni, actx, list, size,
                ctx->streams == NF_STREAMS_INTERFACE_XATTR);
        if (ret < 0)
            goto exit;
        }
    if (errno != ENOENT)
        ret = -errno;
    exit:
        if (actx)
            ntfs_attr_put_search_ctx(actx);
    if (ntfs_inode_close(ni))
        set_fuse_error(&ret);
    return ret;
}

static void *ntfs_init(struct fuse_conn_info *conn)
{
#if defined(__APPLE__) || defined(__DARWIN__)
    FUSE_ENABLE_XTIMES(conn);
#endif
#ifdef FUSE_CAP_DONT_MASK
    /* request umask not to be enforced by fuse */
    conn->want |= FUSE_CAP_DONT_MASK;
#endif /* defined FUSE_CAP_DONT_MASK */
#if POSIXACLS & KERNELACLS
    /* request ACLs to be checked by kernel */
    conn->want |= FUSE_CAP_POSIX_ACL;
#endif /* POSIXACLS & KERNELACLS */
#ifdef FUSE_CAP_BIG_WRITES
    if (ctx->big_writes
        && ((ctx->vol->nr_clusters << ctx->vol->cluster_size_bits)
            >= SAFE_CAPACITY_FOR_BIG_WRITES))
        conn->want |= FUSE_CAP_BIG_WRITES;
#endif
#ifdef FUSE_CAP_IOCTL_DIR
    conn->want |= FUSE_CAP_IOCTL_DIR;
#endif /* defined(FUSE_CAP_IOCTL_DIR) */
    return NULL;
}

static int ntfs_fuse_filler(ntfs_fuse_fill_context_t *fill_ctx, const ntfschar *name, const int name_len, const int name_type, const s64 pos __attribute__((unused)), const MFT_REF mref, const unsigned dt_type __attribute__((unused)))
{
	char *filename = NULL;
	int ret = 0;
	int filenamelen = -1;

	if (name_type == FILE_NAME_DOS)
		return 0;

	if ((filenamelen = ntfs_ucstombs(name, name_len, &filename, 0)) < 0) {
		C_LOG_WARNING("Filename decoding failed (inode %llu)",
				(unsigned long long)MREF(mref));
		return -1;
	}

	if (ntfs_fuse_is_named_data_stream(filename)) {
		ntfs_log_error("Unable to access '%s' (inode %llu) with "
				"current named streams access interface.\n",
				filename, (unsigned long long)MREF(mref));
		free(filename);
		return 0;
	} else {
		struct stat st = { .st_ino = MREF(mref) };
#ifndef DISABLE_PLUGINS
		ntfs_inode *ni;
#endif /* DISABLE_PLUGINS */

		switch (dt_type) {
		case NTFS_DT_DIR :
			st.st_mode = S_IFDIR | (0777 & ~ctx->dmask);
			break;
		case NTFS_DT_LNK :
			st.st_mode = S_IFLNK | 0777;
			break;
		case NTFS_DT_FIFO :
			st.st_mode = S_IFIFO;
			break;
		case NTFS_DT_SOCK :
			st.st_mode = S_IFSOCK;
			break;
		case NTFS_DT_BLK :
			st.st_mode = S_IFBLK;
			break;
		case NTFS_DT_CHR :
			st.st_mode = S_IFCHR;
			break;
		case NTFS_DT_REPARSE :
			st.st_mode = S_IFLNK | 0777; /* default */
#ifndef DISABLE_PLUGINS
			/* get emulated type from plugin if available */
			ni = ntfs_inode_open(ctx->vol, mref);
			if (ni && (ni->flags & FILE_ATTR_REPARSE_POINT)) {
				const plugin_operations_t *ops;
				REPARSE_POINT *reparse;
				int res;

				res = CALL_REPARSE_PLUGIN(ni, getattr, &st);
				if (!res)
					apply_umask(&st);
				else
					st.st_mode = S_IFLNK;
			}
			if (ni)
				ntfs_inode_close(ni);
#endif /* DISABLE_PLUGINS */
			break;
		default : /* unexpected types shown as plain files */
		case NTFS_DT_REG :
			st.st_mode = S_IFREG | (0777 & ~ctx->fmask);
			break;
		}

#if defined(__APPLE__) || defined(__DARWIN__)
		/*
		 * Returning file names larger than MAXNAMLEN (255) bytes
		 * causes Darwin/Mac OS X to bug out and skip the entry.
		 */
		if (filenamelen > MAXNAMLEN) {
			ntfs_log_debug("Truncating %d byte filename to "
				       "%d bytes.\n", filenamelen, MAXNAMLEN);
			ntfs_log_debug("  before: '%s'\n", filename);
			memset(filename + MAXNAMLEN, 0, filenamelen - MAXNAMLEN);
			ntfs_log_debug("   after: '%s'\n", filename);
		}
#elif defined(__sun) && defined (__SVR4)
		/*
		 * Returning file names larger than MAXNAMELEN (256) bytes
		 * causes Solaris/Illumos to return an I/O error from the system
		 * call.
		 * However we also need space for a terminating NULL, or user
		 * space tools will bug out since they expect a NULL terminator.
		 * Effectively the maximum length of a file name is MAXNAMELEN -
		 * 1 (255).
		 */
		if (filenamelen > (MAXNAMELEN - 1)) {
			ntfs_log_debug("Truncating %d byte filename to %d "
				"bytes.\n", filenamelen, MAXNAMELEN - 1);
			ntfs_log_debug("  before: '%s'\n", filename);
			memset(&filename[MAXNAMELEN - 1], 0,
				filenamelen - (MAXNAMELEN - 1));
			ntfs_log_debug("   after: '%s'\n", filename);
		}
#endif /* defined(__APPLE__) || defined(__DARWIN__), ... */

		ret = fill_ctx->filler(fill_ctx->buf, filename, &st, 0);
	}

	free(filename);
	return ret;
}

static void ntfs_close(void)
{
    struct SECURITY_CONTEXT security;

    if (!ctx)
        return;

    if (!ctx->vol)
        return;

    if (ctx->mounted) {
        C_LOG_VERB("start umount");
        if (ntfs_fuse_fill_security_context(&security)) {
            if (ctx->seccache && ctx->seccache->head.p_reads) {
                ntfs_log_info("Permissions cache : %lu writes, "
                "%lu reads, %lu.%1lu%% hits\n",
                  ctx->seccache->head.p_writes,
                  ctx->seccache->head.p_reads,
                  100 * ctx->seccache->head.p_hits
                     / ctx->seccache->head.p_reads,
                  1000 * ctx->seccache->head.p_hits
                     / ctx->seccache->head.p_reads % 10);
            }
        }
        ntfs_destroy_security_context(&security);
    }

    if (ntfs_umount(ctx->vol, FALSE)) {
        C_LOG_WARNING("UMOUNT ERROR");
    }

    ctx->vol = NULL;
}

static void setup_logging(char *parsed_options)
{
#if 0
    // fixme:// 这里应该去掉 daemon 这个操作
    if (!ctx->no_detach) {
        if (daemon(0, ctx->debug)) {
            C_LOG_WARNING("Failed to daemonize.");
        }
        else if (!ctx->debug) {
#ifndef DEBUG
            ntfs_log_set_handler(ntfs_log_handler_syslog);
            /* Override default libntfs identify. */
            openlog(EXEC_NAME, LOG_PID, LOG_DAEMON);
#endif
        }
    }
#endif

    ctx->seccache = (struct PERMISSIONS_CACHE*)NULL;
}

static int restore_privs(void)
{
    return 0;
}

void close_reparse_plugins(ntfs_fuse_context_t *ctx)
{
    while (ctx->plugins) {
        plugin_list_t *next;

        next = ctx->plugins->next;
        if (ctx->plugins->handle)
            dlclose(ctx->plugins->handle);
        free(ctx->plugins);
        ctx->plugins = next;
    }
}

static void mknod_dev_fuse(const char *dev)
{
    struct stat st;

    if (stat(dev, &st) && (errno == ENOENT)) {
        mode_t mask = umask(0);
        if (mknod(dev, S_IFCHR | 0666, makedev(10, 229))) {
            C_LOG_WARNING("Failed to create '%s'", dev);
        }
        umask(mask);
    }
}

static BOOL ntfs_fuse_fill_security_context(struct SECURITY_CONTEXT *scx)
{
    struct fuse_context *fusecontext;

    scx->vol = ctx->vol;
    scx->mapping[MAPUSERS] = ctx->security.mapping[MAPUSERS];
    scx->mapping[MAPGROUPS] = ctx->security.mapping[MAPGROUPS];
    scx->pseccache = &ctx->seccache;
    fusecontext = fuse_get_context();
    scx->uid = fusecontext->uid;
    scx->gid = fusecontext->gid;
    scx->tid = fusecontext->pid;
#ifdef FUSE_CAP_DONT_MASK
    /* the umask can be processed by the file system */
    scx->umask = fusecontext->umask;
#else
    /* the umask if forced by fuse on creation */
    scx->umask = 0;
#endif

    return (ctx->security.mapping[MAPUSERS] != (struct MAPPING*)NULL);
}

static void apply_umask(struct stat *stbuf)
{
    switch (stbuf->st_mode & S_IFMT) {
    case S_IFREG :
        stbuf->st_mode &= ~ctx->fmask;
        break;
    case S_IFDIR :
        stbuf->st_mode &= ~ctx->dmask;
        break;
    case S_IFLNK :
        stbuf->st_mode = (stbuf->st_mode & S_IFMT) | 0777;
        break;
    default :
        break;
    }
}

const struct plugin_operations *select_reparse_plugin(ntfs_fuse_context_t *ctx, ntfs_inode *ni, REPARSE_POINT **reparse_wanted)
{
    const struct plugin_operations *ops;
    void *handle;
    REPARSE_POINT *reparse;
    le32 tag, seltag;
    plugin_list_t *plugin;
    plugin_init_t pinit;

    ops = (struct plugin_operations*)NULL;
    reparse = ntfs_get_reparse_point(ni);
    if (reparse) {
        tag = reparse->reparse_tag;
        seltag = tag & IO_REPARSE_PLUGIN_SELECT;
        for (plugin=ctx->plugins; plugin && (plugin->tag != seltag);
                        plugin = plugin->next) { }
        if (plugin) {
            ops = plugin->ops;
        } else {
#ifdef PLUGIN_DIR
            char name[sizeof(PLUGIN_DIR) + 64];

            snprintf(name,sizeof(name), PLUGIN_DIR
                    "/ntfs-plugin-%08lx.so",
                    (long)le32_to_cpu(seltag));
#else
            char name[64];

            snprintf(name,sizeof(name), "ntfs-plugin-%08lx.so",
                    (long)le32_to_cpu(seltag));
#endif
            handle = dlopen(name, RTLD_LAZY);
            if (handle) {
                pinit = (plugin_init_t)dlsym(handle, "init");
                if (pinit) {
                    /* pinit() should set errno if it fails */
                    ops = (*pinit)(tag);
                    if (ops && register_reparse_plugin(ctx, seltag, ops, handle))
                        ops = (struct plugin_operations*)NULL;
                } else
                    errno = ELIBBAD;
                if (!ops)
                    dlclose(handle);
            } else {
                errno = ELIBACC;
                if (!(ctx->errors_logged & ERR_PLUGIN)) {
                    C_LOG_WARNING(
                        "Could not load plugin %s",
                        name);
                    ntfs_log_error("Hint %s\n",dlerror());
                }
                ctx->errors_logged |= ERR_PLUGIN;
            }
        }
        if (ops && reparse_wanted)
            *reparse_wanted = reparse;
        else
            free(reparse);
    }
    return (ops);
}

int register_reparse_plugin(ntfs_fuse_context_t *ctx, le32 tag, const plugin_operations_t *ops, void *handle)
{
    plugin_list_t *plugin;
    int res;

    res = -1;
    plugin = (plugin_list_t*)ntfs_malloc(sizeof(plugin_list_t));
    if (plugin) {
        plugin->tag = tag;
        plugin->ops = ops;
        plugin->handle = handle;
        plugin->next = ctx->plugins;
        ctx->plugins = plugin;
        res = 0;
    }
    return (res);
}

static int ntfs_fuse_is_named_data_stream(const char *path)
{
    if (strchr(path, ':') && ctx->streams == NF_STREAMS_INTERFACE_WINDOWS)
        return 1;
    return 0;
}

static void set_fuse_error(int *err)
{
    if (!*err)
        *err = -errno;
}

int ntfs_fuse_listxattr_common(ntfs_inode *ni, ntfs_attr_search_ctx *actx, char *list, size_t size, BOOL prefixing)
{
	int ret = 0;
	char *to = list;
#ifdef XATTR_MAPPINGS
	BOOL accepted;
	const struct XATTRMAPPING *item;
#endif /* XATTR_MAPPINGS */

    /* first list the regular user attributes (ADS) */
	while (!ntfs_attr_lookup(AT_DATA, NULL, 0, CASE_SENSITIVE, 0, NULL, 0, actx)) {
		char *tmp_name = NULL;
		int tmp_name_len;

		if (!actx->attr->name_length)
			continue;
		tmp_name_len = ntfs_ucstombs((ntfschar *)((u8*)actx->attr + le16_to_cpu(actx->attr->name_offset)), actx->attr->name_length, &tmp_name, 0);
		if (tmp_name_len < 0) {
			ret = -errno;
			goto exit;
		}
		/*
		 * When using name spaces, do not return
		 * security, trusted or system attributes
		 * (filtered elsewhere anyway)
		 * otherwise insert "user." prefix
		 */
		if (prefixing) {
			if ((strlen(tmp_name) > sizeof(xattr_ntfs_3g)) && !strncmp(tmp_name,xattr_ntfs_3g, sizeof(xattr_ntfs_3g)-1))
				tmp_name_len = 0;
			else
				ret += tmp_name_len + nf_ns_user_prefix_len + 1;
		}
	    else {
			ret += tmp_name_len + 1;
		}
		if (size && tmp_name_len) {
			if ((size_t)ret <= size) {
				if (prefixing) {
					strcpy(to, nf_ns_user_prefix);
					to += nf_ns_user_prefix_len;
				}
				strncpy(to, tmp_name, tmp_name_len);
				to += tmp_name_len;
				*to = 0;
				to++;
			}
		    else {
				free(tmp_name);
				ret = -ERANGE;
				goto exit;
			}
		}
		free(tmp_name);
	}

    /* List efs info xattr for encrypted files */
	if (ni->vol->efs_raw && (ni->flags & FILE_ATTR_ENCRYPTED)) {
		ret += sizeof(nf_ns_alt_xattr_efsinfo);
		if ((size_t)ret <= size) {
			memcpy(to, nf_ns_alt_xattr_efsinfo, sizeof(nf_ns_alt_xattr_efsinfo));
		    to += sizeof(nf_ns_alt_xattr_efsinfo);
		}
	}

exit:
	return (ret);
}

BOOL user_xattrs_allowed(ntfs_fuse_context_t *ctx __attribute__((unused)), ntfs_inode *ni)
{
	u32 dt_type;
	BOOL res;

	/* Quick return for common cases and root */
	if (!(ni->flags & (FILE_ATTR_SYSTEM | FILE_ATTR_REPARSE_POINT))
        || (ni->mft_no == FILE_root))
		res = TRUE;
	else {
		/* Reparse point depends on kind, see plugin */
		if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
		    struct stat stbuf;
		    REPARSE_POINT *reparse;
		    const plugin_operations_t *ops;

		    res = FALSE; /* default for error cases */
		    ops = select_reparse_plugin(ctx, ni, &reparse);
		    if (ops) {
		        if (ops->getattr
                    && !ops->getattr(ni,reparse,&stbuf)) {
		            res = S_ISREG(stbuf.st_mode)
                            || S_ISDIR(stbuf.st_mode);
                    }
		        free(reparse);
		    }
#else /* DISABLE_PLUGINS */
		    res = FALSE; /* mountpoints, symlinks, ... */
#endif /* DISABLE_PLUGINS */
		} else {
		    /* Metadata */
		    if (ni->mft_no < FILE_first_user)
		        res = FALSE;
		    else {
		        /* Interix types */
		        dt_type = ntfs_interix_types(ni);
		        res = (dt_type == NTFS_DT_REG)
                    || (dt_type == NTFS_DT_DIR);
		    }
		}
	}
	return (res);
}

static void ntfs_fuse_update_times(ntfs_inode *ni, ntfs_time_update_flags mask)
{
    if (ctx->atime == ATIME_DISABLED)
        mask &= ~NTFS_UPDATE_ATIME;
    else if (ctx->atime == ATIME_RELATIVE && mask == NTFS_UPDATE_ATIME &&
            (sle64_to_cpu(ni->last_access_time)
                >= sle64_to_cpu(ni->last_data_change_time)) &&
            (sle64_to_cpu(ni->last_access_time)
                >= sle64_to_cpu(ni->last_mft_change_time)))
        return;
    ntfs_inode_update_times(ni, mask);
}

static int fix_xattr_prefix(const char *name, int namespace, ntfschar **lename)
{
    int len;
    char *prefixed;

    *lename = (ntfschar*)NULL;
    switch (namespace) {
    case XATTRNS_USER :
        /*
         * user name space : remove user prefix
         */
            len = ntfs_mbstoucs(name + nf_ns_user_prefix_len, lename);
        break;
    case XATTRNS_SYSTEM :
    case XATTRNS_SECURITY :
    case XATTRNS_TRUSTED :
        /*
         * security, trusted and unmapped system name spaces :
         * insert ntfs-3g prefix
         */
        prefixed = ntfs_malloc(strlen(xattr_ntfs_3g)
             + strlen(name) + 1);
        if (prefixed) {
            strcpy(prefixed,xattr_ntfs_3g);
            strcat(prefixed,name);
            len = ntfs_mbstoucs(prefixed, lename);
            free(prefixed);
        } else
            len = -1;
        break;
    case XATTRNS_OPEN :
        /*
         * in open name space mode : do no fix prefix
         */
            len = ntfs_mbstoucs(name, lename);
        break;
    default :
        len = -1;
    }
    return (len);
}

static int xattr_namespace(const char *name)
{
    int namespace;

    if (ctx->streams == NF_STREAMS_INTERFACE_XATTR) {
        namespace = XATTRNS_NONE;
        if (!strncmp(name, nf_ns_user_prefix,
            nf_ns_user_prefix_len)
            && (strlen(name) != (size_t)nf_ns_user_prefix_len))
            namespace = XATTRNS_USER;
        else if (!strncmp(name, nf_ns_system_prefix,
            nf_ns_system_prefix_len)
            && (strlen(name) != (size_t)nf_ns_system_prefix_len))
            namespace = XATTRNS_SYSTEM;
        else if (!strncmp(name, nf_ns_security_prefix,
            nf_ns_security_prefix_len)
            && (strlen(name) != (size_t)nf_ns_security_prefix_len))
            namespace = XATTRNS_SECURITY;
        else if (!strncmp(name, nf_ns_trusted_prefix,
            nf_ns_trusted_prefix_len)
            && (strlen(name) != (size_t)nf_ns_trusted_prefix_len))
            namespace = XATTRNS_TRUSTED;
    } else
        namespace = XATTRNS_OPEN;
    return (namespace);
}

static ntfs_inode *get_parent_dir(const char *path)
{
    ntfs_inode *dir_ni;
    char *dirpath;
    char *p;

    dirpath = strdup(path);
    dir_ni = (ntfs_inode*)NULL;
    if (dirpath) {
        p = strrchr(dirpath,'/');
        if (p) {  /* always present, be safe */
            *p = 0;
            dir_ni = ntfs_pathname_to_inode(ctx->vol,
                        NULL, dirpath);
        }
        free(dirpath);
    } else
        errno = ENOMEM;
    return (dir_ni);
}

static ntfs_inode *ntfs_check_access_xattr(struct SECURITY_CONTEXT *security, const char *path, int attr, BOOL setting)
{
    ntfs_inode *ni;
    BOOL foracl;
    mode_t acctype;

    ni = (ntfs_inode*)NULL;
    if (ntfs_fuse_is_named_data_stream(path)) {
        errno = EINVAL; /* n/a for named data streams. */
    }
    else {
        foracl = (attr == XATTR_POSIX_ACC) || (attr == XATTR_POSIX_DEF);
        /*
         * When accessing Posix ACL, return unsupported if ACL
         * were disabled or no user mapping has been defined,
         * or trying to change a Windows-inherited ACL.
         * However no error will be returned to getfacl
         */
        if (((!ntfs_fuse_fill_security_context(security)
            || (ctx->secure_flags & ((1 << SECURITY_DEFAULT) | (1 << SECURITY_RAW))))
            || !(ctx->secure_flags & (1 << SECURITY_ACL)) || (setting && ctx->inherit)) && foracl) {
            if (ctx->silent && !ctx->security.mapping[MAPUSERS]) {
                errno = 0;
            }
            else {
                errno = EOPNOTSUPP;
            }
            }
        else {
                /*
                 * parent directory must be executable, and
                 * for setting a DOS name it must be writeable
                 */
                if (setting && (attr == XATTR_NTFS_DOS_NAME)) {
                    acctype = S_IEXEC | S_IWRITE;
                }
                else {
                    acctype = S_IEXEC;
                }
                if ((attr == XATTR_NTFS_DOS_NAME) && !strcmp(path,"/")) {
                    /* forbid getting/setting names on root */
                    errno = EPERM;
                }
            else {
                if (ntfs_allowed_real_dir_access(security, path, (ntfs_inode*)NULL ,acctype)) {
                    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
                }
            }
        }
    }
    return (ni);
}

static int ntfs_allowed_real_dir_access(struct SECURITY_CONTEXT *scx, const char *path, ntfs_inode *dir_ni, mode_t accesstype)
{
    int allowed;
    ntfs_inode *dir_ni2;
    char *dirpath;
    char *name;

    if (dir_ni)
        allowed = ntfs_real_allowed_access(scx, dir_ni, accesstype);
    else {
        allowed = 0;
        dirpath = strdup(path);
        if (dirpath) {
            /* the root of file system is seen as a parent of itself */
            /* is that correct ? */
            name = strrchr(dirpath, '/');
            *name = 0;
            dir_ni2 = ntfs_pathname_to_inode(scx->vol, NULL,
                    dirpath);
            if (dir_ni2) {
                allowed = ntfs_real_allowed_access(scx,
                    dir_ni2, accesstype);
                if (ntfs_inode_close(dir_ni2))
                    allowed = 0;
            }
            free(dirpath);
        }
    }
    return (allowed);
}

static int ntfs_fuse_getxattr_windows(const char *path, const char *name, char *value, size_t size)
{
    ntfs_attr_search_ctx *actx = NULL;
    ntfs_inode *ni;
    char *to = value;
    int ret = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    struct SECURITY_CONTEXT security;
#endif

    if (strcmp(name, "ntfs.streams.list"))
        return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    /* parent directory must be executable */
    if (ntfs_fuse_fill_security_context(&security)
        && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
            (ntfs_inode*)NULL,S_IEXEC)) {
        return (-errno);
            }
#endif
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    if (!ntfs_allowed_access(&security,ni,S_IREAD)) {
        ret = -errno;
        goto exit;
    }
#endif
    actx = ntfs_attr_get_search_ctx(ni, NULL);
    if (!actx) {
        ret = -errno;
        goto exit;
    }
    while (!ntfs_attr_lookup(AT_DATA, NULL, 0, CASE_SENSITIVE,
                0, NULL, 0, actx)) {
        char *tmp_name = NULL;
        int tmp_name_len;

        if (!actx->attr->name_length)
            continue;
        tmp_name_len = ntfs_ucstombs((ntfschar *)((u8*)actx->attr +
                le16_to_cpu(actx->attr->name_offset)),
                actx->attr->name_length, &tmp_name, 0);
        if (tmp_name_len < 0) {
            ret = -errno;
            goto exit;
        }
        if (ret)
            ret++; /* For space delimiter. */
        ret += tmp_name_len;
        if (size) {
            if ((size_t)ret <= size) {
                /* Don't add space to the beginning of line. */
                if (to != value) {
                    *to = '\0';
                    to++;
                }
                strncpy(to, tmp_name, tmp_name_len);
                to += tmp_name_len;
            } else {
                free(tmp_name);
                ret = -ERANGE;
                goto exit;
            }
        }
        free(tmp_name);
                }
    if (errno != ENOENT)
        ret = -errno;
    exit:
        if (actx)
            ntfs_attr_put_search_ctx(actx);
    if (ntfs_inode_close(ni))
        set_fuse_error(&ret);
    return ret;
}

static int ntfs_fuse_rm(const char *org_path)
{
    char *name;
    ntfschar *uname = NULL;
    ntfs_inode *dir_ni = NULL, *ni;
    char *path;
    int res = 0, uname_len;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    struct SECURITY_CONTEXT security;
#endif

    path = strdup(org_path);
    if (!path)
        return -errno;
    /* Open object for delete. */
    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni) {
        res = -errno;
        goto exit;
    }
    /* deny unlinking metadata files */
    if (ni->mft_no < FILE_first_user) {
        errno = EPERM;
        res = -errno;
        goto exit;
    }

    /* Generate unicode filename. */
    name = strrchr(path, '/');
    name++;
    uname_len = ntfs_mbstoucs(name, &uname);
    if (uname_len < 0) {
        res = -errno;
        goto exit;
    }
    /* Open parent directory. */
    *--name = 0;
    dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    /* deny unlinking metadata files from $Extend */
    if (!dir_ni || (dir_ni->mft_no == FILE_Extend)) {
        res = -errno;
        if (dir_ni)
            res = -EPERM;
        goto exit;
    }

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    /* JPA deny unlinking if directory is not writable and executable */
    if (!ntfs_fuse_fill_security_context(&security)
        || ntfs_allowed_dir_access(&security, org_path, dir_ni, ni,
                   S_IEXEC + S_IWRITE + S_ISVTX)) {
#endif
        if (dir_ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
            const plugin_operations_t *ops;
            REPARSE_POINT *reparse;

            res = CALL_REPARSE_PLUGIN(dir_ni, unlink,
                    org_path, ni, uname, uname_len);
#else /* DISABLE_PLUGINS */
            res = -EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
        } else
            if (ntfs_delete(ctx->vol, org_path, ni, dir_ni,
                     uname, uname_len))
                res = -errno;
        /* ntfs_delete() always closes ni and dir_ni */
        ni = dir_ni = NULL;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
                   } else
                       res = -EACCES;
#endif
    exit:
        if (ntfs_inode_close(dir_ni))
            set_fuse_error(&res);
    if (ntfs_inode_close(ni))
        set_fuse_error(&res);
    free(uname);
    free(path);
    return res;
}

static int ntfs_fuse_create(const char *org_path, mode_t typemode, dev_t dev, const char *target, struct fuse_file_info *fi)
{
	char *name;
	ntfschar *uname = NULL, *utarget = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *dir_path;
	le32 securid;
	char *path = NULL;
	gid_t gid;
	mode_t dsetgid;
	ntfschar *stream_name;
	int stream_name_len;
	mode_t type = typemode & ~07777;
	mode_t perm;
	struct SECURITY_CONTEXT security;
	int res = 0, uname_len, utarget_len;

	dir_path = strdup(org_path);
	if (!dir_path)
		return -errno;
	/* Generate unicode filename. */
	name = strrchr(dir_path, '/');
	name++;
	uname_len = ntfs_mbstoucs(name, &uname);
	if ((uname_len < 0)
	    || (ctx->windows_names
		&& ntfs_forbidden_names(ctx->vol,uname,uname_len,TRUE))) {
		res = -errno;
		goto exit;
	}
	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
		/* stream name validity has been checked previously */
	if (stream_name_len < 0) {
		res = stream_name_len;
		goto exit;
	}
	/* Open parent directory. */
	*--name = 0;
	dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, dir_path);
		/* Deny creating files in $Extend */
	if (!dir_ni || (dir_ni->mft_no == FILE_Extend)) {
		free(path);
		res = -errno;
		if (dir_ni)
			res = -EPERM;
		goto exit;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		/* make sure parent directory is writeable and executable */
	if (!ntfs_fuse_fill_security_context(&security)
	       || ntfs_allowed_create(&security,
				dir_ni, &gid, &dsetgid)) {
#else
		ntfs_fuse_fill_security_context(&security);
		ntfs_allowed_create(&security, dir_ni, &gid, &dsetgid);
#endif
		if (S_ISDIR(type))
			perm = (typemode & ~ctx->dmask & 0777)
				| (dsetgid & S_ISGID);
		else
			if ((ctx->special_files == NTFS_FILES_WSL)
			    && S_ISLNK(type))
				perm = typemode | 0777;
			else
				perm = typemode & ~ctx->fmask & 0777;
			/*
			 * Try to get a security id available for
			 * file creation (from inheritance or argument).
			 * This is not possible for NTFS 1.x, and we will
			 * have to build a security attribute later.
			 */
		if (!ctx->security.mapping[MAPUSERS])
			securid = const_cpu_to_le32(0);
		else
			if (ctx->inherit)
				securid = ntfs_inherited_id(&security,
					dir_ni, S_ISDIR(type));
			else
#if POSIXACLS
				securid = ntfs_alloc_securid(&security,
					security.uid, gid,
					dir_ni, perm, S_ISDIR(type));
#else
				securid = ntfs_alloc_securid(&security,
					security.uid, gid,
					perm & ~security.umask, S_ISDIR(type));
#endif
		/* Create object specified in @type. */
		if (dir_ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
			const plugin_operations_t *ops;
			REPARSE_POINT *reparse;

			reparse = (REPARSE_POINT*)NULL;
			ops = select_reparse_plugin(ctx, dir_ni, &reparse);
			if (ops && ops->create) {
				ni = (*ops->create)(dir_ni, reparse,
					securid, uname, uname_len, type);
			} else {
				ni = (ntfs_inode*)NULL;
				errno = EOPNOTSUPP;
			}
			free(reparse);
#else /* DISABLE_PLUGINS */
			errno = EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
		} else {
			switch (type) {
				case S_IFCHR:
				case S_IFBLK:
					ni = ntfs_create_device(dir_ni, securid,
						uname, uname_len, type,	dev);
					break;
				case S_IFLNK:
					utarget_len = ntfs_mbstoucs(target,
							&utarget);
					if (utarget_len < 0) {
						res = -errno;
						goto exit;
					}
					ni = ntfs_create_symlink(dir_ni,
						securid, uname, uname_len,
						utarget, utarget_len);
					break;
				default:
					ni = ntfs_create(dir_ni, securid,
						uname, uname_len, type);
					break;
			}
		}
		if (ni) {
				/*
				 * set the security attribute if a security id
				 * could not be allocated (eg NTFS 1.x)
				 */
			if (ctx->security.mapping[MAPUSERS]) {
#if POSIXACLS
			   	if (!securid
				   && ntfs_set_inherited_posix(&security, ni,
					security.uid, gid,
					dir_ni, perm) < 0)
					set_fuse_error(&res);
#else
			   	if (!securid
				   && ntfs_set_owner_mode(&security, ni,
					security.uid, gid,
					perm & ~security.umask) < 0)
					set_fuse_error(&res);
#endif
			}
			set_archive(ni);
			/* mark a need to compress the end of file */
			if (fi && (ni->flags & FILE_ATTR_COMPRESSED)) {
				fi->fh |= CLOSE_COMPRESSED;
			}
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* mark a future need to fixup encrypted inode */
			if (fi
			    && ctx->efs_raw
			    && (ni->flags & FILE_ATTR_ENCRYPTED))
				fi->fh |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
			/* mark a need to update the mtime */
			if (fi && ctx->dmtime)
				fi->fh |= CLOSE_DMTIME;
			NInoSetDirty(ni);
			/*
			 * closing ni requires access to dir_ni to
			 * synchronize the index, avoid double opening.
			 */
			if (ntfs_inode_close_in_dir(ni, dir_ni))
				set_fuse_error(&res);
			ntfs_fuse_update_times(dir_ni, NTFS_UPDATE_MCTIME);
		} else
			res = -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	} else
		res = -errno;
#endif
	free(path);

exit:
	free(uname);
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
	if (utarget)
		free(utarget);
	free(dir_path);
	return res;
}

static int ntfs_fuse_parse_path(const char *org_path, char **path, ntfschar **stream_name)
{
    char *stream_name_mbs;
    int res;

    stream_name_mbs = strdup(org_path);
    if (!stream_name_mbs)
        return -errno;
    if (ctx->streams == NF_STREAMS_INTERFACE_WINDOWS) {
        *path = strsep(&stream_name_mbs, ":");
        if (stream_name_mbs) {
            *stream_name = NULL;
            res = ntfs_mbstoucs(stream_name_mbs, stream_name);
            if (res < 0) {
                free(*path);
                *path = NULL;
                return -errno;
            }
            return res;
        }
    } else
        *path = stream_name_mbs;
    *stream_name = AT_UNNAMED;
    return 0;
}

static int ntfs_fuse_rename_existing_dest(const char *old_path, const char *new_path)
{
    int ret, len;
    char *tmp;
    const char *ext = ".ntfs-3g-";
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
    struct SECURITY_CONTEXT security;
#endif

    ntfs_log_trace("Entering\n");

    len = strlen(new_path) + strlen(ext) + 10 + 1; /* wc(str(2^32)) + \0 */
    tmp = ntfs_malloc(len);
    if (!tmp)
        return -errno;

    ret = snprintf(tmp, len, "%s%s%010d", new_path, ext, ++ntfs_sequence);
    if (ret != len - 1) {
        ntfs_log_error("snprintf failed: %d != %d\n", ret, len - 1);
        ret = -EOVERFLOW;
    } else {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
        /*
         * Make sure existing dest can be removed.
         * This is only needed if parent directory is
         * sticky, because in this situation condition
         * for unlinking is different from condition for
         * linking
         */
        if (!ntfs_fuse_fill_security_context(&security)
          || ntfs_allowed_dir_access(&security, new_path,
                (ntfs_inode*)NULL, (ntfs_inode*)NULL,
                S_IEXEC + S_IWRITE + S_ISVTX))
            ret = ntfs_fuse_safe_rename(old_path, new_path, tmp);
        else
            ret = -EACCES;
#else
        ret = ntfs_fuse_safe_rename(old_path, new_path, tmp);
#endif
    }
    free(tmp);
    return 	ret;
}

static int ntfs_fuse_safe_rename(const char *old_path, const char *new_path, const char *tmp)
{
    int ret;

    ntfs_log_trace("Entering\n");

    ret = ntfs_fuse_link(new_path, tmp);
    if (ret)
        return ret;

    ret = ntfs_fuse_unlink(new_path);
    if (!ret) {

        ret = ntfs_fuse_link(old_path, new_path);
        if (ret)
            goto restore;

        ret = ntfs_fuse_unlink(old_path);
        if (ret) {
            if (ntfs_fuse_unlink(new_path))
                goto err;
            goto restore;
        }
    }

    goto cleanup;
    restore:
        if (ntfs_fuse_link(tmp, new_path)) {
            err:
                    C_LOG_WARNING("Rename failed. Existing file '%s' was renamed "
                            "to '%s'", new_path, tmp);
        } else {
            cleanup:
                    /*
                     * Condition for this unlink has already been checked in
                     * "ntfs_fuse_rename_existing_dest()", so it should never
                     * fail (unless concurrent access to directories when fuse
                     * is multithreaded)
                     */
                    if (ntfs_fuse_unlink(tmp) < 0)
                        C_LOG_WARNING("Rename failed. Existing file '%s' still present as '%s'", new_path, tmp);
        }
    return 	ret;
}

static int ntfs_fuse_rm_stream(const char *path, ntfschar *stream_name, const int stream_name_len)
{
    ntfs_inode *ni;
    int res = 0;

    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni)
        return -errno;

    if (ntfs_attr_remove(ni, AT_DATA, stream_name, stream_name_len))
        res = -errno;

    if (ntfs_inode_close(ni))
        set_fuse_error(&res);
    return res;
}

static int ntfs_fuse_mknod_common(const char *org_path, mode_t mode, dev_t dev, struct fuse_file_info *fi)
{
    char *path = NULL;
    ntfschar *stream_name;
    int stream_name_len;
    int res = 0;

    stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
    if (stream_name_len < 0)
        return stream_name_len;
    if (stream_name_len
        && (!S_ISREG(mode)
        || (ctx->windows_names
            && ntfs_forbidden_names(ctx->vol,stream_name,
                    stream_name_len, TRUE)))) {
        res = -EINVAL;
        goto exit;
                    }
    if (!stream_name_len)
        res = ntfs_fuse_create(path, mode & (S_IFMT | 07777), dev,
                    NULL,fi);
    else
        res = ntfs_fuse_create_stream(path, stream_name, stream_name_len,fi);
    exit:
        free(path);
    if (stream_name_len)
        free(stream_name);
    return res;
}

static int ntfs_fuse_create_stream(const char *path, ntfschar *stream_name, const int stream_name_len, struct fuse_file_info *fi)
{
    ntfs_inode *ni;
    int res = 0;

    ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
    if (!ni) {
        res = -errno;
        if (res == -ENOENT) {
            /*
             * If such file does not exist, create it and try once
             * again to add stream to it.
             * Note : no fuse_file_info for creation of main file
             */
            res = ntfs_fuse_create(path, S_IFREG, 0, NULL,
                    (struct fuse_file_info*)NULL);
            if (!res)
                return ntfs_fuse_create_stream(path,
                        stream_name, stream_name_len,fi);
            else
                res = -errno;
        }
        return res;
    }
    if (ntfs_attr_add(ni, AT_DATA, stream_name, stream_name_len, NULL, 0))
        res = -errno;
    else
        set_archive(ni);

    if ((res >= 0)
        && fi
        && (fi->flags & (O_WRONLY | O_RDWR))) {
        /* mark a future need to compress the last block */
        if (ni->flags & FILE_ATTR_COMPRESSED)
            fi->fh |= CLOSE_COMPRESSED;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
        /* mark a future need to fixup encrypted inode */
        if (ctx->efs_raw
            && (ni->flags & FILE_ATTR_ENCRYPTED))
            fi->fh |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
        if (ctx->dmtime)
            fi->fh |= CLOSE_DMTIME;
        }

    if (ntfs_inode_close(ni))
        set_fuse_error(&res);
    return res;
}

static int ntfs_fuse_trunc(const char *org_path, off_t size,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			BOOL chkwrite)
#else
			BOOL chkwrite __attribute__((unused)))
#endif
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	int res;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	s64 oldsize;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		goto exit;
	/* deny truncating metadata files */
	if (ni->mft_no < FILE_first_user) {
		errno = EPERM;
		goto exit;
	}

	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
#ifndef DISABLE_PLUGINS
		const plugin_operations_t *ops;
		REPARSE_POINT *reparse;

		if (stream_name_len) {
			res = -EINVAL;
			goto exit;
		}
		res = CALL_REPARSE_PLUGIN(ni, truncate, size);
		if (!res) {
			set_archive(ni);
			goto stamps;
		}
#else /* DISABLE_PLUGINS */
		res = -EOPNOTSUPP;
#endif /* DISABLE_PLUGINS */
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na)
		goto exit;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	/*
	 * JPA deny truncation if cannot search in parent directory
	 * or cannot write to file (already checked for ftruncate())
	 */
	if (ntfs_fuse_fill_security_context(&security)
		&& (!ntfs_allowed_dir_access(&security, path,
			 (ntfs_inode*)NULL, ni, S_IEXEC)
	          || (chkwrite
		     && !ntfs_allowed_access(&security, ni, S_IWRITE)))) {
		errno = EACCES;
		goto exit;
	}
#endif
		/*
		 * For compressed files, upsizing is done by inserting a final
		 * zero, which is optimized as creating a hole when possible.
		 */
	oldsize = na->data_size;
	if ((na->data_flags & ATTR_COMPRESSION_MASK)
	    && (size > na->initialized_size)) {
		char zero = 0;
		if (ntfs_attr_pwrite(na, size - 1, 1, &zero) <= 0)
			goto exit;
	} else
		if (ntfs_attr_truncate(na, size))
			goto exit;
	if (oldsize != size)
		set_archive(ni);

#ifndef DISABLE_PLUGINS
stamps:
#endif /* DISABLE_PLUGINS */
	ntfs_fuse_update_times(ni, NTFS_UPDATE_MCTIME);
	errno = 0;
exit:
	res = -errno;
	ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static struct fuse_chan *try_fuse_mount(const char* mountPoint, char *parsed_options)
{
    struct fuse_chan *fc = NULL;
    struct fuse_args margs = FUSE_ARGS_INIT(0, NULL);

    /* The fuse_mount() options get modified, so we always rebuild it */
    if (fuse_opt_add_arg(&margs, "sandbox") == -1 || fuse_opt_add_arg(&margs, "-o") == -1 || fuse_opt_add_arg(&margs, parsed_options) == -1) {
        ntfs_log_error("Failed to set FUSE options.\n");
        goto free_args;
    }

    fc = fuse_mount(mountPoint, &margs);

free_args:
    fuse_opt_free_args(&margs);

    return fc;
}

int ntfs_strappend(char **dest, const char *append)
{
    char *p;
    size_t size_append, size_dest = 0;

    if (!dest)
        return -1;
    if (!append)
        return 0;

    size_append = strlen(append);
    if (*dest)
        size_dest = strlen(*dest);

    if (strappend_is_large(size_dest) || strappend_is_large(size_append)) {
        errno = EOVERFLOW;
        C_LOG_WARNING("Too large input buffer");
        return -1;
    }

    p = (char*)realloc(*dest, size_dest + size_append + 1);
    if (!p) {
        C_LOG_WARNING("Memory realloction failed");
        return -1;
    }

    *dest = p;
    strcpy(*dest + size_dest, append);

    return 0;
}

static s64 ntfs_get_nr_free_mft_records(ntfs_volume *vol)
{
    ntfs_attr *na = vol->mftbmp_na;
    s64 nr_free = ntfs_attr_get_free_bits(na);

    if (nr_free >= 0)
        nr_free += (na->allocated_size - na->data_size) << 3;
    return nr_free;
}

static gpointer mount_fs_thread (gpointer data)
{
    g_return_val_if_fail(data, NULL);

    SandboxFs* sf = (SandboxFs*) data;

    int                     err = 0;
    struct stat             sbuf;
    bool                    hasErr = false;
    unsigned long           existing_mount;
    const char*             failed_secure = NULL;
    const char*             permissions_mode = NULL;
#if !(defined(__sun) && defined (__SVR4))
    fuse_fstype             fstype = FSTYPE_UNKNOWN;
#endif
    char*                   parsed_options = g_strdup_printf("allow_other,nonempty,relatime,fsname=%s", sf->dev);

    SANDBOX_FS_MUTEX_LOCK();

    // 创建新的进程/线程，执行挂载操作
    if (ntfs_fuse_init()) {
        err = NTFS_VOLUME_OUT_OF_MEMORY;
        C_LOG_WARNING("fuse_init error!");
        hasErr = true;
        goto err2;
    }

    // check is mounted
    if (!ntfs_check_if_mounted(sf->dev, &existing_mount) && (existing_mount & NTFS_MF_MOUNTED) && (!(existing_mount & NTFS_MF_READONLY) || !ctx->ro)) {
        err = NTFS_VOLUME_LOCKED;
        hasErr = true;
        goto err_out;
    }

    if (sf->dev[0] != '/') {
        C_LOG_WARNING("Mount point '%s' is not absolute path.", sf->dev);
        hasErr = true;
        goto err_out;
    }

    ctx->abs_mnt_point = strdup(sf->dev);
    if (!ctx->abs_mnt_point) {
        C_LOG_ERROR("strdup failed");
        hasErr = true;
        goto err_out;
    }

    ctx->security.uid = 0;
    ctx->security.gid = 0;
    if (!stat(sf->dev, &sbuf)) {
        /* collect owner of mount point, useful for default mapping */
        ctx->security.uid = sbuf.st_uid;
        ctx->security.gid = sbuf.st_gid;
   }

#if defined(linux) || defined(__uClinux__)
    fstype = get_fuse_fstype();

    err = NTFS_VOLUME_NO_PRIVILEGE;
    if (restore_privs()) {
        hasErr = true;
        goto err_out;
    }

    if (fstype == FSTYPE_NONE || fstype == FSTYPE_UNKNOWN) {
        fstype = load_fuse_module();
    }
    create_dev_fuse();

    if (drop_privs()) {
        hasErr = true;
        goto err_out;
    }
#endif

    if (stat(sf->dev, &sbuf)) {
        C_LOG_WARNING("Failed to access '%s'", sf->dev);
        err = NTFS_VOLUME_NO_PRIVILEGE;
        hasErr = true;
        goto err_out;
    }

#if !(defined(__sun) && defined (__SVR4))
    /* Always use fuseblk for block devices unless it's surely missing. */
    if (S_ISBLK(sbuf.st_mode) && (fstype != FSTYPE_FUSE)) {
        ctx->blkdev = TRUE;
    }
#endif

#ifndef FUSE_INTERNAL
    if (getuid() && ctx->blkdev) {
        ntfs_log_error("%s", unpriv_fuseblk_msg);
        err = NTFS_VOLUME_NO_PRIVILEGE;
        goto err2;
    }
#endif

    err = ntfs_open(sf->dev);
    if (err) {
        hasErr = true;
        goto err_out;
    }

    /* Force read-only mount if the device was found read-only */
    if (!ctx->ro && NVolReadOnly(ctx->vol)) {
        ctx->rw = FALSE;
        ctx->ro = TRUE;
        if (ntfs_strinsert(&parsed_options, ",ro")) {
            hasErr = true;
            goto err_out;
        }
        C_LOG_VERB("Could not mount read-write, trying read-only");
    }
    else {
        if (ctx->rw && ntfs_strinsert(&parsed_options, ",rw")) {
            hasErr = true;
            goto err_out;
        }
    }
    /* We must do this after ntfs_open() to be able to set the blksize */
    if (ctx->blkdev && set_fuseblk_options(&parsed_options)) {
        hasErr = true;
        goto err_out;
    }

    ctx->vol->abs_mnt_point = ctx->abs_mnt_point;
    ctx->security.vol = ctx->vol;
    ctx->vol->secure_flags = ctx->secure_flags;
    ctx->vol->special_files = ctx->special_files;

#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	ctx->vol->efs_raw = ctx->efs_raw;
#endif /* HAVE_SETXATTR */
	if (!ntfs_build_mapping(&ctx->security,ctx->usermap_path, (ctx->vol->secure_flags & ((1 << SECURITY_DEFAULT) | (1 << SECURITY_ACL))) && !ctx->inherit && !(ctx->vol->secure_flags & (1 << SECURITY_WANTED)))) {
#if POSIXACLS
		/* use basic permissions if requested */
		if (ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))
			permissions_mode = "User mapping built, Posix ACLs not used";
		else {
			permissions_mode = "User mapping built, Posix ACLs in use";
#if KERNELACLS
			if (ntfs_strinsert(&parsed_options, ",default_permissions,acl")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
				goto err_out;
			}
#endif /* KERNELACLS */
		}
#else /* POSIXACLS */
#if KERNELPERMS
		if (!(ctx->vol->secure_flags & ((1 << SECURITY_DEFAULT) | (1 << SECURITY_ACL)))) {
			/*
			 * No explicit option but user mapping found
			 * force default security
			 */
			ctx->vol->secure_flags |= (1 << SECURITY_DEFAULT);
			if (ntfs_strinsert(&parsed_options, ",default_permissions")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
			    hasErr = true;
				goto err_out;
			}
		}
#endif /* KERNELPERMS */
		permissions_mode = "User mapping built";
#endif /* POSIXACLS */
		ctx->dmask = ctx->fmask = 0;
	}
    else {
		ctx->security.uid = ctx->uid;
		ctx->security.gid = ctx->gid;
		/* same ownership/permissions for all files */
		ctx->security.mapping[MAPUSERS] = (struct MAPPING*)NULL;
		ctx->security.mapping[MAPGROUPS] = (struct MAPPING*)NULL;
		if ((ctx->vol->secure_flags & (1 << SECURITY_WANTED)) && !(ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))) {
			ctx->vol->secure_flags |= (1 << SECURITY_DEFAULT);
			if (ntfs_strinsert(&parsed_options, ",default_permissions")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
			    hasErr = true;
				goto err_out;
			}
		}
		if (ctx->vol->secure_flags & (1 << SECURITY_DEFAULT)) {
			ctx->vol->secure_flags |= (1 << SECURITY_RAW);
			permissions_mode = "Global ownership and permissions enforced";
		}
        else {
			ctx->vol->secure_flags &= ~(1 << SECURITY_RAW);
			permissions_mode = "Ownership and permissions disabled";
		}
	}
	if (ctx->usermap_path) {
		free (ctx->usermap_path);
	}

#if defined(HAVE_SETXATTR) && defined(XATTR_MAPPINGS)
	xattr_mapping = ntfs_xattr_build_mapping(ctx->vol, ctx->xattrmap_path);
	ctx->vol->xattr_mapping = xattr_mapping;
	/*
	 * Errors are logged, do not refuse mounting, it would be
	 * too difficult to fix the unmountable mapping file.
	 */
	if (ctx->xattrmap_path) {
		free(ctx->xattrmap_path);
	}
#endif /* defined(HAVE_SETXATTR) && defined(XATTR_MAPPINGS) */

#ifndef DISABLE_PLUGINS
	register_internal_reparse_plugins();
#endif /* DISABLE_PLUGINS */

    C_LOG_VERB("parsed options: %s", parsed_options ? parsed_options : "null");
	gsFuse = mount_fuse(parsed_options, sf->mountPoint);
	if (!gsFuse) {
		err = NTFS_VOLUME_FUSE_ERROR;
	    hasErr = true;
		goto err_out;
	}

	ctx->mounted = TRUE;
    sf->isMounted = true;
    C_LOG_VERB("mounted!");

#if defined(linux) || defined(__uClinux__)
	if (S_ISBLK(sbuf.st_mode) && (fstype == FSTYPE_FUSE)) {
	    C_LOG_VERB("fuse");
	}
#endif
	setup_logging(parsed_options);
	if (failed_secure) {
	    C_LOG_WARNING("%s",failed_secure);
	}
	if (permissions_mode) {
	    C_LOG_WARNING("%s, configuration type %d", permissions_mode, 4 + POSIXACLS*6 - KERNELPERMS*3 + CACHEING);
	}
	if ((ctx->vol->secure_flags & (1 << SECURITY_RAW)) && !ctx->uid && ctx->gid) {
		C_LOG_WARNING("Warning : using problematic uid==0 and gid!=0");
	}

    sf->isMounted = true;

    SANDBOX_FS_MUTEX_UNLOCK();

	fuse_loop(gsFuse);

    SANDBOX_FS_MUTEX_LOCK();

    sf->isMounted = false;

	err = 0;

	fuse_unmount(sf->dev, ctx->fc);

	fuse_destroy(gsFuse);

err_out:
	ntfs_mount_error(sf->dev, sf->mountPoint, err);
	if (ctx->abs_mnt_point) {
		free(ctx->abs_mnt_point);
	}
#if defined(HAVE_SETXATTR) && defined(XATTR_MAPPINGS)
	ntfs_xattr_free_mapping(xattr_mapping);
#endif /* defined(HAVE_SETXATTR) && defined(XATTR_MAPPINGS) */
err2:
	ntfs_close();
#ifndef DISABLE_PLUGINS
	close_reparse_plugins(ctx);
#endif /* DISABLE_PLUGINS */

	free(ctx);
    if (parsed_options) {
	    free(parsed_options);
    }

    return NULL;
}

static void umount_signal_process (int signum)
{
    sandbox_fs_unmount();
}
