//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_LAYOUT_H
#define sandbox_LAYOUT_H

#include <c/clib.h>

C_BEGIN_EXTERN_C

#define FS_BLOCK_SIZE           512
#define FS_BLOCK_SIZE_BITS      9
#define FS_MAX_NAME_LEN         255

#define magicNTFS               (0x202020205346544e)                    /* "NTFS    " */
#define FS_SB_MAGIC             0x5346544e                              /* 'NTFS' */
#define fs_is_magic(x, m)       ((u32)(x) == (u32)magic_##m)
#define fs_is_magicp(p, m)      (*(u32*)(p) == (u32)magic_##m)

#define fs_is_file_record(x)    (fs_is_magic (x, FILE))
#define fs_is_file_recordp(p)   (fs_is_magicp(p, FILE))
#define fs_is_mft_record(x)     (fs_is_file_record(x))
#define fs_is_mft_recordp(p)    (fs_is_file_recordp(p))
#define fs_is_indx_record(x)    (fs_is_magic (x, INDX))
#define fs_is_indx_recordp(p)   (fs_is_magicp(p, INDX))
#define fs_is_hole_record(x)    (fs_is_magic (x, HOLE))
#define fs_is_hole_recordp(p)   (fs_is_magicp(p, HOLE))

#define fs_is_rstr_record(x)    (fs_is_magic (x, RSTR))
#define fs_is_rstr_recordp(p)   (fs_is_magicp(p, RSTR))
#define fs_is_rcrd_record(x)    (fs_is_magic (x, RCRD))
#define fs_is_rcrd_recordp(p)   (fs_is_magicp(p, RCRD))

#define fs_is_chkd_record(x)    (fs_is_magic (x, CHKD))
#define fs_is_chkd_recordp(p)   (fs_is_magicp(p, CHKD))

#define fs_is_baad_record(x)    (fs_is_magic (x, BAAD))
#define fs_is_baad_recordp(p)   (fs_is_magicp(p, BAAD))

#define fs_is_empty_record(x)   (fs_is_magic (x, empty))
#define fs_is_empty_recordp(p)  (fs_is_magicp(p, empty))

#define MFT_REF_MASK_CPU        0x0000ffffffffffffULL
#define MFT_REF_MASK_LE         (cint64)(MFT_REF_MASK_CPU)

typedef cuint64 MFT_REF;
typedef cint64  leMFT_REF;   /* a little-endian MFT_MREF */

#define MK_MREF(m, s)           ((MFT_REF)(((MFT_REF)(s) << 48) | ((MFT_REF)(m) & MFT_REF_MASK_CPU)))
#define MK_LE_MREF(m, s)        (cint64)(((MFT_REF)(((MFT_REF)(s) << 48) | ((MFT_REF)(m) & MFT_REF_MASK_CPU))))


#define MREF(x)                 ((cuint64)((x) & MFT_REF_MASK_CPU))
#define MSEQNO(x)               ((cuint16)(((x) >> 48) & 0xffff))
#define MREF_LE(x)              ((cuint64)((cint64)(x) & MFT_REF_MASK_CPU))
#define MSEQNO_LE(x)            ((cuint16)(((cint64)(x) >> 48) & 0xffff))

#define IS_ERR_MREF(x)          (((x) & 0x0000800000000000ULL) ? 1 : 0)
#define ERR_MREF(x)             ((cuint64)((cint64)(x)))
#define MREF_ERR(x)             ((int)((cint64)(x)))

/*
 * Location of bootsector on partition:
 *   The standard NTFS_BOOT_SECTOR is on sector 0 of the partition.
 *   On NT4 and above there is one backup copy of the boot sector to
 *   be found on the last sector of the partition (not normally accessible
 *   from within Windows as the bootsector contained number of sectors
 *   value is one less than the actual value!).
 *   On versions of NT 3.51 and earlier, the backup copy was located at
 *   number of sectors/2 (integer divide), i.e. in the middle of the volume.
 */

/**
 * struct BIOS_PARAMETER_BLOCK - BIOS parameter block (bpb) structure.
 */
typedef struct
{
    cint16      bytesPerSector;         /* Size of a sector in bytes. */
    cuint8      sectorsPerCluster;      /* Size of a cluster in sectors. */
    cint16      reservedSectors;        /* zero */
    cuint8      fats;                   /* zero */
    cint16      rootEntries;            /* zero */
    cint16      sectors;                /* zero */
    cuint8      mediaType;              /* 0xf8 = hard disk */
    cint16      sectorsPerFat;          /* zero */
    /*0x0d*/
    cint16      sectorsPerTrack;        /* Required to boot Windows. */
    /*0x0f*/
    cint16      heads;                  /* Required to boot Windows. */
    /*0x11*/
    cint32      hiddenSectors;          /* Offset to the start of the partition relative to the disk in sectors. Required to boot Windows. */
    /*0x15*/
    cint32      largeSectors;           /* zero */
    /* sizeof() = 25 (0x19) bytes */
} __attribute__((__packed__)) BIOS_PARAMETER_BLOCK;

/**
 * struct FS_BOOT_SECTOR - FS boot sector structure.
 */
typedef struct
{
    cuint8      jump[3];                    /* Irrelevant (jump to boot up code).*/
    cint64      oemId;                      /* Magic "NTFS    ". */
    /*0x0b*/
    BIOS_PARAMETER_BLOCK bpb;               /* See BIOS_PARAMETER_BLOCK. */
    cuint8      physicalDrive;              /* 0x00 floppy, 0x80 hard disk */
    cuint8      currentHead;                /* zero */
    cuint8      extendedBootSignature;      /* 0x80 */
    cuint8      reserved2;                  /* zero */
    /*0x28*/
    cuint64     numberOfSectors;            /* Number of sectors in volume. Gives maximum volume size of 2^63 sectors. Assuming standard sector size of 512 bytes, the maximum byte size is approx. 4.7x10^21 bytes. (-; */
    cint64      mftLcn;                     /* Cluster location of mft data. */
    cint64      mftmirrLcn;                 /* Cluster location of copy of mft. */
    cint8       clustersPerMftRecord;       /* Mft record size in clusters. */
    cuint8      reserved0[3];               /* zero */
    cint8       clustersPerIndexRecord;     /* Index block size in clusters. */
    cuint8      reserved1[3];               /* zero */
    cint64      volumeSerialNumber;         /* Irrelevant (serial number). */
    cint32      checksum;                   /* Boot sector checksum. */
    /*0x54*/
    cuint8      bootstrap[426];             /* Irrelevant (boot up code). */
    cint16      endOfSectorMarker;          /* End of bootsector magic. Always is 0xaa55 in little endian. */
    /* sizeof() = 512 (0x200) bytes */
} __attribute__((__packed__)) FS_BOOT_SECTOR;

typedef enum
{
    /* Found in $MFT/$DATA. */
    magic_FILE = const_cpu_to_le32(0x454c4946), /* Mft entry. */
    magic_INDX = const_cpu_to_le32(0x58444e49), /* Index buffer. */
    magic_HOLE = const_cpu_to_le32(0x454c4f48), /* ? (NTFS 3.0+?) */

    /* Found in $LogFile/$DATA. */
    magic_RSTR = const_cpu_to_le32(0x52545352), /* Restart page. */
    magic_RCRD = const_cpu_to_le32(0x44524352), /* Log record page. */

    /* Found in $LogFile/$DATA.  (May be found in $MFT/$DATA, also?) */
    magic_CHKD = const_cpu_to_le32(0x444b4843), /* Modified by chkdsk. */

    /* Found in all ntfs record containing records. */
    magic_BAAD = const_cpu_to_le32(0x44414142), /* Failed multi sector transfer was detected. */

    magic_empty = const_cpu_to_le32(0xffffffff),/* Record is empty and has to be initialized before it can be used. */
} FS_RECORD_TYPES;

typedef struct
{
    FS_RECORD_TYPES         magic;              /* A four-byte magic identifying the record type and/or status. */
    cint16                  usaOfs;             /* Offset to the Update Sequence Array (usa) from the start of the ntfs record. */
    cint16                  usaCount;           /* Number of le16 sized entries in the usa including the Update Sequence Number (usn), thus the number of fixups is the usa_count minus 1. */
} __attribute__((__packed__)) FS_RECORD;

typedef enum
{
    FILE_MFT        = 0,    /* Master file table (mft). Data attribute contains the entries and bitmap attribute records which ones are in use (bit==1). */
    FILE_MFTMirr    = 1,    /* Mft mirror: copy of first four mft records in data attribute. If cluster size > 4kiB, copy of first N mft records, with N = cluster_size / mft_record_size. */
    FILE_LogFile    = 2,    /* Journalling log in data attribute. */
    FILE_Volume     = 3,    /* Volume name attribute and volume information attribute (flags and ntfs version). Windows refers to this file as volume DASD (Direct Access Storage Device). */
    FILE_AttrDef    = 4,    /* Array of attribute definitions in data attribute. */
    FILE_root       = 5,    /* Root directory. */
    FILE_Bitmap     = 6,    /* Allocation bitmap of all clusters (lcns) in data attribute. */
    FILE_Boot       = 7,    /* Boot sector (always at cluster 0) in data attribute. */
    FILE_BadClus    = 8,    /* Contains all bad clusters in the non-resident data attribute. */
    FILE_Secure     = 9,    /* Shared security descriptors in data attribute and two indexes into the descriptors. Appeared in Windows 2000. Before that, this file was named $Quota but was unused. */
    FILE_UpCase     = 10,   /* Uppercase equivalents of all 65536 Unicode characters in data attribute. */
    FILE_Extend     = 11,   /* Directory containing other system files (eg. $ObjId, $Quota, $Reparse and $UsnJrnl). This is new to NTFS3.0. */
    FILE_reserved12 = 12,   /* Reserved for future use (records 12-15). */
    FILE_reserved13 = 13,
    FILE_reserved14 = 14,
    FILE_mft_data   = 15,   /* Reserved for first extent of $MFT:$DATA */
    FILE_first_user = 16,   /* First user file, used as test limit for whether to allow opening a file or not. */
} FS_SYSTEM_FILES;

typedef enum
{
    MFT_RECORD_IN_USE               = const_cpu_to_le16(0x0001),
    MFT_RECORD_IS_DIRECTORY         = const_cpu_to_le16(0x0002),
    MFT_RECORD_IS_4                 = const_cpu_to_le16(0x0004),
    MFT_RECORD_IS_VIEW_INDEX        = const_cpu_to_le16(0x0008),
    MFT_REC_SPACE_FILLER            = 0xffff, /* Just to make flags 16-bit. */
} __attribute__((__packed__)) MFT_RECORD_FLAGS;

