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

#include "../3thrd/fs/param.h"
#include "../3thrd/fs/security.h"
#include "../3thrd/fs/attrib.h"
#include "../3thrd/fs/bitmap.h"
#include "../3thrd/fs/bootsect.h"
#include "../3thrd/fs/device.h"
#include "../3thrd/fs/dir.h"
#include "../3thrd/fs/mft.h"
#include "../3thrd/fs/mst.h"
#include "../3thrd/fs/runlist.h"
#include "./fs/utils.h"
#include "../3thrd/fs/ntfstime.h"
#include "./fs/sd.h"
#include "./fs/boot.h"
#include "./fs/attrdef.h"
/* #include "version.h" */
#include "../3thrd/fs/logging.h"
#include "../3thrd/fs/support.h"
#include "../3thrd/fs/unistr.h"
#include "../3thrd/fs/misc.h"
#include "c/clib.h"
#include "../3thrd/fs/types.h"

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
#define NTFS_PAGE_SIZE              4096

#define SANDBOX_FS_MAGIC            0x18A4
#define SANDBOX_FS_MAGIC_ID         0x5246

#define SANDBOX_VERSION_NEW         0x0001

// check --start
#define RETURN_FS_ERRORS_CORRECTED (1)
#define RETURN_SYSTEM_NEEDS_REBOOT (2)
#define RETURN_FS_ERRORS_LEFT_UNCORRECTED (4)
#define RETURN_OPERATIONAL_ERROR (8)
#define RETURN_USAGE_OR_SYNTAX_ERROR (16)
#define RETURN_CANCELLED_BY_USER (32)
/* Where did 64 go? */
#define RETURN_SHARED_LIBRARY_ERROR (128)
#define check_failed(FORMAT, ARGS...) \
do { \
    gsErrors++; \
    ntfs_log_redirect(__FUNCTION__,__FILE__,__LINE__, \
        NTFS_LOG_LEVEL_ERROR,NULL,FORMAT,##ARGS); \
} while (0);
// check -- end


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

static ntfs_time mkntfs_time                    (void);
static void mkntfs_cleanup                      (void);
static long mkntfs_get_page_size                (void);
static BOOL mkntfs_initialize_rl_bad            (void);
static BOOL mkntfs_initialize_rl_mft            (void);
static BOOL mkntfs_initialize_rl_boot           (void);
static BOOL mkntfs_initialize_bitmaps           (void);
static BOOL mkntfs_initialize_rl_logfile        (void);
static BOOL mkntfs_create_root_structures       (void);
static BOOL mkntfs_fill_device_with_zeroes      (void);
static int create_backup_boot_sector            (u8 *buff);
static int mft_bitmap_get_bit                   (s64 mft_no);
static VCN get_last_vcn                         (runlist *rl);
static runlist * allocate_scattered_clusters    (s64 clusters);
static ntfs_time stdinfo_time                   (MFT_RECORD *m);
static int initialize_quota                     (MFT_RECORD *m);
static BOOL non_resident_unnamed_data           (MFT_RECORD *m);
static void check_volume                        (ntfs_volume *vol);
static int reset_dirty                          (ntfs_volume *vol);
static BOOL mkntfs_override_vol_params          (ntfs_volume *vol);
static void deallocate_scattered_clusters       (const runlist *rl);
static BOOL bitmap_deallocate                   (LCN lcn, s64 length);
static int verify_mft_preliminary               (ntfs_volume *rawvol);
static int mft_bitmap_load                      (ntfs_volume *rawvol);
static void fs_sandbox_header_init              (EfsFileHeader* header);
static BOOL check_file_record                   (u8 *buffer, u16 buflen);
static BOOL append_to_bad_blocks                (unsigned long long block);
static void bitmap_build                        (u8 *buf, LCN lcn, s64 length);
static void verify_mft_record                   (ntfs_volume *vol, s64 mft_num);
static int bitmap_get_and_set                   (LCN lcn, unsigned long length);
static int assert_u32_equal                     (u32 val, u32 ok, const char *name);
static int assert_u32_noteq                     (u32 val, u32 wrong, const char *name);
static int assert_u32_less                      (u32 val1, u32 val2, const char *name);
static int assert_u32_lesseq                    (u32 val1, u32 val2, const char *name);
static int add_attr_object_id                   (MFT_RECORD *m, const GUID *object_id);
static BOOL mkntfs_open_partition               (ntfs_volume *vol, const char* devName);
static int initialize_secure                    (char *sds, u32 sds_size, MFT_RECORD *m);
static void replay_log                          (ntfs_volume *vol __attribute__((unused)));
static int make_room_for_attribute              (MFT_RECORD *m, char *pos, const u32 size);
static uint64_t crc64                           (uint64_t crc, const byte * data, size_t size);
static BOOL verify_boot_sector                  (struct ntfs_device *dev, ntfs_volume *rawvol);
static int add_attr_sd                          (MFT_RECORD *m, const u8 *sd, const s64 sd_len);
static s64 mkntfs_bitmap_write                  (struct ntfs_device *dev, s64 offset, s64 length);
static BOOL mkntfs_parse_long                   (const char *string, const char *name, long *num);
static int make_room_for_index_entry_in_index_block (INDEX_BLOCK *idx, INDEX_ENTRY *pos, u32 size);
static ATTR_REC *check_attr_record              (ATTR_REC *attr_rec, MFT_RECORD *mft_rec, u16 buflen);
static int index_obj_id_insert                  (MFT_RECORD *m, const GUID *guid, const leMFT_REF ref);
static BOOL mkntfs_parse_llong                  (const char *string, const char *name, long long *num);
static long long mkntfs_write                   (struct ntfs_device *dev, const void *b, long long count);
static int add_attr_std_info                    (MFT_RECORD *m, const FILE_ATTR_FLAGS flags, le32 security_id);
static BOOL mkntfs_sync_index_record            (INDEX_ALLOCATION* idx, MFT_RECORD* m, ntfschar* name, u32 name_len);
static s64 mkntfs_logfile_write                 (struct ntfs_device *dev, s64 offset __attribute__((unused)), s64 length);
static BOOL create_file_volume                  (MFT_RECORD *m, leMFT_REF root_ref, VOLUME_FLAGS fl, const GUID *volume_guid);
static int add_attr_vol_info                    (MFT_RECORD *m, const VOLUME_FLAGS flags, const u8 major_ver, const u8 minor_ver);
static int insert_file_link_in_dir_index        (INDEX_BLOCK *idx, leMFT_REF file_ref, FILE_NAME_ATTR *file_name, u32 file_name_size);
static int add_attr_vol_name                    (MFT_RECORD *m, const char *vol_name, const int vol_name_len __attribute__((unused)));
static int ntfs_index_keys_compare              (u8 *key1, u8 *key2, int key1_length, int key2_length, COLLATION_RULES collation_rule);
static int insert_index_entry_in_res_dir_index  (INDEX_ENTRY *idx, u32 idx_size, MFT_RECORD *m, ntfschar *name, u32 name_size, ATTR_TYPES type);
static runlist *load_runlist                    (ntfs_volume *rawvol, s64 offset_to_file_record, ATTR_TYPES attr_type, u32 size_of_file_record);
static int upgrade_to_large_index               (MFT_RECORD *m, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, INDEX_ALLOCATION **idx);
static s64 ntfs_rlwrite                         (struct ntfs_device *dev, const runlist *rl, const u8 *val, const s64 val_len, s64 *inited_size, WRITE_TYPE write_type);
static int add_attr_bitmap                      (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *bitmap, const u32 bitmap_len);
static int add_attr_data                        (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const u8 *val, const s64 val_len);
static int add_attr_index_alloc                 (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *index_alloc_val, const u32 index_alloc_val_len);
static int add_attr_bitmap_positioned           (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const runlist *rl, const u8 *bitmap, const u32 bitmap_len);
static int add_attr_data_positioned             (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len);
static int mkntfs_attr_find                     (const ATTR_TYPES type, const ntfschar *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx);
static int insert_positioned_attr_in_mft_record (MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len);
static int insert_non_resident_attr_in_mft_record(MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const u8 *val, const s64 val_len, WRITE_TYPE write_type);
static int add_attr_index_root                  (MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_TYPES indexed_attr_type, const COLLATION_RULES collation_rule, const u32 index_block_size);
static int mkntfs_attr_lookup                   (const ATTR_TYPES type, const ntfschar *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const VCN lowest_vcn __attribute__((unused)), const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx);
static int insert_resident_attr_in_mft_record   (MFT_RECORD *m, const ATTR_TYPES type, const char *name, u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const RESIDENT_ATTR_FLAGS res_flags, const u8 *val, const u32 val_len);
static int add_attr_file_name                   (MFT_RECORD *m, const leMFT_REF parent_dir, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type);
static int create_hardlink                      (INDEX_BLOCK *idx, const leMFT_REF ref_parent, MFT_RECORD *m_file, const leMFT_REF ref_file, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type);
static int create_hardlink_res                  (MFT_RECORD *m_parent, const leMFT_REF ref_parent, MFT_RECORD *m_file, const leMFT_REF ref_file, const s64 allocated_size, const s64 data_size, const FILE_ATTR_FLAGS flags, const u16 packed_ea_size, const u32 reparse_point_tag, const char *file_name, const FILE_NAME_TYPE_FLAGS file_name_type);

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

bool sandbox_fs_generated_box(const char * absolutePath, cuint64 sizeMB)
{
    c_return_val_if_fail(absolutePath && (absolutePath[0] == '/') && (sizeMB > 0), false);

    bool hasError = false;

    cchar* dirPath = c_strdup(absolutePath);
    if (dirPath) {
        cchar* dir = c_strrstr(dirPath, "/");
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
    c_return_val_if_fail(!hasError, false);

    int fd = open(absolutePath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        C_LOG_VERB("open: '%s' error: %s", absolutePath, c_strerror(errno));
        return false;
    }

    if (lseek(fd, 0, SEEK_END) > 0) {
        return true;
    }

    do {
        cuint64 needSize = 1024 * 1024 * sizeMB;
        off_t ret = lseek(fd, needSize - 1, SEEK_SET);
        if (ret < 0) {
            C_LOG_VERB("lseek: '%s' error: %s", absolutePath, c_strerror(errno));
            hasError = true;
            break;
        }
        else {
            if (-1 == write(fd, "", 1)) {
                C_LOG_VERB("write: '%s' error: %s", absolutePath, c_strerror(errno));
                hasError = true;
                break;
            }
            c_fsync(fd);
        }
    } while (0);

    // 写入 andsec 加密文件头

    CError* error = NULL;
    c_close(fd, &error);
    if (error) {
        hasError = true;
        C_LOG_VERB("close: '%s' error: %s", absolutePath, error->message);
        c_error_free(error);
    }
    c_return_val_if_fail(!hasError, false);

    return true;
}

bool sandbox_fs_format(const char* filePath)
{
    c_return_val_if_fail(filePath, false);

    // set locale
    const char* locale = setlocale(LC_ALL, NULL);
    if (!locale) {
        locale = setlocale(LC_CTYPE, NULL);
        C_LOG_VERB("setlocale: '%s' error", locale ? locale : "NULL");
        return false;
    }

    //
    long long       lw, pos;
    cuint64         upCaseCrc;
    int             result = 1;
    ntfs_attr_search_ctx*   ctx = NULL;

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
    int i, err;
    ATTR_RECORD *a;
    MFT_RECORD *m;

    srandom(sle64_to_cpu(mkntfs_time())/10000000);

    // 单纯的分配内存
    gsVol = ntfs_volume_alloc();
    if (!gsVol) {
        C_LOG_ERROR("Could not create volume");
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
        ntfs_log_perror("Could not create attrdef structure");
        goto done;
    }
    memcpy(gsVol->attrdef, attrdef_ntfs3x_array, sizeof(attrdef_ntfs3x_array));
    gsVol->attrdef_len = sizeof(attrdef_ntfs3x_array);

    if (!mkntfs_open_partition(gsVol, filePath)) {
        goto done;
    }

    if (!mkntfs_override_vol_params(gsVol)) {
        goto done;
    }

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
        goto done;
    }

    // MFT runlist、MFT 备份的 runlist、logfile 分配
    if (!mkntfs_initialize_rl_mft()) {
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
        goto done;
    }

    /* Allocate a buffer large enough to hold the mft. */
    // 写入的基本单位，默认 27K
    gsBuf = ntfs_calloc(gsMftSize);
    if (!gsBuf) {
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
        goto done;
    }

    /**
     * 负责创建文件系统根目录结构
     */
    if (!mkntfs_create_root_structures()) {
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
    ntfs_log_verbose("Syncing root directory index record.\n");
    if (!mkntfs_sync_index_record(gsIndexBlock, (MFT_RECORD*) (gsBuf + 5 * gsVol->mft_record_size), NTFS_INDEX_I30, 4)) {
        goto done;
    }

    ntfs_log_verbose("Syncing $Bitmap.\n");
    m = (MFT_RECORD*)(gsBuf + 6 * gsVol->mft_record_size);

    ctx = ntfs_attr_get_search_ctx(NULL, m);
    if (!ctx) {
        ntfs_log_perror("Could not create an attribute search context");
        goto done;
    }

    if (mkntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        ntfs_log_error("BUG: $DATA attribute not found.\n");
        goto done;
    }

    a = ctx->attr;
    if (a->non_resident) {
        runlist *rl = ntfs_mapping_pairs_decompress(gsVol, a, NULL);
        if (!rl) {
            ntfs_log_error("ntfs_mapping_pairs_decompress() failed\n");
            goto done;
        }
        lw = ntfs_rlwrite(gsVol->dev, rl, (const u8*)NULL, gsLcnBitmapByteSize, NULL, WRITE_BITMAP);
        err = errno;
        free(rl);
        if (lw != gsLcnBitmapByteSize) {
            ntfs_log_error("ntfs_rlwrite: %s\n", lw == -1 ? strerror(err) : "unknown error");
            goto done;
        }
    }
    else {
        /* Error : the bitmap must be created non resident */
        ntfs_log_error("Error : the global bitmap is resident\n");
        goto done;
    }

    /*
     * No need to sync $MFT/$BITMAP as that has never been modified since
     * its creation.
     */
    ntfs_log_verbose("Syncing $MFT.\n");
    pos = gsMftLcn * gsVol->cluster_size;
    lw = 1;
    for (i = 0; i < gsMftSize / (s32)gsVol->mft_record_size; i++) {
        lw = ntfs_mst_pwrite(gsVol->dev, pos, 1, gsVol->mft_record_size, gsBuf + i * gsVol->mft_record_size);
        if (lw != 1) {
            ntfs_log_error("ntfs_mst_pwrite: %s\n", lw == -1 ? strerror(errno) : "unknown error");
            goto done;
        }
        pos += gsVol->mft_record_size;
    }
    ntfs_log_verbose("Updating $MFTMirr.\n");
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
            ntfs_log_error("ntfs_mft_usn_dec");
            goto done;
        }

        lw = ntfs_mst_pwrite(gsVol->dev, pos, 1, gsVol->mft_record_size, gsBuf + i * gsVol->mft_record_size);

        if (lw != 1) {
            ntfs_log_error("ntfs_mst_pwrite: %s\n", lw == -1 ? strerror(errno) : "unknown error");
            goto done;
        }
        pos += gsVol->mft_record_size;
    }

    ntfs_log_verbose("Syncing device.\n");
    if (gsVol->dev->d_ops->sync(gsVol->dev)) {
        ntfs_log_error("Syncing device. FAILED");
        goto done;
    }
    ntfs_log_quiet("mkntfs completed successfully. Have a nice day.\n");
    result = 0;

done:
    ntfs_attr_put_search_ctx(ctx);
    mkntfs_cleanup();    /* Device is unlocked and closed here */

    setlocale(LC_ALL, locale);

    return true;
}

