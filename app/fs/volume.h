//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_VOLUME_H
#define sandbox_VOLUME_H
#include <c/clib.h>


C_BEGIN_EXTERN_C

#define FS_BUF_SIZE                     8192

/*
 * NTFS version 1.1 and 1.2 are used by Windows NT4.
 * NTFS version 2.x is used by Windows 2000 Beta
 * NTFS version 3.0 is used by Windows 2000.
 * NTFS version 3.1 is used by Windows XP, 2003 and Vista.
 */
#define NTFS_V1_1(major, minor) ((major) == 1 && (minor) == 1)
#define NTFS_V1_2(major, minor) ((major) == 1 && (minor) == 2)
#define NTFS_V2_X(major, minor) ((major) == 2)
#define NTFS_V3_0(major, minor) ((major) == 3 && (minor) == 0)
#define NTFS_V3_1(major, minor) ((major) == 3 && (minor) == 1)

#define test_nvol_flag(nv, flag)        test_bit(NV_##flag, (nv)->state)
#define set_nvol_flag(nv, flag)         set_bit(NV_##flag, (nv)->state)
#define clear_nvol_flag(nv, flag)       clear_bit(NV_##flag, (nv)->state)

#define NVolReadOnly(nv)                 test_nvol_flag(nv, ReadOnly)
#define NVolSetReadOnly(nv)               set_nvol_flag(nv, ReadOnly)
#define NVolClearReadOnly(nv)           clear_nvol_flag(nv, ReadOnly)

#define NVolCaseSensitive(nv)            test_nvol_flag(nv, CaseSensitive)
#define NVolSetCaseSensitive(nv)          set_nvol_flag(nv, CaseSensitive)
#define NVolClearCaseSensitive(nv)      clear_nvol_flag(nv, CaseSensitive)

#define NVolLogFileEmpty(nv)             test_nvol_flag(nv, LogFileEmpty)
#define NVolSetLogFileEmpty(nv)           set_nvol_flag(nv, LogFileEmpty)
#define NVolClearLogFileEmpty(nv)       clear_nvol_flag(nv, LogFileEmpty)

#define NVolShowSysFiles(nv)             test_nvol_flag(nv, ShowSysFiles)
#define NVolSetShowSysFiles(nv)           set_nvol_flag(nv, ShowSysFiles)
#define NVolClearShowSysFiles(nv)       clear_nvol_flag(nv, ShowSysFiles)

#define NVolShowHidFiles(nv)             test_nvol_flag(nv, ShowHidFiles)
#define NVolSetShowHidFiles(nv)           set_nvol_flag(nv, ShowHidFiles)
#define NVolClearShowHidFiles(nv)       clear_nvol_flag(nv, ShowHidFiles)

#define NVolHideDotFiles(nv)             test_nvol_flag(nv, HideDotFiles)
#define NVolSetHideDotFiles(nv)           set_nvol_flag(nv, HideDotFiles)
#define NVolClearHideDotFiles(nv)       clear_nvol_flag(nv, HideDotFiles)

#define NVolCompression(nv)              test_nvol_flag(nv, Compression)
#define NVolSetCompression(nv)            set_nvol_flag(nv, Compression)
#define NVolClearCompression(nv)        clear_nvol_flag(nv, Compression)

#define NVolNoFixupWarn(nv)              test_nvol_flag(nv, NoFixupWarn)
#define NVolSetNoFixupWarn(nv)            set_nvol_flag(nv, NoFixupWarn)
#define NVolClearNoFixupWarn(nv)        clear_nvol_flag(nv, NoFixupWarn)

#define NVolFreeSpaceKnown(nv)           test_nvol_flag(nv, FreeSpaceKnown)
#define NVolSetFreeSpaceKnown(nv)         set_nvol_flag(nv, FreeSpaceKnown)
#define NVolClearFreeSpaceKnown(nv)     clear_nvol_flag(nv, FreeSpaceKnown)


typedef cuint16                         FSChar;
typedef struct _FSAttr                  FSAttr;
typedef struct _FSInode                 FSInode;
typedef struct _FSDevice                FSDevice;
typedef struct _FSVolume                FSVolume;
typedef struct _FSAttrDef               FSAttrDef;
typedef struct _FSIndexContext          FSIndexContext;

typedef enum
{
    FS_MNT_NONE                 = 0x00000000,
    FS_MNT_READONLY             = 0x00000001,
    FS_MNT_MAY_READONLY         = 0x02000000,
    FS_MNT_FORENSIC             = 0x04000000,
    FS_MNT_EXCLUSIVE            = 0x08000000,
    FS_MNT_RECOVER              = 0x10000000,
    FS_MNT_IGNORE_HIBERFILE     = 0x20000000,
}FSMountFlags;