typedef struct
{
/*Ofs*/
/*  0   FS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
        FS_RECORD_TYPES magic;/* Usually the magic is "FILE". */
        cint16 usa_ofs;           /* See NTFS_RECORD definition above. */
        cint16 usa_count;         /* See NTFS_RECORD definition above. */

/*  8*/ cint64 lsn;              /* $LogFile sequence number for this record.
                                   Changed every time the record is modified. */
/* 16*/ cint16 sequence_number;   /* Number of times this mft record has been
                                   reused. (See description for MFT_REF
                                   above.) NOTE: The increment (skipping zero)
                                   is done when the file is deleted. NOTE: If
                                   this is zero it is left zero. */
/* 18*/ cint16 link_count;                /* Number of hard links, i.e. the number of
                                   directory entries referencing this record.
                                   NOTE: Only used in mft base records.
                                   NOTE: When deleting a directory entry we
                                   check the link_count and if it is 1 we
                                   delete the file. Otherwise we delete the
                                   FILE_NAME_ATTR being referenced by the
                                   directory entry from the mft record and
                                   decrement the link_count.
                                   FIXME: Careful with Win32 + DOS names! */
/* 20*/ cint16 attrs_offset;      /* Byte offset to the first attribute in this
                                   mft record from the start of the mft record.
                                   NOTE: Must be aligned to 8-byte boundary. */
/* 22*/ MFT_RECORD_FLAGS flags; /* Bit array of MFT_RECORD_FLAGS. When a file
                                   is deleted, the MFT_RECORD_IN_USE flag is
                                   set to zero. */
/* 24*/ cint32 bytes_in_use;      /* Number of bytes used in this mft record.
                                   NOTE: Must be aligned to 8-byte boundary. */
/* 28*/ cint32 bytes_allocated;   /* Number of bytes allocated for this mft
                                   record. This should be equal to the mft
                                   record size. */
/* 32*/ leMFT_REF base_mft_record;
                                /* This is zero for base mft records.
                                   When it is not zero it is a mft reference
                                   pointing to the base mft record to which
                                   this record belongs (this is then used to
                                   locate the attribute list attribute present
                                   in the base record which describes this
                                   extension record and hence might need
                                   modification when the extension record
                                   itself is modified, also locating the
                                   attribute list also means finding the other
                                   potential extents, belonging to the non-base
                                   mft record). */
/* 40*/ cint16 next_attr_instance; /* The instance number that will be
                                   assigned to the next attribute added to this
                                   mft record. NOTE: Incremented each time
                                   after it is used. NOTE: Every time the mft
                                   record is reused this number is set to zero.
                                   NOTE: The first instance number is always 0.
                                 */
/* The below fields are specific to NTFS 3.1+ (Windows XP and above): */
/* 42*/ cint16 reserved;          /* Reserved/alignment. */
/* 44*/ cint32 mft_record_number; /* Number of this mft record. */
/* sizeof() = 48 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} __attribute__((__packed__)) MFT_RECORD;

typedef struct
{
/*Ofs*/
/*  0   FS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
        FS_RECORD_TYPES magic;/* Usually the magic is "FILE". */
        cint16 usa_ofs;           /* See NTFS_RECORD definition above. */
        cint16 usa_count;         /* See NTFS_RECORD definition above. */

/*  8*/ cint64 lsn;              /* $LogFile sequence number for this record.
                                   Changed every time the record is modified. */
/* 16*/ cint16 sequence_number;   /* Number of times this mft record has been
                                   reused. (See description for MFT_REF
                                   above.) NOTE: The increment (skipping zero)
                                   is done when the file is deleted. NOTE: If
                                   this is zero it is left zero. */
/* 18*/ cint16 link_count;                /* Number of hard links, i.e. the number of
                                   directory entries referencing this record.
                                   NOTE: Only used in mft base records.
                                   NOTE: When deleting a directory entry we
                                   check the link_count and if it is 1 we
                                   delete the file. Otherwise we delete the
                                   FILE_NAME_ATTR being referenced by the
                                   directory entry from the mft record and
                                   decrement the link_count.
                                   FIXME: Careful with Win32 + DOS names! */
/* 20*/ cint16 attrs_offset;      /* Byte offset to the first attribute in this
                                   mft record from the start of the mft record.
                                   NOTE: Must be aligned to 8-byte boundary. */
/* 22*/ MFT_RECORD_FLAGS flags; /* Bit array of MFT_RECORD_FLAGS. When a file
                                   is deleted, the MFT_RECORD_IN_USE flag is
                                   set to zero. */
/* 24*/ cint32 bytes_in_use;      /* Number of bytes used in this mft record.
                                   NOTE: Must be aligned to 8-byte boundary. */
/* 28*/ cint32 bytes_allocated;   /* Number of bytes allocated for this mft
                                   record. This should be equal to the mft
                                   record size. */
/* 32*/ leMFT_REF base_mft_record;
                                /* This is zero for base mft records.
                                   When it is not zero it is a mft reference
                                   pointing to the base mft record to which
                                   this record belongs (this is then used to
                                   locate the attribute list attribute present
                                   in the base record which describes this
                                   extension record and hence might need
                                   modification when the extension record
                                   itself is modified, also locating the
                                   attribute list also means finding the other
                                   potential extents, belonging to the non-base
                                   mft record). */
/* 40*/ cint16 next_attr_instance; /* The instance number that will be
                                   assigned to the next attribute added to this
                                   mft record. NOTE: Incremented each time
                                   after it is used. NOTE: Every time the mft
                                   record is reused this number is set to zero.
                                   NOTE: The first instance number is always 0.
                                 */
/* sizeof() = 42 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} __attribute__((__packed__)) MFT_RECORD_OLD;

typedef enum
{
    AT_UNUSED                       = (cint32)(0),
    AT_STANDARD_INFORMATION         = (cint32)(0x10),
    AT_ATTRIBUTE_LIST               = (cint32)(0x20),
    AT_FILE_NAME                    = (cint32)(0x30),
    AT_OBJECT_ID                    = (cint32)(0x40),
    AT_SECURITY_DESCRIPTOR          = (cint32)(0x50),
    AT_VOLUME_NAME                  = (cint32)(0x60),
    AT_VOLUME_INFORMATION           = (cint32)(0x70),
    AT_DATA                         = (cint32)(0x80),
    AT_INDEX_ROOT                   = (cint32)(0x90),
    AT_INDEX_ALLOCATION             = (cint32)(0xa0),
    AT_BITMAP                       = (cint32)(0xb0),
    AT_REPARSE_POINT                = (cint32)(0xc0),
    AT_EA_INFORMATION               = (cint32)(0xd0),
    AT_EA                           = (cint32)(0xe0),
    AT_PROPERTY_SET                 = (cint32)(0xf0),
    AT_LOGGED_UTILITY_STREAM        = (cint32)(0x100),
    AT_FIRST_USER_DEFINED_ATTRIBUTE = (cint32)(0x1000),
    AT_END                          = (cint32)(0xffffffff),
} ATTR_TYPES;

typedef enum
{
    COLLATION_BINARY                = (cint32)(0),
    COLLATION_FILE_NAME             = (cint32)(1),
    COLLATION_UNICODE_STRING        = (cint32)(2),
    COLLATION_NTOFS_ULONG           = (cint32)(16),
    COLLATION_NTOFS_SID             = (cint32)(17),
    COLLATION_NTOFS_SECURITY_HASH   = (cint32)(18),
    COLLATION_NTOFS_ULONGS          = (cint32)(19),
} COLLATION_RULES;

typedef enum
{
    ATTR_DEF_INDEXABLE      = (cint32)(0x02), /* Attribute can be indexed. */
    ATTR_DEF_MULTIPLE       = (cint32)(0x04), /* Attribute type can be present multiple times in the mft records of an inode. */
    ATTR_DEF_NOT_ZERO       = (cint32)(0x08), /* Attribute value must contain at least one non-zero byte. */
    ATTR_DEF_INDEXED_UNIQUE = (cint32)(0x10), /* Attribute must be indexed and the attribute value must be unique for the attribute type in all of the mft records of an inode. */
    ATTR_DEF_NAMED_UNIQUE   = (cint32)(0x20), /* Attribute must be named and the name must be unique for the attribute type in all of the mft records of an inode. */
    ATTR_DEF_RESIDENT       = (cint32)(0x40), /* Attribute must be resident. */
    ATTR_DEF_ALWAYS_LOG     = (cint32)(0x80), /* Always log modifications to this attribute, regardless of whether it is resident or non-resident.  Without this, only log modifications if the attribute is resident. */
} ATTR_DEF_FLAGS;

typedef struct
{
    /*hex ofs*/
    /*  0*/ FSChar name[0x40];            /* Unicode name of the attribute. Zero terminated. */
    /* 80*/ ATTR_TYPES type;                /* Type of the attribute. */
    /* 84*/ cint32 display_rule;              /* Default display rule. FIXME: What does it mean? (AIA) */
    /* 88*/ COLLATION_RULES collation_rule; /* Default collation rule. */
    /* 8c*/ ATTR_DEF_FLAGS flags;           /* Flags describing the attribute. */
    /* 90*/ cint64 min_size;                 /* Optional minimum attribute size. */
    /* 98*/ cint64 max_size;                 /* Maximum size of attribute. */
    /* sizeof() = 0xa0 or 160 bytes */
} __attribute__((__packed__)) ATTR_DEF;

typedef enum
{
    ATTR_IS_COMPRESSED      = (cint16)(0x0001),
    ATTR_COMPRESSION_MASK   = (cint16)(0x00ff),  /* Compression method mask. Also, first illegal value. */
    ATTR_IS_ENCRYPTED       = (cint16)(0x4000),
    ATTR_IS_SPARSE          = (cint16)(0x8000),
} __attribute__((__packed__)) ATTR_FLAGS;

typedef enum
{
    RESIDENT_ATTR_IS_INDEXED = 0x01, /* Attribute is referenced in an index (has implications for deleting and modifying the attribute). */
} __attribute__((__packed__)) RESIDENT_ATTR_FLAGS;