bool sandbox_fs_check(const char * filePath)
{
    c_return_val_if_fail(filePath != NULL, false);

    int ret;
    ntfs_volume *vol;
    ntfs_volume rawvol;
    struct ntfs_device *dev;

    dev = ntfs_device_alloc(filePath, 0, &ntfs_device_default_io_ops, NULL);
    if (!dev) {
        return false;
    }

    if (dev->d_ops->open(dev, O_RDONLY)) {
        ntfs_log_perror("Error opening partition device");
        ntfs_device_free(dev);
        return false;
    }

    if ((ret = verify_boot_sector(dev,&rawvol))) {
        dev->d_ops->close(dev);
        return ret;
    }
    ntfs_log_verbose("Boot sector verification complete. Proceeding to $MFT");

    verify_mft_preliminary(&rawvol);

    /* ntfs_device_mount() expects the device to be closed. */
    if (dev->d_ops->close(dev)) {
        ntfs_log_perror("Failed to close the device.");
    }

    // at this point we know that the volume is valid enough for mounting.
    /* Call ntfs_device_mount() to do the actual mount. */
    vol = ntfs_device_mount(dev, NTFS_MNT_RDONLY);
    if (!vol) {
        ntfs_device_free(dev);
        return false;
    }

    replay_log(vol);

    if (vol->flags & VOLUME_IS_DIRTY) {
        ntfs_log_warning("Volume is dirty.\n");
    }

    check_volume(vol);

    if (gsErrors) {
        ntfs_log_info("Errors found.\n");
    }

    if (gsUnsupported) {
        ntfs_log_info("Unsupported cases found.\n");
    }

    if (!gsErrors && !gsUnsupported) {
        reset_dirty(vol);
    }

    ntfs_umount(vol, FALSE);

    if (gsErrors) {
        return false;
    }

    if (gsUnsupported) {
        return false;
    }

    return true;
}