typedef enum
{
    FS_MF_MOUNTED               = 1,    /* Device is mounted. */
    FS_MF_ISROOT                = 2,    /* Device is mounted as system root. */
    FS_MF_READONLY              = 4,    /* Device is mounted read-only. */
};

typedef enum {
    FS_VOLUME_OK                = 0,
    FS_VOLUME_SYNTAX_ERROR      = 11,
    FS_VOLUME_NOT_NTFS          = 12,
    FS_VOLUME_CORRUPT           = 13,
    FS_VOLUME_HIBERNATED        = 14,
    FS_VOLUME_UNCLEAN_UNMOUNT   = 15,
    FS_VOLUME_LOCKED            = 16,
    FS_VOLUME_RAID              = 17,
    FS_VOLUME_UNKNOWN_REASON    = 18,
    FS_VOLUME_NO_PRIVILEGE      = 19,
    FS_VOLUME_OUT_OF_MEMORY     = 20,
    FS_VOLUME_FUSE_ERROR        = 21,
    FS_VOLUME_INSECURE          = 22
} FSVolumeStatus;

typedef enum
{
    FS_VOLUME_READONLY,            /* 1: Volume is read-only. */
    FS_VOLUME_CASE_SENSITIVE,      /* 1: Volume is mounted case-sensitive. */
    FS_VOLUME_LOG_FILE_EMPTY,      /* 1: $logFile journal is empty. */
    FS_VOLUME_SHOW_SYS_FILES,      /* 1: Show NTFS metafiles. */
    FS_VOLUME_SHOW_HIDE_FILES,     /* 1: Show files marked hidden. */
    FS_VOLUME_HIDE_DOT_FILES,      /* 1: Set hidden flag on dot files */
    FS_VOLUME_COMPRESSION,         /* 1: allow compression */
    FS_VOLUME_NO_FIXUP_WARN,       /* 1: Do not log fixup errors */
    FS_VOLUME_FREE_SPACE_KNOWN,    /* 1: The free space is now known */
} FSVolumeStateBits;

typedef enum
{
    FS_FILES_INTERIX,
    FS_FILES_WSL,
} FSVolumeSpecialFiles;


struct _FSVolume
{
    union {
        FSDevice*   dev;                    /* FS device associated with the volume. */
        void*       sb;                     /* For kernel porting compatibility. */
    };
    char*                   volName;                /* Name of the volume. */
    unsigned long           state;                  /* FS specific flags describing this volume. See fs_volume_state_bits above. */

    FSInode*                volNi;                  /* fs_inode structure for FILE_Volume. */
    cuint8                  majorVer;               /* fs major version of volume. */
    cuint8                  minorVer;               /* Ntfs minor version of volume. */
    cuint16                 flags;                  /* Bit array of VOLUME_* flags. */

    cuint16                 sectorSize;             /* Byte size of a sector. */
    cuint8                  sectorSizeBits;         /* Log(2) of the byte size of a sector. */
    cuint32                 clusterSize;            /* Byte size of a cluster. */
    cuint32                 mftRecordSize;          /* Byte size of a mft record. */
    cuint32                 indxRecordSize;         /* Byte size of a INDX record. */
    cuint8                  clusterSizeBits;        /* Log(2) of the byte size of a cluster. */
    cuint8                  mftRecordSizeBits;      /* Log(2) of the byte size of a mft record. */
    cuint8                  indxRecordSizeBits;     /* Log(2) of the byte size of a INDX record. */

    /* Variables used by the cluster and mft allocators. */
    cuint8                  mftZoneMultiplier;      /* Initial mft zone multiplier. */
    cuint8                  fullZones;              /* cluster zones which are full */
    cint16                  mftDataPos;             /* Mft record number at which to allocate the next mft record. */
    cint64                  mftZoneStart;           /* First cluster of the mft zone. */
    cint64                  mftZoneEnd;             /* First cluster beyond the mft zone. */
    cint64                  mftZonePos;             /* Current position in the mft zone. */
    cint64                  data1ZonePos;           /* Current position in the first data zone. */
    cint64                  data2ZonePos;           /* Current position in the second data zone. */