typedef struct
{
/*Ofs*/
/*  0*/ ATTR_TYPES type;        /* The (32-bit) type of the attribute. */
/*  4*/ cint32 length;            /* Byte size of the resident part of the
                                   attribute (aligned to 8-byte boundary).
                                   Used to get to the next attribute. */
/*  8*/ cuint8 non_resident;        /* If 0, attribute is resident.
                                   If 1, attribute is non-resident. */
/*  9*/ cuint8 name_length;         /* Unicode character size of name of attribute.
                                   0 if unnamed. */
/* 10*/ cint16 name_offset;       /* If name_length != 0, the byte offset to the
                                   beginning of the name from the attribute
                                   record. Note that the name is stored as a
                                   Unicode string. When creating, place offset
                                   just at the end of the record header. Then,
                                   follow with attribute value or mapping pairs
                                   array, resident and non-resident attributes
                                   respectively, aligning to an 8-byte
                                   boundary. */
/* 12*/ ATTR_FLAGS flags;       /* Flags describing the attribute. */
/* 14*/ cint16 instance;          /* The instance of this attribute record. This
                                   number is unique within this mft record (see
                                   MFT_RECORD/next_attribute_instance notes
                                   above for more details). */
/* 16*/ union {
                /* Resident attributes. */
                struct {
/* 16 */                cint32 value_length; /* Byte size of attribute value. */
/* 20 */                cint16 value_offset; /* Byte offset of the attribute
                                               value from the start of the
                                               attribute record. When creating,
                                               align to 8-byte boundary if we
                                               have a name present as this might
                                               not have a length of a multiple
                                               of 8-bytes. */
/* 22 */                RESIDENT_ATTR_FLAGS resident_flags; /* See above. */
/* 23 */                cint8 reservedR;       /* Reserved/alignment to 8-byte
                                               boundary. */
/* 24 */                void *resident_end[0]; /* Use offsetof(ATTR_RECORD,
                                                  resident_end) to get size of
                                                  a resident attribute. */
                } __attribute__((__packed__));
                /* Non-resident attributes. */
                struct {
/* 16*/                 leVCN lowest_vcn;       /* Lowest valid virtual cluster number
                                for this portion of the attribute value or
                                0 if this is the only extent (usually the
                                case). - Only when an attribute list is used
                                does lowest_vcn != 0 ever occur. */
/* 24*/                 leVCN highest_vcn; /* Highest valid vcn of this extent of
                                the attribute value. - Usually there is only one
                                portion, so this usually equals the attribute
                                value size in clusters minus 1. Can be -1 for
                                zero length files. Can be 0 for "single extent"
                                attributes. */
/* 32*/                 cint16 mapping_pairs_offset; /* Byte offset from the
                                beginning of the structure to the mapping pairs
                                array which contains the mappings between the
                                vcns and the logical cluster numbers (lcns).
                                When creating, place this at the end of this
                                record header aligned to 8-byte boundary. */
/* 34*/                 cuint8 compression_unit; /* The compression unit expressed
                                as the log to the base 2 of the number of
                                clusters in a compression unit. 0 means not
                                compressed. (This effectively limits the
                                compression unit size to be a power of two
                                clusters.) WinNT4 only uses a value of 4. */
/* 35*/                 cuint8 reserved1[5];        /* Align to 8-byte boundary. */
/* The sizes below are only used when lowest_vcn is zero, as otherwise it would
   be difficult to keep them up-to-date.*/
/* 40*/                 cint64 allocated_size;   /* Byte size of disk space
                                allocated to hold the attribute value. Always
                                is a multiple of the cluster size. When a file
                                is compressed, this field is a multiple of the
                                compression block size (2^compression_unit) and
                                it represents the logically allocated space
                                rather than the actual on disk usage. For this
                                use the compressed_size (see below). */
/* 48*/                 cint64 data_size;        /* Byte size of the attribute
                                value. Can be larger than allocated_size if
                                attribute value is compressed or sparse. */
/* 56*/                 cint64 initialized_size; /* Byte size of initialized
                                portion of the attribute value. Usually equals
                                data_size. */
/* 64 */                void *non_resident_end[0]; /* Use offsetof(ATTR_RECORD,
                                                      non_resident_end) to get
                                                      size of a non resident
                                                      attribute. */
/* sizeof(uncompressed attr) = 64*/
/* 64*/                 cint64 compressed_size;  /* Byte size of the attribute
                                value after compression. Only present when
                                compressed. Always is a multiple of the
                                cluster size. Represents the actual amount of
                                disk space being used on the disk. */
/* 72 */                void *compressed_end[0];
                                /* Use offsetof(ATTR_RECORD, compressed_end) to
                                   get size of a compressed attribute. */
/* sizeof(compressed attr) = 72*/
                } __attribute__((__packed__));
        } __attribute__((__packed__));
} __attribute__((__packed__)) ATTR_RECORD;

typedef ATTR_RECORD ATTR_REC;

typedef enum
{
    /*
     * These flags are only present in the STANDARD_INFORMATION attribute
     * (in the field file_attributes).
     */
    FILE_ATTR_READONLY              = (cint32)(0x00000001),
    FILE_ATTR_HIDDEN                = (cint32)(0x00000002),
    FILE_ATTR_SYSTEM                = (cint32)(0x00000004),
    /* Old DOS volid. Unused in NT. = const_cpu_to_le32(0x00000008), */

    FILE_ATTR_DIRECTORY             = (cint32)(0x00000010),
    /* FILE_ATTR_DIRECTORY is not considered valid in NT. It is reserved for the DOS SUBDIRECTORY flag. */
    FILE_ATTR_ARCHIVE               = (cint32)(0x00000020),
    FILE_ATTR_DEVICE                = (cint32)(0x00000040),
    FILE_ATTR_NORMAL                = (cint32)(0x00000080),

    FILE_ATTR_TEMPORARY             = (cint32)(0x00000100),
    FILE_ATTR_SPARSE_FILE           = (cint32)(0x00000200),
    FILE_ATTR_REPARSE_POINT         = (cint32)(0x00000400),
    FILE_ATTR_COMPRESSED            = (cint32)(0x00000800),

    FILE_ATTR_OFFLINE               = (cint32)(0x00001000),
    FILE_ATTR_NOT_CONTENT_INDEXED   = (cint32)(0x00002000),
    FILE_ATTR_ENCRYPTED             = (cint32)(0x00004000),
    /* Supposed to mean no data locally, possibly repurposed */
    FILE_ATTRIBUTE_RECALL_ON_OPEN   = (cint32)(0x00040000),

    FILE_ATTR_VALID_FLAGS           = (cint32)(0x00047fb7),
    /* FILE_ATTR_VALID_FLAGS masks out the old DOS VolId and the FILE_ATTR_DEVICE and preserves everything else. This mask is used to obtain all flags that are valid for reading. */
    FILE_ATTR_VALID_SET_FLAGS       = (cint32)(0x000031a7),
    /* FILE_ATTR_VALID_SET_FLAGS masks out the old DOS VolId, the FILE_ATTR_DEVICE, FILE_ATTR_DIRECTORY, FILE_ATTR_SPARSE_FILE, FILE_ATTR_REPARSE_POINT, FILE_ATRE_COMPRESSED and FILE_ATTR_ENCRYPTED and preserves the rest. This mask is used to to obtain all flags that are valid for setting. */

    /**
     * FILE_ATTR_I30_INDEX_PRESENT - Is it a directory?
     *
     * This is a copy of the MFT_RECORD_IS_DIRECTORY bit from the mft
     * record, telling us whether this is a directory or not, i.e. whether
     * it has an index root attribute named "$I30" or not.
     *
     * This flag is only present in the FILE_NAME attribute (in the
     * file_attributes field).
     */
    FILE_ATTR_I30_INDEX_PRESENT     = (cint32)(0x10000000),

    /**
     * FILE_ATTR_VIEW_INDEX_PRESENT - Does have a non-directory index?
     *
     * This is a copy of the MFT_RECORD_IS_VIEW_INDEX bit from the mft
     * record, telling us whether this file has a view index present (eg.
     * object id index, quota index, one of the security indexes and the
     * reparse points index).
     *
     * This flag is only present in the $STANDARD_INFORMATION and
     * $FILE_NAME attributes.
     */
    FILE_ATTR_VIEW_INDEX_PRESENT    = (cint32)(0x20000000),
} __attribute__((__packed__)) FILE_ATTR_FLAGS;

typedef struct
{
/*Ofs*/
/*  0*/ cint64 creation_time;            /* Time file was created. Updated when
                                           a filename is changed(?). */
/*  8*/ cint64 last_data_change_time;    /* Time the data attribute was last
                                           modified. */
/* 16*/ cint64 last_mft_change_time;     /* Time this mft record was last
                                           modified. */
/* 24*/ cint64 last_access_time;         /* Approximate time when the file was
                                           last accessed (obviously this is not
                                           updated on read-only volumes). In
                                           Windows this is only updated when
                                           accessed if some time delta has
                                           passed since the last update. Also,
                                           last access times updates can be
                                           disabled altogether for speed. */
/* 32*/ FILE_ATTR_FLAGS file_attributes; /* Flags describing the file. */
/* 36*/ union {
                /* NTFS 1.2 (and previous, presumably) */
                struct {
                /* 36 */ cuint8 reserved12[12];     /* Reserved/alignment to 8-byte
                                                   boundary. */
                /* 48 */ void *v1_end[0];       /* Marker for offsetof(). */
                } __attribute__((__packed__));
/* sizeof() = 48 bytes */
                /* NTFS 3.0 */
                struct {
/*
 * If a volume has been upgraded from a previous NTFS version, then these
 * fields are present only if the file has been accessed since the upgrade.
 * Recognize the difference by comparing the length of the resident attribute
 * value. If it is 48, then the following fields are missing. If it is 72 then
 * the fields are present. Maybe just check like this:
 *      if (resident.ValueLength < sizeof(STANDARD_INFORMATION)) {
 *              Assume NTFS 1.2- format.
 *              If (volume version is 3.0+)
 *                      Upgrade attribute to NTFS 3.0 format.
 *              else
 *                      Use NTFS 1.2- format for access.
 *      } else
 *              Use NTFS 3.0 format for access.
 * Only problem is that it might be legal to set the length of the value to
 * arbitrarily large values thus spoiling this check. - But chkdsk probably
 * views that as a corruption, assuming that it behaves like this for all
 * attributes.
 */
                /* 36*/ cint32 maximum_versions;  /* Maximum allowed versions for
                                file. Zero if version numbering is disabled. */
                /* 40*/ cint32 version_number;    /* This file's version (if any).
                                Set to zero if maximum_versions is zero. */
                /* 44*/ cint32 class_id;          /* Class id from bidirectional
                                class id index (?). */
                /* 48*/ cint32 owner_id;          /* Owner_id of the user owning
                                the file. Translate via $Q index in FILE_Extend
                                /$Quota to the quota control entry for the user
                                owning the file. Zero if quotas are disabled. */
                /* 52*/ cint32 security_id;       /* Security_id for the file.
                                Translate via $SII index and $SDS data stream
                                in FILE_Secure to the security descriptor. */
                /* 56*/ cint64 quota_charged;     /* Byte size of the charge to
                                the quota for all streams of the file. Note: Is
                                zero if quotas are disabled. */
                /* 64*/ cint64 usn;               /* Last update sequence number
                                of the file. This is a direct index into the
                                change (aka usn) journal file. It is zero if
                                the usn journal is disabled.
                                NOTE: To disable the journal need to delete
                                the journal file itself and to then walk the
                                whole mft and set all Usn entries in all mft
                                records to zero! (This can take a while!)
                                The journal is FILE_Extend/$UsnJrnl. Win2k
                                will recreate the journal and initiate
                                logging if necessary when mounting the
                                partition. This, in contrast to disabling the
                                journal is a very fast process, so the user
                                won't even notice it. */
                /* 72*/ void *v3_end[0]; /* Marker for offsetof(). */
                } __attribute__((__packed__));
        } __attribute__((__packed__));
/* sizeof() = 72 bytes (NTFS 3.0) */
} __attribute__((__packed__)) STANDARD_INFORMATION;