static void fs_sandbox_header_init (EfsFileHeader* header)
{
    memset(header, 0, sizeof(EfsFileHeader));

    header->magic = SANDBOX_FS_MAGIC;
    header->magicID = SANDBOX_FS_MAGIC_ID;
    header->version = SANDBOX_VERSION_NEW;
    header->headSize = sizeof(EfsFileHeader);
    header->fileType = FILE_TYPE_SANDBOX;
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
            ntfs_log_perror("Error writing to %s", dev->d_name);
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
        ntfs_log_error("Failed to complete writing to %s after three retries.\n", dev->d_name);
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
            ntfs_log_error("Bitmap allocation error\n");
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
                ntfs_log_perror("Not enough memory");
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
        ntfs_log_error("Can only allocate a single cluster at a time\n");
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
        ntfs_log_error("You may only specify the %s once.\n", name);
        return FALSE;
    }

    tmp = strtol(string, &end, 0);
    if (end && *end) {
        ntfs_log_error("Cannot understand the %s '%s'.\n", name, string);
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
        ntfs_log_error("You may only specify the %s once.\n", name);
        return FALSE;
    }

    tmp = strtoll(string, &end, 0);
    if (end && *end) {
        ntfs_log_error("Cannot understand the %s '%s'.\n", name,
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
            ntfs_log_perror("Reallocating memory for bad blocks list failed");
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
        if (dev->d_ops->seek(dev, rl[i].lcn * gsVol->cluster_size, SEEK_SET) == (off_t)-1)
            return -1LL;
        retry = 0;
        do {
            /* use specific functions if buffer is not prefilled */
            switch (write_type) {
            case WRITE_BITMAP :
                bytes_written = mkntfs_bitmap_write(dev,
                    total, length);
                break;
            case WRITE_LOGFILE :
                bytes_written = mkntfs_logfile_write(dev,
                    total, length);
                break;
            default :
                bytes_written = dev->d_ops->write(dev,
                    val + total, length);
                break;
            }
            if (bytes_written == -1LL) {
                retry = errno;
                ntfs_log_perror("Error writing to %s",
                    dev->d_name);
                errno = retry;
                return bytes_written;
            }
            if (bytes_written) {
                length -= bytes_written;
                total += bytes_written;
                if (inited_size)
                    *inited_size += bytes_written;
            } else {
                retry++;
            }
        } while (length && retry < 3);
        if (length) {
            ntfs_log_error("Failed to complete writing to %s after three "
                    "retries.\n", dev->d_name);
            return total;
        }
    }
    if (delta) {
        int eo;
        char *b = ntfs_calloc(delta);
        if (!b)
            return -1;
        bytes_written = mkntfs_write(dev, b, delta);
        eo = errno;
        free(b);
        errno = eo;
        if (bytes_written == -1LL)
            return bytes_written;
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
        ntfs_log_error("make_room_for_attribute() received non 8-byte aligned "
                "size.\n");
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
    ntfs_log_trace("File is corrupt. Run chkdsk.\n");
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
        ntfs_log_error("Failed to allocate attribute search context.\n");
        err = -ENOMEM;
        goto err_out;
    }
    if (ic == IGNORE_CASE) {
        ntfs_log_error("FIXME: Hit unimplemented code path #1.\n");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, NULL, 0, ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    if (errno != ENOENT) {
        ntfs_log_error("Corrupt inode.\n");
        err = -errno;
        goto err_out;
    }
    a = ctx->attr;
    if (flags & ATTR_COMPRESSION_MASK) {
        ntfs_log_error("Compressed attributes not supported yet.\n");
        /* FIXME: Compress attribute into a temporary buffer, set */
        /* val accordingly and save the compressed size. */
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
        ntfs_log_error("Encrypted/sparse attributes not supported.\n");
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
                ntfs_log_error("Failed to get size for mapping pairs.\n");
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
        ntfs_log_error("BUG: Allocated size is smaller than data size!\n");
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
        ntfs_log_error("BUG(): in insert_positioned_attribute_in_mft_"
                "record(): make_room_for_attribute() returned "
                "error: EINVAL!\n");
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
            ntfs_log_error("Unknown compression format. Reverting to standard compression.\n");
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
            ntfs_log_error("Error writing non-resident attribute value.\n");
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
        ntfs_log_error("insert_positioned_attr_in_mft_record failed with error %i.\n", err < 0 ? err : (int)bw);
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
        ntfs_log_error("Failed to allocate attribute search context.\n");
        err = -ENOMEM;
        goto err_out;
    }
    if (ic == IGNORE_CASE) {
        ntfs_log_error("FIXME: Hit unimplemented code path #2.\n");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, NULL, 0, ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    if (errno != ENOENT) {
        ntfs_log_error("Corrupt inode.\n");
        err = -errno;
        goto err_out;
    }
    a = ctx->attr;
    if (flags & ATTR_COMPRESSION_MASK) {
        ntfs_log_error("Compressed attributes not supported yet.\n");
        /* FIXME: Compress attribute into a temporary buffer, set */
        /* val accordingly and save the compressed size. */
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
        ntfs_log_error("Encrypted/sparse attributes not supported.\n");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (val_len) {
        rl = allocate_scattered_clusters((val_len + gsVol->cluster_size - 1) / gsVol->cluster_size);
        if (!rl) {
            err = -errno;
            ntfs_log_perror("Failed to allocate scattered clusters");
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
                ntfs_log_error("Failed to get size for mapping pairs.\n");
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
        ntfs_log_error("BUG(): in insert_non_resident_attribute_in_mft_record(): make_room_for_attribute() returned error: EINVAL!\n");
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
            ntfs_log_error("Unknown compression format. Reverting to standard compression.\n");
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
            ntfs_log_error("Error writing non-resident attribute value.\n");
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
        ntfs_log_error("insert_non_resident_attr_in_mft_record failed with error %lld.\n", (long long) (err < 0 ? err : bw));
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
        ntfs_log_error("Failed to allocate attribute search context.\n");
        err = -ENOMEM;
        goto err_out;
    }
    if (ic == IGNORE_CASE) {
        ntfs_log_error("FIXME: Hit unimplemented code path #3.\n");
        err = -EOPNOTSUPP;
        goto err_out;
    }
    if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, val, val_len,
            ctx)) {
        err = -EEXIST;
        goto err_out;
    }
    if (errno != ENOENT) {
        ntfs_log_error("Corrupt inode.\n");
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
        ntfs_log_error("BUG(): in insert_resident_attribute_in_mft_"
                "record(): make_room_for_attribute() returned "
                "error: EINVAL!\n");
        goto err_out;
    }
#endif
    a->type = type;
    a->length = cpu_to_le32(asize);
    a->non_resident = 0;
    a->name_length = name_len;
    if (type == AT_OBJECT_ID)
        a->name_offset = const_cpu_to_le16(0);
    else
        a->name_offset = const_cpu_to_le16(24);
    a->flags = flags;
    a->instance = m->next_attr_instance;
    m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance)
            + 1) & 0xffff);
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
        ntfs_log_perror("add_attr_std_info failed");
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
                    ntfs_log_error("BUG: Unnamed data not found\n");
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
                    ntfs_log_error("BUG: Standard information not found\n");
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
        ntfs_log_error("Failed to get attribute search context.\n");
        return -ENOMEM;
    }
    if (mkntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED, 0,
                CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        int eo = errno;
        ntfs_log_error("BUG: Standard information attribute not "
                "present in file record.\n");
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
        ntfs_log_error("add_attr_file_name failed: %s\n", strerror(-i));
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
        ntfs_log_error("add_attr_vol_info failed: %s\n", strerror(-err));
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
        ntfs_log_error("add_attr_sd failed: %s\n", strerror(-err));
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
        ntfs_log_error("add_attr_data failed: %s\n", strerror(-err));
    return err;
}