    cint64                  nrClusters;             /* Volume size in clusters, hence also the number of bits in lcn_bitmap. */
    FSInode*                lcnbmpNi;               /* fs_inode structure for FILE_Bitmap. */
    FSAttr*                 lcnbmpNa;               /* fs_attr structure for the data attribute of FILE_Bitmap. Each bit represents a cluster on the volume, bit 0 representing lcn 0 and so on. A set bit means that the cluster and vice versa. */

    cint64                  mftLcn;                 /* Logical cluster number of the data attribute for FILE_MFT. */
    FSInode*                mftNi;                  /* fs_inode structure for FILE_MFT. */
    FSAttr*                 mftNa;                  /* fs_attr structure for the data attribute of FILE_MFT. */
    FSAttr*                 mftbmpNa;               /* fs_attr structure for the bitmap attribute of FILE_MFT. Each bit represents an mft record in the $DATA attribute, bit 0 representing mft record 0 and so on. A set bit means that the mft record is in use and vice versa. */

    FSInode*                secureNi;               /* fs_inode structure for FILE $Secure */
    FSIndexContext*         secureXsii;             /* index for using $Secure:$SII */
    FSIndexContext*         secureXsdh;             /* index for using $Secure:$SDH */
    int                     secureReentry;          /* check for non-rentries */
    unsigned int            secureFlags;            /* flags, see security.h for values */

    int                     mftmirrSize;            /* Size of the FILE_MFTMirr in mft records. */
    cint64                  mftmirrLcn;             /* Logical cluster number of the data attribute for FILE_MFTMirr. */
    FSInode*                mftmirrNi;              /* ntfs_inode structure for FILE_MFTMirr. */
    FSAttr*                 mftmirrNa;              /* ntfs_attr structure for the data attribute of FILE_MFTMirr. */

    FSChar*                 upcase;                 /* Upper case equivalents of all 65536 2-byte Unicode characters. Obtained from FILE_UpCase. */
    cint32                  upcaseLen;               /* Length in Unicode characters of the upcase table. */
    FSChar*                 locase;                 /* Lower case equivalents of all 65536 2-byte Unicode characters. Only if option case_ignore is set. */

    FSAttrDef*              attrdef;                /* Attribute definitions. Obtained from FILE_AttrDef. */
    cint32                  attrdefLen;             /* Size of the attribute definition table in bytes. */

    cint64                  freeClusters;           /* Track the number of free clusters which greatly improves statfs() performance */
    cint64                  freeMftRecords;         /* Same for free mft records (see above) */
    bool                    efsRaw;                 /* volume is mounted for raw access to efs-encrypted files */
    FSVolumeSpecialFiles    specialFiles;           /* Implementation of special files */
    const char*             absMntPoint;            /* Mount point */
#ifdef XATTR_MAPPINGS
    struct XATTRMAPPING *xattr_mapping;
#endif /* XATTR_MAPPINGS */
#if CACHE_INODE_SIZE
    struct CACHE_HEADER *xinode_cache;
#endif
#if CACHE_NIDATA_SIZE
    struct CACHE_HEADER *nidata_cache;
#endif
#if CACHE_LOOKUP_SIZE
    struct CACHE_HEADER *lookup_cache;
#endif
#if CACHE_SECURID_SIZE
    struct CACHE_HEADER *securid_cache;
#endif
#if CACHE_LEGACY_SIZE
    struct CACHE_HEADER *legacy_cache;
#endif
};


extern const char* gFSHhome;


FSVolume*       fs_volume_alloc             (void);
FSVolume*       fs_volume_startup           (FSDevice* dev, FSMountFlags flags);
FSVolume*       fs_device_mount             (FSDevice* dev, FSMountFlags flags);
FSVolume*       fs_mount                    (const char *name, FSMountFlags flags);
int             fs_umount                   (FSVolume* vol, const bool force);
int             fs_version_is_supported     (FSVolume* vol);
int             fs_volume_check_hiberfile   (FSVolume* vol, int verbose);
int             fs_logfile_reset            (FSVolume* vol);
int             fs_volume_write_flags       (FSVolume* vol, const cuint16 flags);
int             fs_volume_error             (int err);
void            fs_mount_error              (const char *vol, const char *mntpoint, int err);
int             fs_volume_get_free_space    (FSVolume* vol);
int             fs_volume_rename            (FSVolume* vol, const FSChar* label, int labelLen);
int             fs_set_shown_files          (FSVolume* vol, bool showSysFiles, bool showHideFiles, bool hideDotFiles);
int             fs_set_locale               (void);
int             fs_set_ignore_case          (FSVolume* vol);


C_END_EXTERN_C


#endif // sandbox_VOLUME_H