typedef struct
{
/*Ofs*/
/*  0*/ ATTR_TYPES type;        /* Type of referenced attribute. */
/*  4*/ cint16 length;            /* Byte size of this entry. */
/*  6*/ cuint8 name_length;         /* Size in Unicode chars of the name of the
                                   attribute or 0 if unnamed. */
/*  7*/ cuint8 name_offset;         /* Byte offset to beginning of attribute name
                                   (always set this to where the name would
                                   start even if unnamed). */
/*  8*/ leVCN lowest_vcn;       /* Lowest virtual cluster number of this portion
                                   of the attribute value. This is usually 0. It
                                   is non-zero for the case where one attribute
                                   does not fit into one mft record and thus
                                   several mft records are allocated to hold
                                   this attribute. In the latter case, each mft
                                   record holds one extent of the attribute and
                                   there is one attribute list entry for each
                                   extent. NOTE: This is DEFINITELY a signed
                                   value! The windows driver uses cmp, followed
                                   by jg when comparing this, thus it treats it
                                   as signed. */
/* 16*/ leMFT_REF mft_reference;/* The reference of the mft record holding
                                   the ATTR_RECORD for this portion of the
                                   attribute value. */
/* 24*/ cint16 instance;          /* If lowest_vcn = 0, the instance of the
                                   attribute being referenced; otherwise 0. */
/* 26*/ FSChar name[0];       /* Use when creating only. When reading use
                                   name_offset to determine the location of the
                                   name. */
/* sizeof() = 26 + (attribute_name_length * 2) bytes */
} __attribute__((__packed__)) ATTR_LIST_ENTRY;

typedef enum
{
    FILE_NAME_POSIX                 = 0x00,
            /* This is the largest namespace. It is case sensitive and
               allows all Unicode characters except for: '\0' and '/'.
               Beware that in WinNT/2k files which eg have the same name
               except for their case will not be distinguished by the
               standard utilities and thus a "del filename" will delete
               both "filename" and "fileName" without warning. */
    FILE_NAME_WIN32                 = 0x01,
            /* The standard WinNT/2k NTFS long filenames. Case insensitive.
               All Unicode chars except: '\0', '"', '*', '/', ':', '<',
               '>', '?', '\' and '|'.  Trailing dots and spaces are allowed,
               even though on Windows a filename with such a suffix can only
               be created and accessed using a WinNT-style path, i.e.
               \\?\-prefixed.  (If a regular path is used, Windows will
               strip the trailing dots and spaces, which makes such
               filenames incompatible with most Windows software.) */
    FILE_NAME_DOS                   = 0x02,
            /* The standard DOS filenames (8.3 format). Uppercase only.
               All 8-bit characters greater space, except: '"', '*', '+',
               ',', '/', ':', ';', '<', '=', '>', '?' and '\'.  Trailing
               dots and spaces are forbidden. */
    FILE_NAME_WIN32_AND_DOS         = 0x03,
            /* 3 means that both the Win32 and the DOS filenames are
               identical and hence have been saved in this single filename
               record. */
} __attribute__((__packed__)) FILE_NAME_TYPE_FLAGS;

typedef struct
{
/*hex ofs*/
/*  0*/ leMFT_REF parent_directory;     /* Directory this filename is
                                           referenced from. */
/*  8*/ cint64 creation_time;            /* Time file was created. */
/* 10*/ cint64 last_data_change_time;    /* Time the data attribute was last
                                           modified. */
/* 18*/ cint64 last_mft_change_time;     /* Time this mft record was last
                                           modified. */
/* 20*/ cint64 last_access_time;         /* Last time this mft record was
                                           accessed. */
/* 28*/ cint64 allocated_size;           /* Byte size of on-disk allocated space
                                           for the data attribute.  So for
                                           normal $DATA, this is the
                                           allocated_size from the unnamed
                                           $DATA attribute and for compressed
                                           and/or sparse $DATA, this is the
                                           compressed_size from the unnamed
                                           $DATA attribute.  NOTE: This is a
                                           multiple of the cluster size. */
/* 30*/ cint64 data_size;                        /* Byte size of actual data in data
                                           attribute. */
/* 38*/ FILE_ATTR_FLAGS file_attributes;        /* Flags describing the file. */
/* 3c*/ union {
        /* 3c*/ struct {
                /* 3c*/ cint16 packed_ea_size;    /* Size of the buffer needed to
                                                   pack the extended attributes
                                                   (EAs), if such are present.*/
                /* 3e*/ cint16 reserved;          /* Reserved for alignment. */
                } __attribute__((__packed__));
        /* 3c*/ cint32 reparse_point_tag;         /* Type of reparse point,
                                                   present only in reparse
                                                   points and only if there are
                                                   no EAs. */
        } __attribute__((__packed__));
/* 40*/ cuint8 file_name_length;                    /* Length of file name in
                                                   (Unicode) characters. */
/* 41*/ FILE_NAME_TYPE_FLAGS file_name_type;    /* Namespace of the file name.*/
/* 42*/ FSChar file_name[0];                  /* File name in Unicode. */
} __attribute__((__packed__)) FILE_NAME_ATTR;

typedef struct
{
    cint32 data1;     /* The first eight hexadecimal digits of the GUID. */
    cint16 data2;     /* The first group of four hexadecimal digits. */
    cint16 data3;     /* The second group of four hexadecimal digits. */
    cuint8 data4[8];    /* The first two bytes are the third group of four
                       hexadecimal digits. The remaining six bytes are the
                       final 12 hexadecimal digits. */
} __attribute__((__packed__)) GUID;

typedef struct
{
    leMFT_REF mft_reference;        /* Mft record containing the object_id
                                       in the index entry key. */
    union {
        struct {
            GUID birth_volume_id;
            GUID birth_object_id;
            GUID domain_id;
        } __attribute__((__packed__));
        cuint8 extended_info[48];
    } __attribute__((__packed__));
} __attribute__((__packed__)) OBJ_ID_INDEX_DATA;

typedef struct
{
    GUID object_id;                         /* Unique id assigned to the
                                               file.*/
    /* The following fields are optional. The attribute value size is 16
       bytes, i.e. sizeof(GUID), if these are not present at all. Note,
       the entries can be present but one or more (or all) can be zero
       meaning that that particular value(s) is(are) not defined. Note,
       when the fields are missing here, it is well possible that they are
       to be found within the $Extend/$ObjId system file indexed under the
       above object_id. */
    union {
        struct {
            GUID birth_volume_id;   /* Unique id of volume on which
                                       the file was first created.*/
            GUID birth_object_id;   /* Unique id of file when it was
                                       first created. */
            GUID domain_id;         /* Reserved, zero. */
        } __attribute__((__packed__));
        cuint8 extended_info[48];
    } __attribute__((__packed__));
} __attribute__((__packed__)) OBJECT_ID_ATTR;

typedef enum
{
    /* Identifier authority. */
    SECURITY_NULL_RID                 = 0,  /* S-1-0 */
    SECURITY_WORLD_RID                = 0,  /* S-1-1 */
    SECURITY_LOCAL_RID                = 0,  /* S-1-2 */

    SECURITY_CREATOR_OWNER_RID        = 0,  /* S-1-3 */
    SECURITY_CREATOR_GROUP_RID        = 1,  /* S-1-3 */

    SECURITY_CREATOR_OWNER_SERVER_RID = 2,  /* S-1-3 */
    SECURITY_CREATOR_GROUP_SERVER_RID = 3,  /* S-1-3 */

    SECURITY_DIALUP_RID               = 1,
    SECURITY_NETWORK_RID              = 2,
    SECURITY_BATCH_RID                = 3,
    SECURITY_INTERACTIVE_RID          = 4,
    SECURITY_SERVICE_RID              = 6,
    SECURITY_ANONYMOUS_LOGON_RID      = 7,
    SECURITY_PROXY_RID                = 8,
    SECURITY_ENTERPRISE_CONTROLLERS_RID=9,
    SECURITY_SERVER_LOGON_RID         = 9,
    SECURITY_PRINCIPAL_SELF_RID       = 0xa,
    SECURITY_AUTHENTICATED_USER_RID   = 0xb,
    SECURITY_RESTRICTED_CODE_RID      = 0xc,
    SECURITY_TERMINAL_SERVER_RID      = 0xd,

    SECURITY_LOGON_IDS_RID            = 5,
    SECURITY_LOGON_IDS_RID_COUNT      = 3,

    SECURITY_LOCAL_SYSTEM_RID         = 0x12,

    SECURITY_NT_NON_UNIQUE            = 0x15,

    SECURITY_BUILTIN_DOMAIN_RID       = 0x20,

    /*
     * Well-known domain relative sub-authority values (RIDs).
     */

    /* Users. */
    DOMAIN_USER_RID_ADMIN             = 0x1f4,
    DOMAIN_USER_RID_GUEST             = 0x1f5,
    DOMAIN_USER_RID_KRBTGT            = 0x1f6,

    /* Groups. */
    DOMAIN_GROUP_RID_ADMINS           = 0x200,
    DOMAIN_GROUP_RID_USERS            = 0x201,
    DOMAIN_GROUP_RID_GUESTS           = 0x202,
    DOMAIN_GROUP_RID_COMPUTERS        = 0x203,
    DOMAIN_GROUP_RID_CONTROLLERS      = 0x204,
    DOMAIN_GROUP_RID_CERT_ADMINS      = 0x205,
    DOMAIN_GROUP_RID_SCHEMA_ADMINS    = 0x206,
    DOMAIN_GROUP_RID_ENTERPRISE_ADMINS= 0x207,
    DOMAIN_GROUP_RID_POLICY_ADMINS    = 0x208,

    /* Aliases. */
    DOMAIN_ALIAS_RID_ADMINS           = 0x220,
    DOMAIN_ALIAS_RID_USERS            = 0x221,
    DOMAIN_ALIAS_RID_GUESTS           = 0x222,
    DOMAIN_ALIAS_RID_POWER_USERS      = 0x223,

    DOMAIN_ALIAS_RID_ACCOUNT_OPS      = 0x224,
    DOMAIN_ALIAS_RID_SYSTEM_OPS       = 0x225,
    DOMAIN_ALIAS_RID_PRINT_OPS        = 0x226,
    DOMAIN_ALIAS_RID_BACKUP_OPS       = 0x227,

    DOMAIN_ALIAS_RID_REPLICATOR       = 0x228,
    DOMAIN_ALIAS_RID_RAS_SERVERS      = 0x229,
    DOMAIN_ALIAS_RID_PREW2KCOMPACCESS = 0x22a,
} RELATIVE_IDENTIFIERS;
typedef union {
    struct {
        cint16 high_part;         /* High 16-bits. */
        cint32 low_part;          /* Low 32-bits. */
    } __attribute__((__packed__));
    cuint8 value[6];                    /* Value as individual bytes. */
} __attribute__((__packed__)) SID_IDENTIFIER_AUTHORITY;

