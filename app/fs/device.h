//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_DEVICE_H
#define sandbox_DEVICE_H

#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

#define test_ndev_flag(nd, flag)          test_bit(ND_##flag, (nd)->d_state)
#define set_ndev_flag(nd, flag)           set_bit(ND_##flag, (nd)->d_state)
#define clear_ndev_flag(nd, flag)         clear_bit(ND_##flag, (nd)->d_state)

#define NDevOpen(nd)             test_ndev_flag(nd, Open)
#define NDevSetOpen(nd)           set_ndev_flag(nd, Open)
#define NDevClearOpen(nd)       clear_ndev_flag(nd, Open)

#define NDevReadOnly(nd)         test_ndev_flag(nd, ReadOnly)
#define NDevSetReadOnly(nd)       set_ndev_flag(nd, ReadOnly)
#define NDevClearReadOnly(nd)   clear_ndev_flag(nd, ReadOnly)

#define NDevDirty(nd)            test_ndev_flag(nd, Dirty)
#define NDevSetDirty(nd)          set_ndev_flag(nd, Dirty)
#define NDevClearDirty(nd)      clear_ndev_flag(nd, Dirty)

#define NDevBlock(nd)            test_ndev_flag(nd, Block)
#define NDevSetBlock(nd)          set_ndev_flag(nd, Block)
#define NDevClearBlock(nd)      clear_ndev_flag(nd, Block)

#define NDevSync(nd)             test_ndev_flag(nd, Sync)
#define NDevSetSync(nd)           set_ndev_flag(nd, Sync)
#define NDevClearSync(nd)       clear_ndev_flag(nd, Sync)


typedef enum {
    FSD_OPEN,               /* 1: Device is open. */
    FSD_READONLY,           /* 1: Device is read-only. */
    FSD_DIRTY,              /* 1: Device is dirty, needs sync. */
    FSD_BLOCK,              /* 1: Device is a block device. */
    FSD_SYNC,               /* 1: Device is mounted with "-o sync" */
} FSDeviceStateBits;

struct _FSDevice
{
    FSDeviceOperations*         dOps;                   /* Device operations. */
    unsigned long               dState;                 /* State of the device. */
    char*                       dName;                  /* Name of device. */
    void*                       dPrivate;               /* Private data used by the device operations. */
    int                         dHeads;                 /* Disk geometry: number of heads or -1. */
    int                         dSectorsPerTrack;       /* Disk geometry: number of sectors per track or -1. */
};


struct _FSDeviceOperations
{
    int     (*open)     (FSDevice* dev, int flags);
    int     (*close)    (FSDevice* dev);
    csize   (*seek)     (FSDevice* dev, csize offset, int whence);
    csize   (*read)     (FSDevice* dev, void *buf, csize count);
    csize   (*write)    (FSDevice* dev, const void *buf, csize count);
    csize   (*pread)    (FSDevice* dev, void *buf, csize count, csize offset);
    csize   (*pwrite)   (FSDevice* dev, const void *buf, csize count, csize offset);
    int     (*sync)     (FSDevice* dev);
    int     (*stat)     (FSDevice* dev, struct stat *buf);
    int     (*ioctl)    (FSDevice* dev, unsigned long request, void *argp);
};

FSDevice*       fs_device_alloc                         (const char *name, const long state, FSDeviceOperations* dops, void* privData);
int             fs_device_free                          (FSDevice* dev);
int             fs_device_sync                          (FSDevice* dev);
csize           fs_pread                                (FSDevice* dev, const csize pos, csize count, void *b);
csize           fs_pwrite                               (FSDevice* dev, const csize pos, csize count, const void *b);
csize           fs_mst_pread                            (FSDevice* dev, const csize pos, csize count, const cuint32 bksize, void *b);
csize           fs_mst_pwrite                           (FSDevice* dev, const csize pos, csize count, const cuint32 bksize, void *b);
csize           fs_cluster_read                         (FSVolume* vol, const csize lcn, const csize count, void *b);
csize           fs_cluster_write                        (const FSVolume* vol, const csize lcn, const csize count, const void *b);
csize           fs_device_size_get                      (FSDevice* dev, int blockSize);
csize           fs_device_partition_start_sector_get    (FSDevice* dev);
int             fs_device_heads_get                     (FSDevice* dev);
int             fs_device_sectors_per_track_get         (FSDevice* dev);
int             fs_device_sector_size_get               (FSDevice* dev);
int             fs_device_block_size_set                (FSDevice* dev, int blockSize);


C_END_EXTERN_C

#endif // sandbox_DEVICE_H