static int add_attr_data_positioned(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags, const runlist *rl, const u8 *val, const s64 val_len)
{
    int err;

    err = insert_positioned_attr_in_mft_record(m, AT_DATA, name, name_len,
            ic, flags, rl, val, val_len);
    if (err < 0)
        ntfs_log_error("add_attr_data_positioned failed: %s\n",
                strerror(-err));
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
        ntfs_log_error("add_attr_vol_name failed: %s\n", strerror(-i));
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
        ntfs_log_error("add_attr_vol_info failed: %s\n", strerror(-err));
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
        ntfs_log_error("add_attr_index_root: indexed attribute is $FILE_NAME "
            "but collation rule is not COLLATION_FILE_NAME.\n");
        return -EINVAL;
    }
    r->collation_rule = collation_rule;
    r->index_block_size = cpu_to_le32(index_block_size);
    if (index_block_size >= gsVol->cluster_size) {
        if (index_block_size % gsVol->cluster_size) {
            ntfs_log_error("add_attr_index_root: index block size is not "
                    "a multiple of the cluster size.\n");
            free(r);
            return -EINVAL;
        }
        r->clusters_per_index_block = index_block_size / gsVol->cluster_size;
    } else { /* if (g_vol->cluster_size > index_block_size) */
        if (index_block_size & (index_block_size - 1)) {
            ntfs_log_error("add_attr_index_root: index block size is not "
                    "a power of 2.\n");
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
        ntfs_log_error("add_attr_index_root failed: %s\n", strerror(-err));
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
        ntfs_log_error("add_attr_index_alloc failed: %s\n", strerror(-err));
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
        ntfs_log_error("add_attr_bitmap failed: %s\n", strerror(-err));
    return err;
}

static int add_attr_bitmap_positioned(MFT_RECORD *m, const char *name, const u32 name_len, const IGNORE_CASE_BOOL ic, const runlist *rl, const u8 *bitmap, const u32 bitmap_len)
{
    int err;

    err = insert_positioned_attr_in_mft_record(m, AT_BITMAP, name, name_len,
            ic, const_cpu_to_le16(0), rl, bitmap, bitmap_len);
    if (err < 0)
        ntfs_log_error("add_attr_bitmap_positioned failed: %s\n",
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
        ntfs_log_error("Failed to allocate attribute search context.\n");
        ntfs_ucsfree(uname);
        return -ENOMEM;
    }
    if (ic == IGNORE_CASE) {
        ntfs_log_error("FIXME: Hit unimplemented code path #4.\n");
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
        ntfs_log_error("Sector size is bigger than index block size. "
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
        ntfs_log_error("ntfs_mst_pre_write_fixup() failed in "
                "upgrade_to_large_index.\n");
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
        ntfs_log_error("make_room_for_index_entry_in_index_block() received "
                "non 8-byte aligned size.\n");
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
            "collation rule.\n");
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
        ntfs_log_error("Failed to allocate attribute search "
                "context.\n");
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
        ntfs_log_debug("file_name_attr1->file_name_length = %i\n",
                file_name->file_name_length);
        if (file_name->file_name_length) {
            char *__buf = NULL;
            i = ntfs_ucstombs((ntfschar*)&file_name->file_name,
                file_name->file_name_length, &__buf, 0);
            if (i < 0)
                ntfs_log_debug("Name contains non-displayable "
                        "Unicode characters.\n");
            ntfs_log_debug("file_name_attr1->file_name = %s\n",
                    __buf);
            free(__buf);
        }
        ntfs_log_debug("file_name_attr2->file_name_length = %i\n",
                ie->key.file_name.file_name_length);
        if (ie->key.file_name.file_name_length) {
            char *__buf = NULL;
            i = ntfs_ucstombs(ie->key.file_name.file_name,
                ie->key.file_name.file_name_length + 1, &__buf,
                0);
            if (i < 0)
                ntfs_log_debug("Name contains non-displayable "
                        "Unicode characters.\n");
            ntfs_log_debug("file_name_attr2->file_name = %s\n",
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
            ntfs_log_debug("BUG: ie->length is zero, breaking out "
                    "of loop.\n");
            break;
        }
#endif
        ie = (INDEX_ENTRY*)((char*)ie + le16_to_cpu(ie->length));
    };
    i = (sizeof(INDEX_ENTRY_HEADER) + file_name_size + 7) & ~7;
    err = make_room_for_index_entry_in_index_block(idx, ie, i);
    if (err) {
        ntfs_log_error("make_room_for_index_entry_in_index_block "
                "failed: %s\n", strerror(-err));
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
        ntfs_log_error("Too many hardlinks present already.\n");
        free(fn);
        return -EINVAL;
    }
    m_file->link_count = cpu_to_le16(i + 1);
    /* Add the file_name to @m_file. */
    i = insert_resident_attr_in_mft_record(m_file, AT_FILE_NAME, NULL, 0,
            CASE_SENSITIVE, const_cpu_to_le16(0),
            RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
    if (i < 0) {
        ntfs_log_error("create_hardlink failed adding file name "
                "attribute: %s\n", strerror(-i));
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
        ntfs_log_error("create_hardlink failed inserting index entry: "
                "%s\n", strerror(-i));
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
        ntfs_log_error("Too many hardlinks present already.\n");
        free(fn);
        return -EINVAL;
    }
    m_file->link_count = cpu_to_le16(i + 1);
    /* Add the file_name to @m_file. */
    i = insert_resident_attr_in_mft_record(m_file, AT_FILE_NAME, NULL, 0,
            CASE_SENSITIVE, const_cpu_to_le16(0),
            RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
    if (i < 0) {
        ntfs_log_error("create_hardlink failed adding file name attribute: "
                "%s\n", strerror(-i));
        free(fn);
        /* Undo link count increment. */
        m_file->link_count = cpu_to_le16(
                le16_to_cpu(m_file->link_count) - 1);
        return i;
    }
    /* Insert the index entry for file_name in @idx. */
    i = insert_file_link_in_dir_index(idx, ref_file, fn, fn_size);
    if (i < 0) {
        ntfs_log_error("create_hardlink failed inserting index entry: %s\n", strerror(-i));
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
        ntfs_log_error("index_obj_id_insert failed inserting index entry: %s\n", strerror(-err));
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
                ntfs_log_perror("Warning: Could not close %s", gsVol->dev->d_name);
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
    int i;
    struct stat sbuf;
    unsigned long mnt_flags;

    /**
     * 此处 设备读写，对 Linux C 读写等函数做了封装，所有信息放到 vol->dev 中
     */
    vol->dev = ntfs_device_alloc(devName, 0, &ntfs_device_default_io_ops, NULL);
    if (!vol->dev) {
        ntfs_log_perror("Could not create device");
        goto done;
    }

    /**
     * opts.no_action 命令行输入，是否执行实际的格式化磁盘操作
     */
    i = O_RDWR;

    /**
     * 打开磁盘
     */
    if (vol->dev->d_ops->open(vol->dev, i)) {
        if (errno == ENOENT) {
            ntfs_log_error("The device doesn't exist; did you specify it correctly?\n");
        }
        else {
            ntfs_log_perror("Could not open %s", vol->dev->d_name);
        }
        goto done;
    }

    /**
     * 获取磁盘信息，相当于执行 stat 操作
     */
    if (vol->dev->d_ops->stat(vol->dev, &sbuf)) {
        ntfs_log_perror("Error getting information about %s", vol->dev->d_name);
        goto done;
    }

    if (!S_ISBLK(sbuf.st_mode)) {
        if (!sbuf.st_size && !sbuf.st_blocks) {
            ntfs_log_error("You must specify the number of sectors.\n");
            goto done;
        }
#ifdef HAVE_LINUX_MAJOR_H
    }
    else if ((IDE_DISK_MAJOR(MAJOR(sbuf.st_rdev)) && MINOR(sbuf.st_rdev) % 64 == 0) || (SCSI_DISK_MAJOR(MAJOR(sbuf.st_rdev)) && MINOR(sbuf.st_rdev) % 16 == 0)) {
        ntfs_log_error("%s is entire device, not just one partition.\n", vol->dev->d_name);
#endif
    }

    /**
     * 根据 /etc/mtab 确认是否挂载
     */
    if (ntfs_check_if_mounted(vol->dev->d_name, &mnt_flags)) {
        ntfs_log_perror("Failed to determine whether %s is mounted", vol->dev->d_name);
    }
    else if (mnt_flags & NTFS_MF_MOUNTED) {
        ntfs_log_error("%s is mounted.\n", vol->dev->d_name);
        ntfs_log_warning("format forced anyway. Hope /etc/mtab is incorrect.\n");
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
        ntfs_log_warning("Failed to determine system page size. Assuming safe default of 4096 bytes.\n");
        return 4096;
    }
    ntfs_log_debug("System page size is %li bytes.\n", page_size);
    return page_size;
}

static BOOL mkntfs_override_vol_params(ntfs_volume *vol)
{
    s64 volume_size;
    long page_size;
    int i;
    BOOL winboot = TRUE;

    /* If user didn't specify the sector size, determine it now. */
    if (opts.sectorSize < 0) {
        opts.sectorSize = ntfs_device_sector_size_get(vol->dev);
        if (opts.sectorSize < 0) {
            ntfs_log_warning("The sector size was not specified for %s and it could not be obtained automatically.  It has been set to 512 bytes.\n", vol->dev->d_name);
            opts.sectorSize = 512;
        }
    }

    /* Validate sector size. */
    if ((opts.sectorSize - 1) & opts.sectorSize) {
        ntfs_log_error("The sector size is invalid.  It must be a power of two, e.g. 512, 1024.\n");
        return FALSE;
    }

    if (opts.sectorSize < 256 || opts.sectorSize > 4096) {
        ntfs_log_error("The sector size is invalid.  The minimum size is 256 bytes and the maximum is 4096 bytes.\n");
        return FALSE;
    }

    ntfs_log_debug("sector size = %ld bytes\n", opts.sectorSize);

    /**
     * 设置扇区大小
     */
    if (ntfs_device_block_size_set(vol->dev, opts.sectorSize)) {
        ntfs_log_debug("Failed to set the device block size to the sector size.  This may cause problems when creating the backup boot sector and also may affect performance but should be harmless otherwise.  Error: %s\n", strerror(errno));
    }

    /**
     * 扇区数量：测盘总大小 / 扇区大小
     */
    if (opts.numSectors < 0) {
        opts.numSectors = ntfs_device_size_get(vol->dev, opts.sectorSize);
        if (opts.numSectors <= 0) {
            ntfs_log_error("Couldn't determine the size of %s.  Please specify the number of sectors manually.\n", vol->dev->d_name);
            return FALSE;
        }
    }
    ntfs_log_debug("number of sectors = %lld (0x%llx)\n", opts.numSectors, opts.numSectors);

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
            ntfs_log_warning("The partition start sector was not specified for %s and it could not be obtained automatically.  It has been set to 0.\n", vol->dev->d_name);
            opts.partStartSect = 0;
            winboot = FALSE;
        }
        else if (opts.partStartSect >> 32) {
            ntfs_log_warning("The partition start sector was not specified for %s and the automatically determined value is too large (%lld). It has been set to 0.\n", vol->dev->d_name, (long long)opts.partStartSect);
            opts.partStartSect = 0;
            winboot = FALSE;
        }
    }
    else if (opts.partStartSect >> 32) {
        ntfs_log_error("Invalid partition start sector.  Maximum is 4294967295 (2^32-1).\n");
        return FALSE;
    }

    /* If user didn't specify the sectors per track, determine it now. */
    /**
     * 每个track多少扇区
     */
    if (opts.sectorsPerTrack < 0) {
        opts.sectorsPerTrack = ntfs_device_sectors_per_track_get(vol->dev);
        if (opts.sectorsPerTrack < 0) {
            ntfs_log_warning("The number of sectors per track was not specified for %s and it could not be obtained automatically.  It has been set to 0.\n", vol->dev->d_name);
            opts.sectorsPerTrack = 0;
            winboot = FALSE;
        }
        else if (opts.sectorsPerTrack > 65535) {
            ntfs_log_warning("The number of sectors per track was not specified for %s and the automatically determined value is too large.  It has been set to 0.\n", vol->dev->d_name);
            opts.sectorsPerTrack = 0;
            winboot = FALSE;
        }
    }
    else if (opts.sectorsPerTrack > 65535) {
        ntfs_log_error("Invalid number of sectors per track.  Maximum is 65535.\n");
        return FALSE;
    }

    /**
     * 磁盘磁头数量
     */
    if (opts.heads < 0) {
        opts.heads = ntfs_device_heads_get(vol->dev);
        if (opts.heads < 0) {
            ntfs_log_warning("The number of heads was not specified for %s and it could not be obtained automatically.  It has been set to 0.\n", vol->dev->d_name);
            opts.heads = 0;
            winboot = FALSE;
        }
        else if (opts.heads > 65535) {
            ntfs_log_warning("The number of heads was not specified for %s and the automatically determined value is too large.  It has been set to 0.\n", vol->dev->d_name);
            opts.heads = 0;
            winboot = FALSE;
        }
    }
    else if (opts.heads > 65535) {
        ntfs_log_error("Invalid number of heads.  Maximum is 65535.\n");
        return FALSE;
    }
    volume_size = opts.numSectors * opts.sectorSize;  // 磁盘扇区数量 x 磁盘扇区大小 = 磁盘总容量

    /**
     * 磁盘容量需大于 1MB
     */
    if (volume_size < (1 << 20)) {            /* 1MiB */
        ntfs_log_error("Device is too small (%llikiB).  Minimum NTFS volume size is 1MiB.\n", (long long)(volume_size / 1024));
        return FALSE;
    }
    ntfs_log_debug("volume size = %llikiB\n", (long long) (volume_size / 1024));

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
                ntfs_log_error("Device is too large to hold an NTFS volume (maximum size is 256TiB).\n");
                return FALSE;
            }
        }
        ntfs_log_quiet("Cluster size has been automatically set to %u bytes.\n", (unsigned)vol->cluster_size);
    }

    /**
     * 检测块大小是否是 2 的指数次
     */
    if (vol->cluster_size & (vol->cluster_size - 1)) {
        ntfs_log_error("The cluster size is invalid.  It must be a power of two, e.g. 1024, 4096.\n");
        return FALSE;
    }
    if (vol->cluster_size < (u32)opts.sectorSize) {
        ntfs_log_error("The cluster size is invalid.  It must be equal to, or larger than, the sector size.\n");
        return FALSE;
    }

    /* Before Windows 10 Creators, the limit was 128 */
    if (vol->cluster_size > 4096 * (u32)opts.sectorSize) {
        ntfs_log_error("The cluster size is invalid.  It cannot be more that 4096 times the size of the sector size.\n");
        return FALSE;
    }

    if (vol->cluster_size > NTFS_MAX_CLUSTER_SIZE) {
        ntfs_log_error("The cluster size is invalid.  The maximum cluster size is %lu bytes (%lukiB).\n", (unsigned long)NTFS_MAX_CLUSTER_SIZE, (unsigned long)(NTFS_MAX_CLUSTER_SIZE >> 10));
        return FALSE;
    }

    vol->cluster_size_bits = ffs(vol->cluster_size) - 1;
    ntfs_log_debug("cluster size = %u bytes\n", (unsigned int)vol->cluster_size);
    if (vol->cluster_size > 4096) {
        ntfs_log_warning("Windows cannot use compression when the cluster size is larger than 4096 bytes. Compression has been disabled for this volume.\n");
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
        ntfs_log_error("Illegal combination of volume/cluster/sector size and/or cluster/sector number.\n");
        return FALSE;
    }
    ntfs_log_debug("number of clusters = %llu (0x%llx)\n", (unsigned long long)vol->nr_clusters, (unsigned long long)vol->nr_clusters);

    /* Number of clusters must fit within 32 bits (Win2k limitation). */
    if (vol->nr_clusters >> 32) {
        if (vol->cluster_size >= 65536) {
            ntfs_log_error("Device is too large to hold an NTFS volume (maximum size is 256TiB).\n");
            return FALSE;
        }
        ntfs_log_error("Number of clusters exceeds 32 bits.  Please try again with a larger\ncluster size or leave the cluster size unspecified and the smallest possible cluster size for the size of the device will be used.\n");
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
        ntfs_log_warning("Mft record size (%u bytes) exceeds system page size (%li bytes). You will not be able to mount this volume using the NTFS kernel driver.\n", (unsigned)vol->mft_record_size, page_size);
    }

    // ffs 位操作中用于定位下一个可用的位（从低位开始，下一个可被设位1的位置）
    // 默认值：10
    vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;
    ntfs_log_debug("mft record size = %u bytes\n", (unsigned)vol->mft_record_size);

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
        ntfs_log_warning("Index record size (%u bytes) exceeds system page size (%li bytes).  You will not be able to mount this volume using the NTFS kernel driver.\n", (unsigned)vol->indx_record_size, page_size);
    }
    vol->indx_record_size_bits = ffs(vol->indx_record_size) - 1;
    ntfs_log_debug("index record size = %u bytes\n", (unsigned)vol->indx_record_size);
    if (!winboot) {
        ntfs_log_warning("To boot from a device, Windows needs the 'partition start sector', the 'sectors per track' and the 'number of heads' to be set.\n");
        ntfs_log_warning("Windows will not be able to boot from this device.\n");
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
    ntfs_log_debug("g_lcn_bitmap_byte_size = %i, allocated = %llu\n", gsLcnBitmapByteSize, (unsigned long long)i);
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
    ntfs_log_debug("MFT size = %i (0x%x) bytes\n", gsMftSize, gsMftSize);

    /* Determine mft bitmap size and allocate it. */
    mft_bitmap_size = gsMftSize / gsVol->mft_record_size;      // 7

    /* Convert to bytes, at least one. */
    gsMftBitmapByteSize = (mft_bitmap_size + 7) >> 3;        // 1

    /* Mft bitmap is allocated in multiples of 8 bytes. */
    gsMftBitmapByteSize = (gsMftBitmapByteSize + 7) & ~7; // 8
    ntfs_log_debug("mft_bitmap_size = %i, g_mft_bitmap_byte_size = %i\n", mft_bitmap_size, gsMftBitmapByteSize);
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
    ntfs_log_debug("$MFT logical cluster number = 0x%llx\n", gsMftLcn);
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
    ntfs_log_debug("MFT zone size = %lldkiB\n", gsMftZoneEnd << gsVol->cluster_size_bits >> 10 /* >> 10 == / 1024 */);

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
    ntfs_log_debug("$MFTMirr logical cluster number = 0x%llx\n", gsMftmirrLcn);

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
    ntfs_log_debug("$LogFile logical cluster number = 0x%llx\n", gsLogfileLcn);

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
    ntfs_log_debug("$LogFile (journal) size = %ikiB\n", gsLogfileSize / 1024);
    /*
     * FIXME: The 256kiB limit is arbitrary. Should find out what the real
     * minimum requirement for Windows is so it doesn't blue screen.
     */
    if (gsLogfileSize < 256 << 10) {
        ntfs_log_error("$LogFile would be created with invalid size. This is not allowed as it would cause Windows to blue screen and during boot.\n");
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
                ntfs_log_error("This should not happen.\n");
                return FALSE;
            }

            if (!position) {
                ntfs_log_error("Error: Cluster zero is bad. Cannot create NTFS file system.\n");
                return FALSE;
            }

            /* Add the baddie to our bad blocks list. */
            if (!append_to_bad_blocks(position)) {
                return FALSE;
            }
            ntfs_log_quiet("\nFound bad cluster (%lld). Adding to list of bad blocks.\nInitializing device with zeroes: %3.0f%%", position, position / progress_inc);

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
                ntfs_log_error("This should not happen.\n");
                return FALSE;
            }
            else if (i + 1ull == position) {
                ntfs_log_error("Error: Bad cluster found in location reserved for system file $Boot.\n");
                return FALSE;
            }
            /* Seek to next sector. */
            gsVol->dev->d_ops->seek(gsVol->dev, opts.sectorSize, SEEK_CUR);
        }
    }
    ntfs_log_progress(" - Done.\n");

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
        ntfs_log_perror("Failed to allocate attribute search context");
        return FALSE;
    }
    /* FIXME: This should be IGNORE_CASE! */
    if (mkntfs_attr_lookup(AT_INDEX_ALLOCATION, name, name_len, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
        ntfs_attr_put_search_ctx(ctx);
        ntfs_log_error("BUG: $INDEX_ALLOCATION attribute not found.\n");
        return FALSE;
    }
    a = ctx->attr;
    rl_index = ntfs_mapping_pairs_decompress(gsVol, a, NULL);
    if (!rl_index) {
        ntfs_attr_put_search_ctx(ctx);
        ntfs_log_error("Failed to decompress runlist of $INDEX_ALLOCATION "
                "attribute.\n");
        return FALSE;
    }
    if (sle64_to_cpu(a->initialized_size) < i) {
        ntfs_attr_put_search_ctx(ctx);
        free(rl_index);
        ntfs_log_error("BUG: $INDEX_ALLOCATION attribute too short.\n");
        return FALSE;
    }
    ntfs_attr_put_search_ctx(ctx);
    i = sizeof(INDEX_BLOCK) - sizeof(INDEX_HEADER) +
            le32_to_cpu(idx->index.allocated_size);
    err = ntfs_mst_pre_write_fixup((NTFS_RECORD*)idx, i);
    if (err) {
        free(rl_index);
        ntfs_log_error("ntfs_mst_pre_write_fixup() failed while "
            "syncing index block.\n");
        return FALSE;
    }
    lw = ntfs_rlwrite(gsVol->dev, rl_index, (u8*)idx, i, NULL, WRITE_STANDARD);
    free(rl_index);
    if (lw != i) {
        ntfs_log_error("Error writing $INDEX_ALLOCATION.\n");
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

    ntfs_log_verbose("Creating $Volume (mft record 3)\n");
    m = (MFT_RECORD*)(gsBuf + 3 * gsVol->mft_record_size);
    err = create_hardlink(gsIndexBlock, root_ref, m,
            MK_LE_MREF(FILE_Volume, FILE_Volume), 0LL, 0LL,
            FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
            "$Volume", FILE_NAME_WIN32_AND_DOS);
    if (!err) {
        init_system_file_sd(FILE_Volume, &sd, &i);
        err = add_attr_sd(m, sd, i);
    }
    if (!err)
        err = add_attr_data(m, NULL, 0, CASE_SENSITIVE,
                const_cpu_to_le16(0), NULL, 0);
    if (!err)
        err = add_attr_vol_name(m, gsVol->vol_name, gsVol->vol_name ?
                strlen(gsVol->vol_name) : 0);
    if (!err) {
        if (fl & VOLUME_IS_DIRTY)
            ntfs_log_quiet("Setting the volume dirty so check "
                    "disk runs on next reboot into "
                    "Windows.\n");
        err = add_attr_vol_info(m, fl, gsVol->major_ver, gsVol->minor_ver);
    }
    if (err < 0) {
        ntfs_log_error("Couldn't create $Volume: %s\n", strerror(-err));
        return FALSE;
    }
    return TRUE;
}

static int create_backup_boot_sector(u8 *buff)
{
    const char *s;
    ssize_t bw;
    int size, e;

    ntfs_log_verbose("Creating backup boot sector.\n");
    /*
     * Write the first max(512, opts.sector_size) bytes from buf to the
     * last sector, but limit that to 8192 bytes of written data since that
     * is how big $Boot is (and how big our buffer is)..
     */
    size = 512;
    if (size < opts.sectorSize)
        size = opts.sectorSize;
    if (gsVol->dev->d_ops->seek(gsVol->dev, (opts.numSectors + 1) * opts.sectorSize - size, SEEK_SET) == (off_t)-1) {
        ntfs_log_perror("Seek failed");
        goto bb_err;
    }
    if (size > 8192)
        size = 8192;
    bw = mkntfs_write(gsVol->dev, buff, size);
    if (bw == size)
        return 0;
    e = errno;
    if (bw == -1LL)
        s = strerror(e);
    else
        s = "unknown error";
    /* At least some 2.4 kernels return EIO instead of ENOSPC. */
    if (bw != -1LL || (bw == -1LL && e != ENOSPC && e != EIO)) {
        ntfs_log_critical("Couldn't write backup boot sector: %s\n", s);
        return -1;
    }
    bb_err:
        ntfs_log_error("Couldn't write backup boot sector. This is due to a "
                "limitation in the\nLinux kernel. This is not a major "
                "problem as Windows check disk will create the\n"
                "backup boot sector when it is run on your next boot "
                "into Windows.\n");
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

    ntfs_log_quiet("Creating NTFS volume structures.\n");
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
            ntfs_log_error("Failed to layout system mft records.\n");
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
                ntfs_log_error("Failed to layout mft record.\n");
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
    ntfs_log_verbose("Creating root directory (mft record 5)\n");
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
            ntfs_log_perror("Failed to allocate attribute search context");
            return FALSE;
        }
        /* There is exactly one file name so this is ok. */
        if (mkntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
            ntfs_attr_put_search_ctx(ctx);
            ntfs_log_error("BUG: $FILE_NAME attribute not found.\n");
            return FALSE;
        }
        a = ctx->attr;
        err = insert_file_link_in_dir_index(gsIndexBlock, root_ref, (FILE_NAME_ATTR*)((char*)a + le16_to_cpu(a->value_offset)), le32_to_cpu(a->value_length));
        ntfs_attr_put_search_ctx(ctx);
    }
    if (err) {
        ntfs_log_error("Couldn't create root directory: %s\n", strerror(-err));
        return FALSE;
    }

    /* Add all other attributes, on a per-file basis for clarity. */
    ntfs_log_verbose("Creating $MFT (mft record 0)\n");
    m = (MFT_RECORD*)gsBuf;
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE,
            const_cpu_to_le16(0), gsRlMft, gsBuf, gsMftSize);
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_MFT, 1),
                ((gsMftSize - 1)
                    | (gsVol->cluster_size - 1)) + 1,
                gsMftSize, FILE_ATTR_HIDDEN |
                FILE_ATTR_SYSTEM, 0, 0, "$MFT",
                FILE_NAME_WIN32_AND_DOS);
    /* mft_bitmap is not modified in mkntfs; no need to sync it later. */
    if (!err)
        err = add_attr_bitmap_positioned(m, NULL, 0, CASE_SENSITIVE,
                gsRlMftBmp,
                gsMftBitmap, gsMftBitmapByteSize);
    if (err < 0) {
        ntfs_log_error("Couldn't create $MFT: %s\n", strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $MFTMirr (mft record 1)\n");
    m = (MFT_RECORD*)(gsBuf + 1 * gsVol->mft_record_size);
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE,
            const_cpu_to_le16(0), gsRlMftmirr, gsBuf,
            gsRlMftmirr[0].length * gsVol->cluster_size);
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_MFTMirr, FILE_MFTMirr),
                gsRlMftmirr[0].length * gsVol->cluster_size,
                gsRlMftmirr[0].length * gsVol->cluster_size,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$MFTMirr", FILE_NAME_WIN32_AND_DOS);
    if (err < 0) {
        ntfs_log_error("Couldn't create $MFTMirr: %s\n",
                strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $LogFile (mft record 2)\n");
    m = (MFT_RECORD*)(gsBuf + 2 * gsVol->mft_record_size);
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE,
            const_cpu_to_le16(0), gsRlLogfile,
            (const u8*)NULL, gsLogfileSize);
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_LogFile, FILE_LogFile),
                gsLogfileSize, gsLogfileSize,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$LogFile", FILE_NAME_WIN32_AND_DOS);
    if (err < 0) {
        ntfs_log_error("Couldn't create $LogFile: %s\n",
                strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $AttrDef (mft record 4)\n");
    m = (MFT_RECORD*)(gsBuf + 4 * gsVol->mft_record_size);
    err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0),
            (u8*)gsVol->attrdef, gsVol->attrdef_len);
    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_AttrDef, FILE_AttrDef),
                (gsVol->attrdef_len + gsVol->cluster_size - 1) &
                ~(gsVol->cluster_size - 1), gsVol->attrdef_len,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$AttrDef", FILE_NAME_WIN32_AND_DOS);
    if (!err) {
        init_system_file_sd(FILE_AttrDef, &sd, &i);
        err = add_attr_sd(m, sd, i);
    }
    if (err < 0) {
        ntfs_log_error("Couldn't create $AttrDef: %s\n",
                strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $Bitmap (mft record 6)\n");
    m = (MFT_RECORD*)(gsBuf + 6 * gsVol->mft_record_size);
    /* the data attribute of $Bitmap must be non-resident or otherwise */
    /* windows 2003 will regard the volume as corrupt (ERSO) */
    if (!err)
        err = insert_non_resident_attr_in_mft_record(m,
            AT_DATA,  NULL, 0, CASE_SENSITIVE,
            const_cpu_to_le16(0), (const u8*)NULL,
            gsLcnBitmapByteSize, WRITE_BITMAP);


    if (!err)
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_Bitmap, FILE_Bitmap),
                (gsLcnBitmapByteSize + gsVol->cluster_size -
                1) & ~(gsVol->cluster_size - 1),
                gsLcnBitmapByteSize,
                FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
                "$Bitmap", FILE_NAME_WIN32_AND_DOS);
    if (err < 0) {
        ntfs_log_error("Couldn't create $Bitmap: %s\n", strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $Boot (mft record 7)\n");
    m = (MFT_RECORD*)(gsBuf + 7 * gsVol->mft_record_size);
    bs = ntfs_calloc(8192);
    if (!bs)
        return FALSE;
    memcpy(bs, boot_array, sizeof(boot_array));
    /*
     * Create the boot sector in bs. Note, that bs is already zeroed
     * in the boot sector section and that it has the NTFS OEM id/magic
     * already inserted, so no need to worry about these things.
     */
    bs->bpb.bytes_per_sector = cpu_to_le16(opts.sectorSize);
    sectors_per_cluster = gsVol->cluster_size / opts.sectorSize;
    if (sectors_per_cluster > 128)
        bs->bpb.sectors_per_cluster = 257 - ffs(sectors_per_cluster);
    else
        bs->bpb.sectors_per_cluster = sectors_per_cluster;
    bs->bpb.media_type = 0xf8; /* hard disk */
    bs->bpb.sectors_per_track = cpu_to_le16(opts.sectorsPerTrack);
    ntfs_log_debug("sectors per track = %ld (0x%lx)\n",
            opts.sectorsPerTrack, opts.sectorsPerTrack);
    bs->bpb.heads = cpu_to_le16(opts.heads);
    ntfs_log_debug("heads = %ld (0x%lx)\n", opts.heads, opts.heads);
    bs->bpb.hidden_sectors = cpu_to_le32(opts.partStartSect);
    ntfs_log_debug("hidden sectors = %llu (0x%llx)\n", opts.partStartSect,
            opts.partStartSect);
    bs->physical_drive = 0x80;          /* boot from hard disk */
    bs->extended_boot_signature = 0x80; /* everybody sets this, so we do */
    bs->number_of_sectors = cpu_to_sle64(opts.numSectors);
    bs->mft_lcn = cpu_to_sle64(gsMftLcn);
    bs->mftmirr_lcn = cpu_to_sle64(gsMftmirrLcn);
    if (gsVol->mft_record_size >= gsVol->cluster_size) {
        bs->clusters_per_mft_record = gsVol->mft_record_size /
            gsVol->cluster_size;
    } else {
        bs->clusters_per_mft_record = -(ffs(gsVol->mft_record_size) -
                1);
        if ((u32)(1 << -bs->clusters_per_mft_record) !=
                gsVol->mft_record_size) {
            free(bs);
            ntfs_log_error("BUG: calculated clusters_per_mft_record"
                    " is wrong (= 0x%x)\n",
                    bs->clusters_per_mft_record);
            return FALSE;
        }
    }
    ntfs_log_debug("clusters per mft record = %i (0x%x)\n",
            bs->clusters_per_mft_record,
            bs->clusters_per_mft_record);
    if (gsVol->indx_record_size >= gsVol->cluster_size) {
        bs->clusters_per_index_record = gsVol->indx_record_size /
            gsVol->cluster_size;
    } else {
        bs->clusters_per_index_record = -gsVol->indx_record_size_bits;
        if ((1 << -bs->clusters_per_index_record) !=
                (s32)gsVol->indx_record_size) {
            free(bs);
            ntfs_log_error("BUG: calculated "
                    "clusters_per_index_record is wrong "
                    "(= 0x%x)\n",
                    bs->clusters_per_index_record);
            return FALSE;
        }
    }
    ntfs_log_debug("clusters per index block = %i (0x%x)\n",
            bs->clusters_per_index_record,
            bs->clusters_per_index_record);
    /* Generate a 64-bit random number for the serial number. */
    bs->volume_serial_number = cpu_to_le64(((u64)random() << 32) |
            ((u64)random() & 0xffffffff));
    /*
     * Leave zero for now as NT4 leaves it zero, too. If want it later, see
     * ../libntfs/bootsect.c for how to calculate it.
     */
    bs->checksum = const_cpu_to_le32(0);
    /* Make sure the bootsector is ok. */
    if (!ntfs_boot_sector_is_ntfs(bs)) {
        free(bs);
        ntfs_log_error("FATAL: Generated boot sector is invalid!\n");
        return FALSE;
    }
    err = add_attr_data_positioned(m, NULL, 0, CASE_SENSITIVE,
            const_cpu_to_le16(0), gsRlBoot, (u8*)bs, 8192);
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
        ntfs_log_error("Couldn't create $Boot: %s\n", strerror(-err));
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
    if (!create_file_volume(m, root_ref, volume_flags, &vol_guid))
        return FALSE;
    ntfs_log_verbose("Creating $BadClus (mft record 8)\n");
    m = (MFT_RECORD*)(gsBuf + 8 * gsVol->mft_record_size);
    /* FIXME: This should be IGNORE_CASE */
    /* Create a sparse named stream of size equal to the volume size. */
    err = add_attr_data_positioned(m, "$Bad", 4, CASE_SENSITIVE,
            const_cpu_to_le16(0), gsRlBad, NULL,
            gsVol->nr_clusters * gsVol->cluster_size);
    if (!err) {
        err = add_attr_data(m, NULL, 0, CASE_SENSITIVE,
                const_cpu_to_le16(0), NULL, 0);
    }
    if (!err) {
        err = create_hardlink(gsIndexBlock, root_ref, m,
                MK_LE_MREF(FILE_BadClus, FILE_BadClus),
                0LL, 0LL, FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM,
                0, 0, "$BadClus", FILE_NAME_WIN32_AND_DOS);
    }
    if (err < 0) {
        ntfs_log_error("Couldn't create $BadClus: %s\n",
                strerror(-err));
        return FALSE;
    }
    /* create $Secure (NTFS 3.0+) */
    ntfs_log_verbose("Creating $Secure (mft record 9)\n");
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
        if (!buf_sds)
            return FALSE;
        init_secure_sds(buf_sds);
        memcpy(buf_sds + 0x40000, buf_sds, buf_sds_first_size);
        err = add_attr_data(m, "$SDS", 4, CASE_SENSITIVE,
                const_cpu_to_le16(0), (u8*)buf_sds,
                buf_sds_size);
    }
    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$SDH", 4, CASE_SENSITIVE,
            AT_UNUSED, COLLATION_NTOFS_SECURITY_HASH,
            gsVol->indx_record_size);
    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$SII", 4, CASE_SENSITIVE,
            AT_UNUSED, COLLATION_NTOFS_ULONG,
            gsVol->indx_record_size);
    if (!err)
        err = initialize_secure(buf_sds, buf_sds_first_size, m);
    free(buf_sds);
    if (err < 0) {
        ntfs_log_error("Couldn't create $Secure: %s\n",
            strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $UpCase (mft record 0xa)\n");
    m = (MFT_RECORD*)(gsBuf + 0xa * gsVol->mft_record_size);
    err = add_attr_data(m, NULL, 0, CASE_SENSITIVE, const_cpu_to_le16(0),
            (u8*)gsVol->upcase, gsVol->upcase_len << 1);
    /*
     * The $Info only exists since Windows 8, but it apparently
     * does not disturb chkdsk from earlier versions.
     */
    if (!err)
        err = add_attr_data(m, "$Info", 5, CASE_SENSITIVE,
            const_cpu_to_le16(0),
            (u8*)gsUpcaseInfo, sizeof(struct UPCASEINFO));
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
        ntfs_log_error("Couldn't create $UpCase: %s\n", strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $Extend (mft record 11)\n");
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
    if (!err)
        err = add_attr_index_root(m, "$I30", 4, CASE_SENSITIVE,
            AT_FILE_NAME, COLLATION_FILE_NAME,
            gsVol->indx_record_size);
    if (err < 0) {
        ntfs_log_error("Couldn't create $Extend: %s\n",
            strerror(-err));
        return FALSE;
    }
    /* NTFS reserved system files (mft records 0xc-0xf) */
    for (i = 0xc; i < 0x10; i++) {
        ntfs_log_verbose("Creating system file (mft record 0x%x)\n", i);
        m = (MFT_RECORD*)(gsBuf + i * gsVol->mft_record_size);
        err = add_attr_data(m, NULL, 0, CASE_SENSITIVE,
                const_cpu_to_le16(0), NULL, 0);
        if (!err) {
            init_system_file_sd(i, &sd, &j);
            err = add_attr_sd(m, sd, j);
        }
        if (err < 0) {
            ntfs_log_error("Couldn't create system file %i (0x%x): "
                    "%s\n", i, i, strerror(-err));
            return FALSE;
        }
    }
    /* create systemfiles for ntfs volumes (3.1) */
    /* starting with file 24 (ignoring file 16-23) */
    extend_flags = FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
        FILE_ATTR_ARCHIVE | FILE_ATTR_VIEW_INDEX_PRESENT;
    ntfs_log_verbose("Creating $Quota (mft record 24)\n");
    m = (MFT_RECORD*)(gsBuf + 24 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_4;
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err)
        err = create_hardlink_res((MFT_RECORD*)(gsBuf +
            11 * gsVol->mft_record_size), extend_ref, m,
            MK_LE_MREF(24, 1), 0LL, 0LL, extend_flags,
            0, 0, "$Quota", FILE_NAME_WIN32_AND_DOS);
    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$Q", 2, CASE_SENSITIVE, AT_UNUSED,
            COLLATION_NTOFS_ULONG, gsVol->indx_record_size);
    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$O", 2, CASE_SENSITIVE, AT_UNUSED,
            COLLATION_NTOFS_SID, gsVol->indx_record_size);
    if (!err)
        err = initialize_quota(m);
    if (err < 0) {
        ntfs_log_error("Couldn't create $Quota: %s\n", strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $ObjId (mft record 25)\n");
    m = (MFT_RECORD*)(gsBuf + 25 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_4;
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err)
        err = create_hardlink_res((MFT_RECORD*)(gsBuf +
                11 * gsVol->mft_record_size), extend_ref,
                m, MK_LE_MREF(25, 1), 0LL, 0LL,
                extend_flags, 0, 0, "$ObjId",
                FILE_NAME_WIN32_AND_DOS);

    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$O", 2, CASE_SENSITIVE, AT_UNUSED,
            COLLATION_NTOFS_ULONGS,
            gsVol->indx_record_size);
    if (err < 0) {
        ntfs_log_error("Couldn't create $ObjId: %s\n",
                strerror(-err));
        return FALSE;
    }
    ntfs_log_verbose("Creating $Reparse (mft record 26)\n");
    m = (MFT_RECORD*)(gsBuf + 26 * gsVol->mft_record_size);
    m->flags |= MFT_RECORD_IS_4;
    m->flags |= MFT_RECORD_IS_VIEW_INDEX;
    if (!err)
        err = create_hardlink_res((MFT_RECORD*)(gsBuf + 11 * gsVol->mft_record_size),
                extend_ref, m, MK_LE_MREF(26, 1),
                0LL, 0LL, extend_flags, 0, 0,
                "$Reparse", FILE_NAME_WIN32_AND_DOS);
    /* FIXME: This should be IGNORE_CASE */
    if (!err)
        err = add_attr_index_root(m, "$R", 2, CASE_SENSITIVE, AT_UNUSED,
            COLLATION_NTOFS_ULONGS, gsVol->indx_record_size);
    if (err < 0) {
        ntfs_log_error("Couldn't create $Reparse: %s\n",
            strerror(-err));
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
        check_failed("Failed to read boot sector.\n");
        return 1;
    }

    if ((buf[0]!=0xeb) || ((buf[1]!=0x52) && (buf[1]!=0x5b)) || (buf[2]!=0x90)) {
        check_failed("Boot sector: Bad jump.\n");
    }

    if (ntfs_boot->oem_id != magicNTFS) {
        check_failed("Boot sector: Bad NTFS magic.\n");
    }

    gsBytesPerSector = le16_to_cpu(ntfs_boot->bpb.bytes_per_sector);
    if (!gsBytesPerSector) {
        check_failed("Boot sector: Bytes per sector is 0.\n");
    }

    if (gsBytesPerSector % 512) {
        check_failed("Boot sector: Bytes per sector is not a multiple of 512.\n");
    }
    gsSectorsPerCluster = ntfs_boot->bpb.sectors_per_cluster;

    // todo: if partition, query bios and match heads/tracks? */

    // Initialize some values into rawvol. We will need those later.
    rawvol->dev = dev;
    ntfs_boot_sector_parse(rawvol, (NTFS_BOOT_SECTOR *)buf);

    return 0;
}

static void check_volume(ntfs_volume *vol)
{
    s64 mft_num, nr_mft_records;

    ntfs_log_warning("Unsupported: check_volume()\n");

    // For each mft record, verify that it contains a valid file record.
    nr_mft_records = vol->mft_na->initialized_size >> vol->mft_record_size_bits;
    ntfs_log_info("Checking %lld MFT records.\n", (long long)nr_mft_records);

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

    if (!(vol->flags | VOLUME_IS_DIRTY))
        return 0;

    ntfs_log_verbose("Resetting dirty flag.\n");

    flags = vol->flags & ~VOLUME_IS_DIRTY;

    if (ntfs_volume_write_flags(vol, flags)) {
        ntfs_log_error("Error setting volume flags.\n");
        return -1;
    }
    return 0;
}

static int verify_mft_preliminary(ntfs_volume *rawvol)
{
    gsCurrentMftRecord = 0;
    s64 mft_offset, mftmirr_offset;
    int res;

    ntfs_log_trace("Entering verify_mft_preliminary().\n");
    // todo: get size_of_file_record from boot sector
    // Load the first segment of the $MFT/DATA runlist.
    mft_offset = rawvol->mft_lcn * rawvol->cluster_size;
    mftmirr_offset = rawvol->mftmirr_lcn * rawvol->cluster_size;
    gsMftRl = load_runlist(rawvol, mft_offset, AT_DATA, 1024);
    if (!gsMftRl) {
        check_failed("Loading $MFT runlist failed. Trying $MFTMirr.\n");
        gsMftRl = load_runlist(rawvol, mftmirr_offset, AT_DATA, 1024);
    }
    if (!gsMftRl) {
        check_failed("Loading $MFTMirr runlist failed too. Aborting.\n");
        return RETURN_FS_ERRORS_LEFT_UNCORRECTED | RETURN_OPERATIONAL_ERROR;
    }
    // TODO: else { recover $MFT } // Use $MFTMirr to recover $MFT.
    // todo: support loading the next runlist extents when ATTRIBUTE_LIST is used on $MFT.
    // If attribute list: Gradually load mft runlist. (parse runlist from first file record, check all referenced file records, continue with the next file record). If no attribute list, just load it.

    // Load the runlist of $MFT/Bitmap.
    // todo: what about ATTRIBUTE_LIST? Can we reuse code?
    gsMftBitmapRl = load_runlist(rawvol, mft_offset, AT_BITMAP, 1024);
    if (!gsMftBitmapRl) {
        check_failed("Loading $MFT/Bitmap runlist failed. Trying $MFTMirr.\n");
        gsMftBitmapRl = load_runlist(rawvol, mftmirr_offset, AT_BITMAP, 1024);
    }
    if (!gsMftBitmapRl) {
        check_failed("Loading $MFTMirr/Bitmap runlist failed too. Aborting.\n");
        return RETURN_FS_ERRORS_LEFT_UNCORRECTED;
        // todo: rebuild the bitmap by using the "in_use" file record flag or by filling it with 1's.
    }

    /* Load $MFT/Bitmap */
    if ((res = mft_bitmap_load(rawvol)))
        return res;
    return -1; /* FIXME: Just added to fix compiler warning without thinking about what should be here.  (Yura) */
}

static void verify_mft_record(ntfs_volume *vol, s64 mft_num)
{
    u8 *buffer;
    int is_used;

    gsCurrentMftRecord = mft_num;

    is_used = mft_bitmap_get_bit(mft_num);
    if (is_used<0) {
        ntfs_log_error("Error getting bit value for record %lld.\n",
            (long long)mft_num);
    } else if (!is_used) {
        ntfs_log_verbose("Record %lld unused. Skipping.\n", (long long)mft_num);
        return;
    }

    buffer = ntfs_malloc(vol->mft_record_size);
    if (!buffer)
        goto verify_mft_record_error;

    ntfs_log_verbose("MFT record %lld\n", (long long)mft_num);
    if (ntfs_attr_pread(vol->mft_na, mft_num*vol->mft_record_size, vol->mft_record_size, buffer) < 0) {
        ntfs_log_perror("Couldn't read $MFT record %lld", (long long)mft_num);
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
    ntfs_log_warning("Unsupported: replay_log()\n");
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
        check_failed("Unknown MFT record flags (0x%x).\n", (unsigned int)le16_to_cpu(mft_rec->flags));
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
            check_failed("Attribute 0x%x is not AT_END, yet no "
                    "room for the length field.\n",
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
        check_failed("Attribute (0x%x) is larger than FILE record (%lld).\n",
                (int)attr_type, (long long)gsCurrentMftRecord);
        return NULL;
    }

    // Attr type must be a multiple of 0x10 and 0x10<=x<=0x100.
    if ((attr_type & ~0x0F0) && (attr_type != 0x100)) {
        check_failed("Unknown attribute type 0x%x.\n",
            (int)attr_type);
        goto check_attr_record_next_attr;
    }

    if (length<24) {
        check_failed("Attribute %lld:0x%x Length too short (%u).\n",
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
    //printf("Attribute %lld:0x%x instance: %u isbase:%d.\n",
    //        current_mft_record, (int)attr_type, (int)le16_to_cpu(attr_rec->instance), (int)mft_rec->base_mft_record);
    // todo: instance is unique.

    // Check flags.
    if (attr_rec->flags & ~(const_cpu_to_le16(0xc0ff))) {
        check_failed("Attribute %lld:0x%x Unknown flags (0x%x).\n",
            (long long)gsCurrentMftRecord, (int)attr_type,
            (int)le16_to_cpu(attr_rec->flags));
    }

    if (attr_rec->non_resident>1) {
        check_failed("Attribute %lld:0x%x Unknown non-resident "
            "flag (0x%x).\n", (long long)gsCurrentMftRecord,
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
            check_failed("Non-resident attribute %lld:0x%x too short (%u).\n",
                (long long)gsCurrentMftRecord, (int)attr_type,
                (int)length);
            goto check_attr_record_next_attr;
        }
        if (attr_rec->compression_unit && (length<72)) {
            check_failed("Compressed attribute %lld:0x%x too short (%u).\n",
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
                check_failed("Resident attribute with "
                    "name intersecting header.\n");
            if (value_offset < name_offset +
                    attr_rec->name_length)
                check_failed("Named resident attribute "
                    "with value before name.\n");
        }
        // if resident, length==value_length+value_offset
        //assert_u32_equal(le32_to_cpu(attr_rec->value_length)+
        //    value_offset, length,
        //    "length==value_length+value_offset");
        // if resident, length==value_length+value_offset
        if (value_length+value_offset > length) {
            check_failed("value_length(%d)+value_offset(%d)>length(%d) for attribute 0x%x.\n", (int)value_length, (int)value_offset, (int)length, (int)attr_type);
            return NULL;
        }

        // Check resident_flags.
        if (attr_rec->resident_flags>0x01) {
            check_failed("Unknown resident flags (0x%x) for attribute 0x%x.\n", (int)attr_rec->resident_flags, (int)attr_type);
        } else if (attr_rec->resident_flags && (attr_type!=0x30)) {
            check_failed("Resident flags mark attribute 0x%x as indexed.\n", (int)attr_type);
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
        /* This case should not happen, not even with on-disk errors */
        goto error;
    }

    mft_bitmap_length = vcn * rawvol->cluster_size;
    gsMftBitmapRecords = 8 * mft_bitmap_length * rawvol->cluster_size /
        rawvol->mft_record_size;

    gsMftBitmapBuf = (u8*)ntfs_malloc(mft_bitmap_length);
    if (!gsMftBitmapBuf)
        goto error;
    if (ntfs_rl_pread(rawvol, gsMftBitmapRl, 0, mft_bitmap_length, gsMftBitmapBuf)!=mft_bitmap_length)
        goto error;
    return 0;
    error:
        gsMftBitmapRecords = 0;
    ntfs_log_error("Could not load $MFT/Bitmap.\n");
    return RETURN_OPERATIONAL_ERROR;
}

static VCN get_last_vcn(runlist *rl)
{
    VCN res;

    if (!rl)
        return LCN_EINVAL;

    res = LCN_EINVAL;
    while (rl->length) {
        ntfs_log_verbose("vcn: %lld, length: %lld.\n", (long long)rl->vcn, (long long)rl->length);
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
        check_failed("Failed to read file record at offset %lld (0x%llx).\n", (long long)offset_to_file_record, (long long)offset_to_file_record);
        return NULL;
    }

    attrs_offset = le16_to_cpu(((MFT_RECORD*)buf)->attrs_offset);
    // first attribute must be after the header.
    if (attrs_offset<42) {
        check_failed("First attribute must be after the header (%u).\n", (int)attrs_offset);
    }
    attr_rec = (ATTR_RECORD *)(buf + attrs_offset);
    //printf("uv1.\n");

    while ((u8*)attr_rec<=buf+size_of_file_record-4) {

        //printf("Attr type: 0x%x.\n", attr_rec->type);
        // Check attribute record. (Only what is in the buffer)
        if (attr_rec->type==AT_END) {
            check_failed("Attribute 0x%x not found in file record at offset %lld (0x%llx).\n", (int)le32_to_cpu(attr_rec->type),
                    (long long)offset_to_file_record,
                    (long long)offset_to_file_record);
            return NULL;
        }
        if ((u8*)attr_rec>buf+size_of_file_record-8) {
            // not AT_END yet no room for the length field.
            check_failed("Attribute 0x%x is not AT_END, yet no room for the length field.\n", (int)le32_to_cpu(attr_rec->type));
            return NULL;
        }

        length = le32_to_cpu(attr_rec->length);

        // Check that this attribute does not overflow the mft_record
        if ((u8*)attr_rec+length >= buf+size_of_file_record) {
            check_failed("Attribute (0x%x) is larger than FILE record at offset %lld (0x%llx).\n",
                    (int)le32_to_cpu(attr_rec->type),
                    (long long)offset_to_file_record,
                    (long long)offset_to_file_record);
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
    check_failed("file record corrupted at offset %lld (0x%llx).\n", (long long)offset_to_file_record, (long long)offset_to_file_record);

    return NULL;
}

static int assert_u32_equal(u32 val, u32 ok, const char *name)
{
    if (val!=ok) {
        check_failed("Assertion failed for '%lld:%s'. should be 0x%x, "
            "was 0x%x.\n", (long long)gsCurrentMftRecord, name,
            (int)ok, (int)val);
        //errors++;
        return 1;
    }
    return 0;
}

static int assert_u32_noteq(u32 val, u32 wrong, const char *name)
{
    if (val==wrong) {
        check_failed("Assertion failed for '%lld:%s'. should not be "
            "0x%x.\n", (long long)gsCurrentMftRecord, name,
            (int)wrong);
        return 1;
    }
    return 0;
}

static int assert_u32_lesseq(u32 val1, u32 val2, const char *name)
{
    if (val1 > val2) {
        check_failed("Assertion failed for '%s'. 0x%x > 0x%x\n", name, (int)val1, (int)val2);
        //errors++;
        return 1;
    }
    return 0;
}

static int assert_u32_less(u32 val1, u32 val2, const char *name)
{
    if (val1 >= val2) {
        check_failed("Assertion failed for '%s'. 0x%x >= 0x%x\n",
            name, (int)val1, (int)val2);
        //errors++;
        return 1;
    }
    return 0;
}