typedef struct
{
    cuint8 revision;
    cuint8 sub_authority_count;
    SID_IDENTIFIER_AUTHORITY identifier_authority;
    cint32 sub_authority[1];          /* At least one sub_authority. */
} __attribute__((__packed__)) SID;

typedef enum
{
    SID_REVISION                    =  1,   /* Current revision level. */
    SID_MAX_SUB_AUTHORITIES         = 15,   /* Maximum number of those. */
    SID_RECOMMENDED_SUB_AUTHORITIES =  1,   /* Will change to around 6 in a future revision. */
} SID_CONSTANTS;

typedef enum
{
    ACCESS_MIN_MS_ACE_TYPE          = 0,
    ACCESS_ALLOWED_ACE_TYPE         = 0,
    ACCESS_DENIED_ACE_TYPE          = 1,
    SYSTEM_AUDIT_ACE_TYPE           = 2,
    SYSTEM_ALARM_ACE_TYPE           = 3, /* Not implemented as of Win2k. */
    ACCESS_MAX_MS_V2_ACE_TYPE       = 3,

    ACCESS_ALLOWED_COMPOUND_ACE_TYPE= 4,
    ACCESS_MAX_MS_V3_ACE_TYPE       = 4,

    /* The following are Win2k only. */
    ACCESS_MIN_MS_OBJECT_ACE_TYPE   = 5,
    ACCESS_ALLOWED_OBJECT_ACE_TYPE  = 5,
    ACCESS_DENIED_OBJECT_ACE_TYPE   = 6,
    SYSTEM_AUDIT_OBJECT_ACE_TYPE    = 7,
    SYSTEM_ALARM_OBJECT_ACE_TYPE    = 8,
    ACCESS_MAX_MS_OBJECT_ACE_TYPE   = 8,

    ACCESS_MAX_MS_V4_ACE_TYPE       = 8,

    /* This one is for WinNT&2k. */
    ACCESS_MAX_MS_ACE_TYPE          = 8,

    /* Windows XP and later */
    ACCESS_ALLOWED_CALLBACK_ACE_TYPE        = 9,
    ACCESS_DENIED_CALLBACK_ACE_TYPE         = 10,
    ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE = 11,
    ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE  = 12,
    SYSTEM_AUDIT_CALLBACK_ACE_TYPE          = 13,
    SYSTEM_ALARM_CALLBACK_ACE_TYPE          = 14, /* reserved */
    SYSTEM_AUDIT_CALLBACK_OBJECT_ACE_TYPE   = 15,
    SYSTEM_ALARM_CALLBACK_OBJECT_ACE_TYPE   = 16, /* reserved */

    /* Windows Vista and later */
    SYSTEM_MANDATORY_LABEL_ACE_TYPE         = 17,

    /* Windows 8 and later */
    SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE      = 18,
    SYSTEM_SCOPED_POLICY_ID_ACE_TYPE        = 19,

    /* Windows 10 and later */
    SYSTEM_PROCESS_TRUST_LABEL_ACE_TYPE     = 20,
} __attribute__((__packed__)) ACE_TYPES;

typedef enum
{
    /* The inheritance flags. */
    OBJECT_INHERIT_ACE              = 0x01,
    CONTAINER_INHERIT_ACE           = 0x02,
    NO_PROPAGATE_INHERIT_ACE        = 0x04,
    INHERIT_ONLY_ACE                = 0x08,
    INHERITED_ACE                   = 0x10, /* Win2k only. */
    VALID_INHERIT_FLAGS             = 0x1f,

    /* The audit flags. */
    SUCCESSFUL_ACCESS_ACE_FLAG      = 0x40,
    FAILED_ACCESS_ACE_FLAG          = 0x80,
} __attribute__((__packed__)) ACE_FLAGS;

typedef struct
{
    ACE_TYPES type;         /* Type of the ACE. */
    ACE_FLAGS flags;        /* Flags describing the ACE. */
    cint16 size;              /* Size in bytes of the ACE. */
} __attribute__((__packed__)) ACE_HEADER;

typedef enum
{
    /*
     * The specific rights (bits 0 to 15). Depend on the type of the
     * object being secured by the ACE.
     */

    /* Specific rights for files and directories are as follows: */

    /* Right to read data from the file. (FILE) */
    FILE_READ_DATA                  = const_cpu_to_le32(0x00000001),
    /* Right to list contents of a directory. (DIRECTORY) */
    FILE_LIST_DIRECTORY             = const_cpu_to_le32(0x00000001),

    /* Right to write data to the file. (FILE) */
    FILE_WRITE_DATA                 = const_cpu_to_le32(0x00000002),
    /* Right to create a file in the directory. (DIRECTORY) */
    FILE_ADD_FILE                   = const_cpu_to_le32(0x00000002),

    /* Right to append data to the file. (FILE) */
    FILE_APPEND_DATA                = const_cpu_to_le32(0x00000004),
    /* Right to create a subdirectory. (DIRECTORY) */
    FILE_ADD_SUBDIRECTORY           = const_cpu_to_le32(0x00000004),

    /* Right to read extended attributes. (FILE/DIRECTORY) */
    FILE_READ_EA                    = const_cpu_to_le32(0x00000008),

    /* Right to write extended attributes. (FILE/DIRECTORY) */
    FILE_WRITE_EA                   = const_cpu_to_le32(0x00000010),

    /* Right to execute a file. (FILE) */
    FILE_EXECUTE                    = const_cpu_to_le32(0x00000020),
    /* Right to traverse the directory. (DIRECTORY) */
    FILE_TRAVERSE                   = const_cpu_to_le32(0x00000020),

    /*
     * Right to delete a directory and all the files it contains (its
     * children), even if the files are read-only. (DIRECTORY)
     */
    FILE_DELETE_CHILD               = const_cpu_to_le32(0x00000040),

    /* Right to read file attributes. (FILE/DIRECTORY) */
    FILE_READ_ATTRIBUTES            = const_cpu_to_le32(0x00000080),

    /* Right to change file attributes. (FILE/DIRECTORY) */
    FILE_WRITE_ATTRIBUTES           = const_cpu_to_le32(0x00000100),

    /*
     * The standard rights (bits 16 to 23). Are independent of the type of
     * object being secured.
     */

    /* Right to delete the object. */
    DELETE                          = const_cpu_to_le32(0x00010000),

    /*
     * Right to read the information in the object's security descriptor,
     * not including the information in the SACL. I.e. right to read the
     * security descriptor and owner.
     */
    READ_CONTROL                    = const_cpu_to_le32(0x00020000),

    /* Right to modify the DACL in the object's security descriptor. */
    WRITE_DAC                       = const_cpu_to_le32(0x00040000),

    /* Right to change the owner in the object's security descriptor. */
    WRITE_OWNER                     = const_cpu_to_le32(0x00080000),

    /*
     * Right to use the object for synchronization. Enables a process to
     * wait until the object is in the signalled state. Some object types
     * do not support this access right.
     */
    SYNCHRONIZE                     = const_cpu_to_le32(0x00100000),

    /*
     * The following STANDARD_RIGHTS_* are combinations of the above for
     * convenience and are defined by the Win32 API.
     */

    /* These are currently defined to READ_CONTROL. */
    STANDARD_RIGHTS_READ            = const_cpu_to_le32(0x00020000),
    STANDARD_RIGHTS_WRITE           = const_cpu_to_le32(0x00020000),
    STANDARD_RIGHTS_EXECUTE         = const_cpu_to_le32(0x00020000),

    /* Combines DELETE, READ_CONTROL, WRITE_DAC, and WRITE_OWNER access. */
    STANDARD_RIGHTS_REQUIRED        = const_cpu_to_le32(0x000f0000),

    /*
     * Combines DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, and
     * SYNCHRONIZE access.
     */
    STANDARD_RIGHTS_ALL             = const_cpu_to_le32(0x001f0000),

    /*
     * The access system ACL and maximum allowed access types (bits 24 to
     * 25, bits 26 to 27 are reserved).
     */
    ACCESS_SYSTEM_SECURITY          = const_cpu_to_le32(0x01000000),
    MAXIMUM_ALLOWED                 = const_cpu_to_le32(0x02000000),

    /*
     * The generic rights (bits 28 to 31). These map onto the standard and
     * specific rights.
     */

    /* Read, write, and execute access. */
    GENERIC_ALL                     = const_cpu_to_le32(0x10000000),

    /* Execute access. */
    GENERIC_EXECUTE                 = const_cpu_to_le32(0x20000000),

    /*
     * Write access. For files, this maps onto:
     *      FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA |
     *      FILE_WRITE_EA | STANDARD_RIGHTS_WRITE | SYNCHRONIZE
     * For directories, the mapping has the same numerical value. See
     * above for the descriptions of the rights granted.
     */
    GENERIC_WRITE                   = const_cpu_to_le32(0x40000000),

    /*
     * Read access. For files, this maps onto:
     *      FILE_READ_ATTRIBUTES | FILE_READ_DATA | FILE_READ_EA |
     *      STANDARD_RIGHTS_READ | SYNCHRONIZE
     * For directories, the mapping has the same numerical value. See
     * above for the descriptions of the rights granted.
     */
    GENERIC_READ                    = const_cpu_to_le32(0x80000000),
} ACCESS_MASK;

typedef struct {
    ACCESS_MASK generic_read;
    ACCESS_MASK generic_write;
    ACCESS_MASK generic_execute;
    ACCESS_MASK generic_all;
} __attribute__((__packed__)) GENERIC_MAPPING;

typedef struct {
    /*  0   ACE_HEADER; -- Unfolded here as gcc doesn't like unnamed structs. */
    ACE_TYPES type;         /* Type of the ACE. */
    ACE_FLAGS flags;        /* Flags describing the ACE. */
    cint16 size;              /* Size in bytes of the ACE. */

    /*  4*/ ACCESS_MASK mask;       /* Access mask associated with the ACE. */
    /*  8*/ SID sid;                /* The SID associated with the ACE. */
} __attribute__((__packed__)) ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE, SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE;

typedef enum {
    ACE_OBJECT_TYPE_PRESENT                 = const_cpu_to_le32(1),
    ACE_INHERITED_OBJECT_TYPE_PRESENT       = const_cpu_to_le32(2),
} OBJECT_ACE_FLAGS;

typedef struct {
    /*  0   ACE_HEADER; -- Unfolded here as gcc doesn't like unnamed structs. */
    ACE_TYPES type;         /* Type of the ACE. */
    ACE_FLAGS flags;        /* Flags describing the ACE. */
    cint16 size;              /* Size in bytes of the ACE. */

    /*  4*/ ACCESS_MASK mask;       /* Access mask associated with the ACE. */
    /*  8*/ OBJECT_ACE_FLAGS object_flags;  /* Flags describing the object ACE. */
    /* 12*/ GUID object_type;
    /* 28*/ GUID inherited_object_type;
    /* 44*/ SID sid;                /* The SID associated with the ACE. */
} __attribute__((__packed__)) ACCESS_ALLOWED_OBJECT_ACE, ACCESS_DENIED_OBJECT_ACE, SYSTEM_AUDIT_OBJECT_ACE, SYSTEM_ALARM_OBJECT_ACE;

typedef struct
{
    u8 revision;    /* Revision of this ACL. */
    u8 alignment1;
    le16 size;      /* Allocated space in bytes for ACL. Includes this
                       header, the ACEs and the remaining free space. */
    le16 ace_count; /* Number of ACEs in the ACL. */
    le16 alignment2;
    /* sizeof() = 8 bytes */
} __attribute__((__packed__)) ACL;

typedef enum
{
    /* Current revision. */
    ACL_REVISION            = 2,
    ACL_REVISION_DS         = 4,

    /* History of revisions. */
    ACL_REVISION1           = 1,
    MIN_ACL_REVISION        = 2,
    ACL_REVISION2           = 2,
    ACL_REVISION3           = 3,
    ACL_REVISION4           = 4,
    MAX_ACL_REVISION        = 4,
} ACL_CONSTANTS;

typedef enum
{
    SE_OWNER_DEFAULTED              = const_cpu_to_le16(0x0001),
    SE_GROUP_DEFAULTED              = const_cpu_to_le16(0x0002),
    SE_DACL_PRESENT                 = const_cpu_to_le16(0x0004),
    SE_DACL_DEFAULTED               = const_cpu_to_le16(0x0008),
    SE_SACL_PRESENT                 = const_cpu_to_le16(0x0010),
    SE_SACL_DEFAULTED               = const_cpu_to_le16(0x0020),
    SE_DACL_AUTO_INHERIT_REQ        = const_cpu_to_le16(0x0100),
    SE_SACL_AUTO_INHERIT_REQ        = const_cpu_to_le16(0x0200),
    SE_DACL_AUTO_INHERITED          = const_cpu_to_le16(0x0400),
    SE_SACL_AUTO_INHERITED          = const_cpu_to_le16(0x0800),
    SE_DACL_PROTECTED               = const_cpu_to_le16(0x1000),
    SE_SACL_PROTECTED               = const_cpu_to_le16(0x2000),
    SE_RM_CONTROL_VALID             = const_cpu_to_le16(0x4000),
    SE_SELF_RELATIVE                = const_cpu_to_le16(0x8000),
} __attribute__((__packed__)) SECURITY_DESCRIPTOR_CONTROL;

typedef struct
{
    u8 revision;    /* Revision level of the security descriptor. */
    u8 alignment;
    SECURITY_DESCRIPTOR_CONTROL control; /* Flags qualifying the type of
                       the descriptor as well as the following fields. */
    le32 owner;     /* Byte offset to a SID representing an object's
                       owner. If this is NULL, no owner SID is present in
                       the descriptor. */
    le32 group;     /* Byte offset to a SID representing an object's
                       primary group. If this is NULL, no primary group
                       SID is present in the descriptor. */
    le32 sacl;      /* Byte offset to a system ACL. Only valid, if
                       SE_SACL_PRESENT is set in the control field. If
                       SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL
                       is specified. */
    le32 dacl;      /* Byte offset to a discretionary ACL. Only valid, if
                       SE_DACL_PRESENT is set in the control field. If
                       SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL
                       (unconditionally granting access) is specified. */
    /* sizeof() = 0x14 bytes */
} __attribute__((__packed__)) SECURITY_DESCRIPTOR_RELATIVE;

typedef struct
{
    u8 revision;    /* Revision level of the security descriptor. */
    u8 alignment;
    SECURITY_DESCRIPTOR_CONTROL control;    /* Flags qualifying the type of
                       the descriptor as well as the following fields. */
    SID *owner;     /* Points to a SID representing an object's owner. If
                       this is NULL, no owner SID is present in the
                       descriptor. */
    SID *group;     /* Points to a SID representing an object's primary
                       group. If this is NULL, no primary group SID is
                       present in the descriptor. */
    ACL *sacl;      /* Points to a system ACL. Only valid, if
                       SE_SACL_PRESENT is set in the control field. If
                       SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL
                       is specified. */
    ACL *dacl;      /* Points to a discretionary ACL. Only valid, if
                       SE_DACL_PRESENT is set in the control field. If
                       SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL
                       (unconditionally granting access) is specified. */
} __attribute__((__packed__)) SECURITY_DESCRIPTOR;

typedef enum
{
    /* Current revision. */
    SECURITY_DESCRIPTOR_REVISION    = 1,
    SECURITY_DESCRIPTOR_REVISION1   = 1,

    /* The sizes of both the absolute and relative security descriptors is
       the same as pointers, at least on ia32 architecture are 32-bit. */
    SECURITY_DESCRIPTOR_MIN_LENGTH  = sizeof(SECURITY_DESCRIPTOR),
} SECURITY_DESCRIPTOR_CONSTANTS;

typedef SECURITY_DESCRIPTOR_RELATIVE SECURITY_DESCRIPTOR_ATTR;

typedef struct
{
    le32 hash;         /* Hash of the security descriptor. */
    le32 security_id;   /* The security_id assigned to the descriptor. */
    le64 offset;       /* Byte offset of this entry in the $SDS stream. */
    le32 length;       /* Size in bytes of this entry in $SDS stream. */
} __attribute__((__packed__)) SECURITY_DESCRIPTOR_HEADER;

/**
 * struct SDH_INDEX_DATA -
 */
typedef struct {
    le32 hash;          /* Hash of the security descriptor. */
    le32 security_id;   /* The security_id assigned to the descriptor. */
    le64 offset;       /* Byte offset of this entry in the $SDS stream. */
    le32 length;       /* Size in bytes of this entry in $SDS stream. */
    le32 reserved_II;   /* Padding - always unicode "II" or zero. This field
                          isn't counted in INDEX_ENTRY's data_length. */
} __attribute__((__packed__)) SDH_INDEX_DATA;

typedef SECURITY_DESCRIPTOR_HEADER SII_INDEX_DATA;

typedef struct
{
    /*  0   SECURITY_DESCRIPTOR_HEADER; -- Unfolded here as gcc doesn't like
                                           unnamed structs. */
    le32 hash;         /* Hash of the security descriptor. */
    le32 security_id;   /* The security_id assigned to the descriptor. */
    le64 offset;       /* Byte offset of this entry in the $SDS stream. */
    le32 length;       /* Size in bytes of this entry in $SDS stream. */
    /* 20*/ SECURITY_DESCRIPTOR_RELATIVE sid; /* The self-relative security
                                                 descriptor. */
} __attribute__((__packed__)) SDS_ENTRY;

typedef struct
{
    le32 security_id;   /* The security_id assigned to the descriptor. */
} __attribute__((__packed__)) SII_INDEX_KEY;

typedef struct {
    le32 hash;         /* Hash of the security descriptor. */
    le32 security_id;   /* The security_id assigned to the descriptor. */
} __attribute__((__packed__)) SDH_INDEX_KEY;

typedef struct {
    ntfschar name[0];       /* The name of the volume in Unicode. */
} __attribute__((__packed__)) VOLUME_NAME;

typedef enum
{
    VOLUME_IS_DIRTY                 = const_cpu_to_le16(0x0001),
    VOLUME_RESIZE_LOG_FILE          = const_cpu_to_le16(0x0002),
    VOLUME_UPGRADE_ON_MOUNT         = const_cpu_to_le16(0x0004),
    VOLUME_MOUNTED_ON_NT4           = const_cpu_to_le16(0x0008),
    VOLUME_DELETE_USN_UNDERWAY      = const_cpu_to_le16(0x0010),
    VOLUME_REPAIR_OBJECT_ID         = const_cpu_to_le16(0x0020),
    VOLUME_CHKDSK_UNDERWAY          = const_cpu_to_le16(0x4000),
    VOLUME_MODIFIED_BY_CHKDSK       = const_cpu_to_le16(0x8000),
    VOLUME_FLAGS_MASK               = const_cpu_to_le16(0xc03f),
} __attribute__((__packed__)) VOLUME_FLAGS;

typedef struct {
    le64 reserved;          /* Not used (yet?). */
    u8 majorVer;           /* Major version of the ntfs format. */
    u8 minorVer;           /* Minor version of the ntfs format. */
    VOLUME_FLAGS flags;     /* Bit array of VOLUME_* flags. */
} __attribute__((__packed__)) VOLUME_INFORMATION;
typedef struct {
    u8 data[0];             /* The file's data contents. */
} __attribute__((__packed__)) DATA_ATTR;

/**
 * enum INDEX_HEADER_FLAGS - Index header flags (8-bit).
 */
typedef enum {
    /* When index header is in an index root attribute: */
    SMALL_INDEX     = 0, /* The index is small enough to fit inside the
                            index root attribute and there is no index
                            allocation attribute present. */
    LARGE_INDEX     = 1, /* The index is too large to fit in the index
                            root attribute and/or an index allocation
                            attribute is present. */
    /*
     * When index header is in an index block, i.e. is part of index
     * allocation attribute:
     */
    LEAF_NODE       = 0, /* This is a leaf node, i.e. there are no more
                            nodes branching off it. */
    INDEX_NODE      = 1, /* This node indexes other nodes, i.e. is not a
                            leaf node. */
    NODE_MASK       = 1, /* Mask for accessing the *_NODE bits. */
} __attribute__((__packed__)) INDEX_HEADER_FLAGS;

typedef struct {
    /*  0*/ le32 entries_offset;    /* Byte offset from the INDEX_HEADER to first
                                       INDEX_ENTRY, aligned to 8-byte boundary.  */
    /*  4*/ le32 index_length;      /* Data size in byte of the INDEX_ENTRY's,
                                       including the INDEX_HEADER, aligned to 8. */
    /*  8*/ le32 allocated_size;    /* Allocated byte size of this index (block),
                                       multiple of 8 bytes. See more below.      */
    /*
       For the index root attribute, the above two numbers are always
       equal, as the attribute is resident and it is resized as needed.

       For the index allocation attribute, the attribute is not resident
       and the allocated_size is equal to the index_block_size specified
       by the corresponding INDEX_ROOT attribute minus the INDEX_BLOCK
       size not counting the INDEX_HEADER part (i.e. minus -24).
     */
    /* 12*/ INDEX_HEADER_FLAGS ih_flags;    /* Bit field of INDEX_HEADER_FLAGS.  */
    /* 13*/ u8 reserved[3];                 /* Reserved/align to 8-byte boundary.*/
    /* sizeof() == 16 */
} __attribute__((__packed__)) INDEX_HEADER;
typedef struct {
    /*  0*/ ATTR_TYPES type;                /* Type of the indexed attribute. Is
                                               $FILE_NAME for directories, zero
                                               for view indexes. No other values
                                               allowed. */
    /*  4*/ COLLATION_RULES collation_rule; /* Collation rule used to sort the
                                               index entries. If type is $FILE_NAME,
                                               this must be COLLATION_FILE_NAME. */
    /*  8*/ le32 index_block_size;          /* Size of index block in bytes (in
                                               the index allocation attribute). */
    /* 12*/ s8 clusters_per_index_block;    /* Size of index block in clusters (in
                                               the index allocation attribute), when
                                               an index block is >= than a cluster,
                                               otherwise sectors per index block. */
    /* 13*/ u8 reserved[3];                 /* Reserved/align to 8-byte boundary. */
    /* 16*/ INDEX_HEADER index;             /* Index header describing the
                                               following index entries. */
    /* sizeof()= 32 bytes */
} __attribute__((__packed__)) INDEX_ROOT;

typedef struct {
    /*  0   NTFS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
    NTFS_RECORD_TYPES magic;/* Magic is "INDX". */
    le16 usa_ofs;           /* See NTFS_RECORD definition. */
    le16 usa_count;         /* See NTFS_RECORD definition. */

    /*  8*/ leLSN lsn;              /* $LogFile sequence number of the last
                                       modification of this index block. */
    /* 16*/ leVCN index_block_vcn;  /* Virtual cluster number of the index block. */
    /* 24*/ INDEX_HEADER index;     /* Describes the following index entries. */
    /* sizeof()= 40 (0x28) bytes */
    /*
     * When creating the index block, we place the update sequence array at this
     * offset, i.e. before we start with the index entries. This also makes sense,
     * otherwise we could run into problems with the update sequence array
     * containing in itself the last two bytes of a sector which would mean that
     * multi sector transfer protection wouldn't work. As you can't protect data
     * by overwriting it since you then can't get it back...
     * When reading use the data from the ntfs record header.
     */
} __attribute__((__packed__)) INDEX_BLOCK;

typedef INDEX_BLOCK INDEX_ALLOCATION;

typedef struct {
    le32 reparse_tag;       /* Reparse point type (inc. flags). */
    leMFT_REF file_id;      /* Mft record of the file containing the
                               reparse point attribute. */
} __attribute__((__packed__)) REPARSE_INDEX_KEY;

/**
 * enum QUOTA_FLAGS - Quota flags (32-bit).
 */
typedef enum {
    /* The user quota flags. Names explain meaning. */
    QUOTA_FLAG_DEFAULT_LIMITS       = const_cpu_to_le32(0x00000001),
    QUOTA_FLAG_LIMIT_REACHED        = const_cpu_to_le32(0x00000002),
    QUOTA_FLAG_ID_DELETED           = const_cpu_to_le32(0x00000004),

    QUOTA_FLAG_USER_MASK            = const_cpu_to_le32(0x00000007),
            /* Bit mask for user quota flags. */

    /* These flags are only present in the quota defaults index entry,
       i.e. in the entry where owner_id = QUOTA_DEFAULTS_ID. */
    QUOTA_FLAG_TRACKING_ENABLED     = const_cpu_to_le32(0x00000010),
    QUOTA_FLAG_ENFORCEMENT_ENABLED  = const_cpu_to_le32(0x00000020),
    QUOTA_FLAG_TRACKING_REQUESTED   = const_cpu_to_le32(0x00000040),
    QUOTA_FLAG_LOG_THRESHOLD        = const_cpu_to_le32(0x00000080),
    QUOTA_FLAG_LOG_LIMIT            = const_cpu_to_le32(0x00000100),
    QUOTA_FLAG_OUT_OF_DATE          = const_cpu_to_le32(0x00000200),
    QUOTA_FLAG_CORRUPT              = const_cpu_to_le32(0x00000400),
    QUOTA_FLAG_PENDING_DELETES      = const_cpu_to_le32(0x00000800),
} QUOTA_FLAGS;

typedef struct {
    le32 version;           /* Currently equals 2. */
    QUOTA_FLAGS flags;      /* Flags describing this quota entry. */
    le64 bytes_used;                /* How many bytes of the quota are in use. */
    sle64 change_time;      /* Last time this quota entry was changed. */
    sle64 threshold;                /* Soft quota (-1 if not limited). */
    sle64 limit;            /* Hard quota (-1 if not limited). */
    sle64 exceeded_time;    /* How long the soft quota has been exceeded. */
    /* The below field is NOT present for the quota defaults entry. */
    SID sid;                /* The SID of the user/object associated with
                               this quota entry. If this field is missing
                               then the INDEX_ENTRY is padded to a multiple
                               of 8 with zeros which are not counted in
                               the data_length field. If the sid is present
                               then this structure is padded with zeros to
                               a multiple of 8 and the padding is counted in
                               the INDEX_ENTRY's data_length. */
} __attribute__((__packed__)) QUOTA_CONTROL_ENTRY;

/**
 * struct QUOTA_O_INDEX_DATA -
 */
typedef struct {
    le32 owner_id;
    le32 unknown;           /* Always 32. Seems to be padding and it's not
                               counted in the INDEX_ENTRY's data_length.
                               This field shouldn't be really here. */
} __attribute__((__packed__)) QUOTA_O_INDEX_DATA;

/**
 * enum PREDEFINED_OWNER_IDS - Predefined owner_id values (32-bit).
 */
typedef enum {
    QUOTA_INVALID_ID        = const_cpu_to_le32(0x00000000),
    QUOTA_DEFAULTS_ID       = const_cpu_to_le32(0x00000001),
    QUOTA_FIRST_USER_ID     = const_cpu_to_le32(0x00000100),
} PREDEFINED_OWNER_IDS;

typedef enum {
    INDEX_ENTRY_NODE = const_cpu_to_le16(1), /* This entry contains a
                                    sub-node, i.e. a reference to an index
                                    block in form of a virtual cluster
                                    number (see below). */
    INDEX_ENTRY_END  = const_cpu_to_le16(2), /* This signifies the last
                                    entry in an index block. The index
                                    entry does not represent a file but it
                                    can point to a sub-node. */
    INDEX_ENTRY_SPACE_FILLER = 0xffff, /* Just to force 16-bit width. */
} __attribute__((__packed__)) INDEX_ENTRY_FLAGS;

/**
 * struct INDEX_ENTRY_HEADER - This the index entry header (see below).
 *
 *         ==========================================================
 *         !!!!!  SEE DESCRIPTION OF THE FIELDS AT INDEX_ENTRY  !!!!!
 *         ==========================================================
 */
typedef struct {
    /*  0*/ union {
        leMFT_REF indexed_file;
        struct {
            le16 data_offset;
            le16 data_length;
            le32 reservedV;
        } __attribute__((__packed__));
    } __attribute__((__packed__));
    /*  8*/ le16 length;
    /* 10*/ le16 key_length;
    /* 12*/ INDEX_ENTRY_FLAGS flags;
    /* 14*/ le16 reserved;
    /* sizeof() = 16 bytes */
} __attribute__((__packed__)) INDEX_ENTRY_HEADER;
typedef struct {
/*  0   INDEX_ENTRY_HEADER; -- Unfolded here as gcc dislikes unnamed structs. */
        union {         /* Only valid when INDEX_ENTRY_END is not set. */
                leMFT_REF indexed_file;         /* The mft reference of the file
                                                   described by this index
                                                   entry. Used for directory
                                                   indexes. */
                struct { /* Used for views/indexes to find the entry's data. */
                        le16 data_offset;       /* Data byte offset from this
                                                   INDEX_ENTRY. Follows the
                                                   index key. */
                        le16 data_length;       /* Data length in bytes. */
                        le32 reservedV;         /* Reserved (zero). */
                } __attribute__((__packed__));
        } __attribute__((__packed__));
/*  8*/ le16 length;             /* Byte size of this index entry, multiple of
                                    8-bytes. Size includes INDEX_ENTRY_HEADER
                                    and the optional subnode VCN. See below. */
/* 10*/ le16 key_length;                 /* Byte size of the key value, which is in the
                                    index entry. It follows field reserved. Not
                                    multiple of 8-bytes. */
/* 12*/ INDEX_ENTRY_FLAGS ie_flags; /* Bit field of INDEX_ENTRY_* flags. */
/* 14*/ le16 reserved;           /* Reserved/align to 8-byte boundary. */
/*      End of INDEX_ENTRY_HEADER */
/* 16*/ union {         /* The key of the indexed attribute. NOTE: Only present
                           if INDEX_ENTRY_END bit in flags is not set. NOTE: On
                           NTFS versions before 3.0 the only valid key is the
                           FILE_NAME_ATTR. On NTFS 3.0+ the following
                           additional index keys are defined: */
                FILE_NAME_ATTR file_name;/* $I30 index in directories. */
                SII_INDEX_KEY sii;      /* $SII index in $Secure. */
                SDH_INDEX_KEY sdh;      /* $SDH index in $Secure. */
                GUID object_id;         /* $O index in FILE_Extend/$ObjId: The
                                           object_id of the mft record found in
                                           the data part of the index. */
                REPARSE_INDEX_KEY reparse;      /* $R index in
                                                   FILE_Extend/$Reparse. */
                SID sid;                /* $O index in FILE_Extend/$Quota:
                                           SID of the owner of the user_id. */
                le32 owner_id;          /* $Q index in FILE_Extend/$Quota:
                                           user_id of the owner of the quota
                                           control entry in the data part of
                                           the index. */
        } __attribute__((__packed__)) key;
        /* The (optional) index data is inserted here when creating.
        leVCN vcn;         If INDEX_ENTRY_NODE bit in ie_flags is set, the last
                           eight bytes of this index entry contain the virtual
                           cluster number of the index block that holds the
                           entries immediately preceding the current entry.

                           If the key_length is zero, then the vcn immediately
                           follows the INDEX_ENTRY_HEADER.

                           The address of the vcn of "ie" INDEX_ENTRY is given by
                           (char*)ie + le16_to_cpu(ie->length) - sizeof(VCN)
        */
} __attribute__((__packed__)) INDEX_ENTRY;

typedef struct {
    u8 bitmap[0];                   /* Array of bits. */
} __attribute__((__packed__)) BITMAP_ATTR;

typedef enum {
        IO_REPARSE_TAG_DIRECTORY        = const_cpu_to_le32(0x10000000),
        IO_REPARSE_TAG_IS_ALIAS         = const_cpu_to_le32(0x20000000),
        IO_REPARSE_TAG_IS_HIGH_LATENCY  = const_cpu_to_le32(0x40000000),
        IO_REPARSE_TAG_IS_MICROSOFT     = const_cpu_to_le32(0x80000000),

        IO_REPARSE_TAG_RESERVED_ZERO    = const_cpu_to_le32(0x00000000),
        IO_REPARSE_TAG_RESERVED_ONE     = const_cpu_to_le32(0x00000001),
        IO_REPARSE_TAG_RESERVED_RANGE   = const_cpu_to_le32(0x00000001),

        IO_REPARSE_TAG_CSV              = const_cpu_to_le32(0x80000009),
        IO_REPARSE_TAG_DEDUP            = const_cpu_to_le32(0x80000013),
        IO_REPARSE_TAG_DFS              = const_cpu_to_le32(0x8000000A),
        IO_REPARSE_TAG_DFSR             = const_cpu_to_le32(0x80000012),
        IO_REPARSE_TAG_HSM              = const_cpu_to_le32(0xC0000004),
        IO_REPARSE_TAG_HSM2             = const_cpu_to_le32(0x80000006),
        IO_REPARSE_TAG_MOUNT_POINT      = const_cpu_to_le32(0xA0000003),
        IO_REPARSE_TAG_NFS              = const_cpu_to_le32(0x80000014),
        IO_REPARSE_TAG_SIS              = const_cpu_to_le32(0x80000007),
        IO_REPARSE_TAG_SYMLINK          = const_cpu_to_le32(0xA000000C),
        IO_REPARSE_TAG_WIM              = const_cpu_to_le32(0x80000008),
        IO_REPARSE_TAG_DFM              = const_cpu_to_le32(0x80000016),
        IO_REPARSE_TAG_WOF              = const_cpu_to_le32(0x80000017),
        IO_REPARSE_TAG_WCI              = const_cpu_to_le32(0x80000018),
        IO_REPARSE_TAG_CLOUD            = const_cpu_to_le32(0x9000001A),
        IO_REPARSE_TAG_APPEXECLINK      = const_cpu_to_le32(0x8000001B),
        IO_REPARSE_TAG_GVFS             = const_cpu_to_le32(0x9000001C),
        IO_REPARSE_TAG_LX_SYMLINK       = const_cpu_to_le32(0xA000001D),
        IO_REPARSE_TAG_AF_UNIX          = const_cpu_to_le32(0x80000023),
        IO_REPARSE_TAG_LX_FIFO          = const_cpu_to_le32(0x80000024),
        IO_REPARSE_TAG_LX_CHR           = const_cpu_to_le32(0x80000025),
        IO_REPARSE_TAG_LX_BLK           = const_cpu_to_le32(0x80000026),

        IO_REPARSE_TAG_VALID_VALUES     = const_cpu_to_le32(0xf000ffff),
        IO_REPARSE_PLUGIN_SELECT        = const_cpu_to_le32(0xffff0fff),
} PREDEFINED_REPARSE_TAGS;

typedef struct {
    le32 reparse_tag;               /* Reparse point type (inc. flags). */
    le16 reparse_data_length;       /* Byte size of reparse data. */
    le16 reserved;                  /* Align to 8-byte boundary. */
    u8 reparse_data[0];             /* Meaning depends on reparse_tag. */
} __attribute__((__packed__)) REPARSE_POINT;

typedef struct {
    le16 ea_length;         /* Byte size of the packed extended
                               attributes. */
    le16 need_ea_count;     /* The number of extended attributes which have
                               the NEED_EA bit set. */
    le32 ea_query_length;   /* Byte size of the buffer required to query
                               the extended attributes when calling
                               ZwQueryEaFile() in Windows NT/2k. I.e. the
                               byte size of the unpacked extended
                               attributes. */
} __attribute__((__packed__)) EA_INFORMATION;

/**
 * enum EA_FLAGS - Extended attribute flags (8-bit).
 */
typedef enum {
    NEED_EA = 0x80,         /* Indicate that the file to which the EA
                               belongs cannot be interpreted without
                               understanding the associated extended
                               attributes. */
} __attribute__((__packed__)) EA_FLAGS;

/**
 * struct EA_ATTR - Attribute: Extended attribute (EA) (0xe0).
 *
 * Like the attribute list and the index buffer list, the EA attribute value is
 * a sequence of EA_ATTR variable length records.
 *
 * FIXME: It appears weird that the EA name is not Unicode. Is it true?
 * FIXME: It seems that name is always uppercased. Is it true?
 */
typedef struct {
    le32 next_entry_offset; /* Offset to the next EA_ATTR. */
    EA_FLAGS flags;         /* Flags describing the EA. */
    u8 name_length;         /* Length of the name of the extended
                               attribute in bytes. */
    le16 value_length;      /* Byte size of the EA's value. */
    u8 name[0];             /* Name of the EA. */
    u8 value[0];            /* The value of the EA. Immediately
                               follows the name. */
} __attribute__((__packed__)) EA_ATTR;

/**
 * struct PROPERTY_SET - Attribute: Property set (0xf0).
 *
 * Intended to support Native Structure Storage (NSS) - a feature removed from
 * NTFS 3.0 during beta testing.
 */
typedef struct {
    /* Irrelevant as feature unused. */
} __attribute__((__packed__)) PROPERTY_SET;

/**
 * struct LOGGED_UTILITY_STREAM - Attribute: Logged utility stream (0x100).
 *
 * NOTE: Can be resident or non-resident.
 *
 * Operations on this attribute are logged to the journal ($LogFile) like
 * normal metadata changes.
 *
 * Used by the Encrypting File System (EFS).  All encrypted files have this
 * attribute with the name $EFS.  See below for the relevant structures.
 */
typedef struct {
    /* Can be anything the creator chooses. */
} __attribute__((__packed__)) LOGGED_UTILITY_STREAM;

/**
 * struct EFS_ATTR_HEADER - "$EFS" header.
 *
 * The header of the Logged utility stream (0x100) attribute named "$EFS".
 */
typedef struct {
/*  0*/ le32 length;            /* Length of EFS attribute in bytes. */
        le32 state;             /* Always 0? */
        le32 version;           /* Efs version.  Always 2? */
        le32 crypto_api_version;        /* Always 0? */
/* 16*/ u8 unknown4[16];        /* MD5 hash of decrypted FEK?  This field is
                                   created with a call to UuidCreate() so is
                                   unlikely to be an MD5 hash and is more
                                   likely to be GUID of this encrytped file
                                   or something like that. */
/* 32*/ u8 unknown5[16];        /* MD5 hash of DDFs? */
/* 48*/ u8 unknown6[16];        /* MD5 hash of DRFs? */
/* 64*/ le32 offset_to_ddf_array;/* Offset in bytes to the array of data
                                   decryption fields (DDF), see below.  Zero if
                                   no DDFs are present. */
        le32 offset_to_drf_array;/* Offset in bytes to the array of data
                                   recovery fields (DRF), see below.  Zero if
                                   no DRFs are present. */
        le32 reserved;          /* Reserved. */
} __attribute__((__packed__)) EFS_ATTR_HEADER;

/**
 * struct EFS_DF_ARRAY_HEADER -
 */
typedef struct {
        le32 df_count;          /* Number of data decryption/recovery fields in
                                   the array. */
} __attribute__((__packed__)) EFS_DF_ARRAY_HEADER;

/**
 * struct EFS_DF_HEADER -
 */
typedef struct {
/*  0*/ le32 df_length;         /* Length of this data decryption/recovery
                                   field in bytes. */
        le32 cred_header_offset;        /* Offset in bytes to the credential header. */
        le32 fek_size;          /* Size in bytes of the encrypted file
                                   encryption key (FEK). */
        le32 fek_offset;                /* Offset in bytes to the FEK from the start of
                                   the data decryption/recovery field. */
/* 16*/ le32 unknown1;          /* always 0?  Might be just padding. */
} __attribute__((__packed__)) EFS_DF_HEADER;

/**
 * struct EFS_DF_CREDENTIAL_HEADER -
 */
typedef struct {
/*  0*/ le32 cred_length;       /* Length of this credential in bytes. */
        le32 sid_offset;                /* Offset in bytes to the user's sid from start
                                   of this structure.  Zero if no sid is
                                   present. */
/*  8*/ le32 type;              /* Type of this credential:
                                        1 = CryptoAPI container.
                                        2 = Unexpected type.
                                        3 = Certificate thumbprint.
                                        other = Unknown type. */
        union {
                /* CryptoAPI container. */
                struct {
/* 12*/                 le32 container_name_offset;     /* Offset in bytes to
                                   the name of the container from start of this
                                   structure (may not be zero). */
/* 16*/                 le32 provider_name_offset;      /* Offset in bytes to
                                   the name of the provider from start of this
                                   structure (may not be zero). */
                        le32 public_key_blob_offset;    /* Offset in bytes to
                                   the public key blob from start of this
                                   structure. */
/* 24*/                 le32 public_key_blob_size;      /* Size in bytes of
                                   public key blob. */
                } __attribute__((__packed__));
                /* Certificate thumbprint. */
                struct {
/* 12*/                 le32 cert_thumbprint_header_size;       /* Size in
                                   bytes of the header of the certificate
                                   thumbprint. */
/* 16*/                 le32 cert_thumbprint_header_offset;     /* Offset in
                                   bytes to the header of the certificate
                                   thumbprint from start of this structure. */
                        le32 unknown1;  /* Always 0?  Might be padding... */
                        le32 unknown2;  /* Always 0?  Might be padding... */
                } __attribute__((__packed__));
        } __attribute__((__packed__));
} __attribute__((__packed__)) EFS_DF_CREDENTIAL_HEADER;

typedef EFS_DF_CREDENTIAL_HEADER EFS_DF_CRED_HEADER;
/**
 * struct EFS_DF_CERTIFICATE_THUMBPRINT_HEADER -
 */
typedef struct {
/*  0*/ le32 thumbprint_offset;         /* Offset in bytes to the thumbprint. */
        le32 thumbprint_size;           /* Size of thumbprint in bytes. */
/*  8*/ le32 container_name_offset;     /* Offset in bytes to the name of the
                                           container from start of this
                                           structure or 0 if no name present. */
        le32 provider_name_offset;      /* Offset in bytes to the name of the
                                           cryptographic provider from start of
                                           this structure or 0 if no name
                                           present. */
/* 16*/ le32 user_name_offset;          /* Offset in bytes to the user name
                                           from start of this structure or 0 if
                                           no user name present.  (This is also
                                           known as lpDisplayInformation.) */
} __attribute__((__packed__)) EFS_DF_CERTIFICATE_THUMBPRINT_HEADER;

typedef EFS_DF_CERTIFICATE_THUMBPRINT_HEADER EFS_DF_CERT_THUMBPRINT_HEADER;

typedef enum
{
        INTX_SYMBOLIC_LINK =
                const_cpu_to_le64(0x014B4E4C78746E49ULL), /* "IntxLNK\1" */
        INTX_CHARACTER_DEVICE =
                const_cpu_to_le64(0x0052484378746E49ULL), /* "IntxCHR\0" */
        INTX_BLOCK_DEVICE =
                const_cpu_to_le64(0x004B4C4278746E49ULL), /* "IntxBLK\0" */
} INTX_FILE_TYPES;

typedef struct
{
        INTX_FILE_TYPES magic;          /* Intx file magic. */
        union {
                /* For character and block devices. */
                struct {
                        le64 major;             /* Major device number. */
                        le64 minor;             /* Minor device number. */
                        void *device_end[0];    /* Marker for offsetof(). */
                } __attribute__((__packed__));
                /* For symbolic links. */
                ntfschar target[0];
        } __attribute__((__packed__));
} __attribute__((__packed__)) INTX_FILE;


C_END_EXTERN_C

#endif // sandbox_LAYOUT_H
