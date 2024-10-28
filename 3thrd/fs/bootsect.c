/**
 * bootsect.c - Boot sector handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2003-2008 Szabolcs Szakacsits
 * Copyright (c)      2005 Yura Pakhuchiy
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "param.h"
#include "compat.h"
#include "defines.h"
#include "bootsect.h"
#include "debug.h"
#include "logging.h"
#include "c/log.h"

extern uint64_t gVolumeSize;

/**
 * ntfs_boot_sector_is_ntfs - check if buffer contains a valid ntfs boot sector
 * @b:        buffer containing putative boot sector to analyze
 * @silent:    if zero, output progress messages to stderr
 *
 * Check if the buffer @b contains a valid ntfs boot sector. The buffer @b
 * must be at least 512 bytes in size.
 *
 * If @silent is zero, output progress messages to stderr. Otherwise, do not
 * output any messages (except when configured with --enable-debug in which
 * case warning/debug messages may be displayed).
 *
 * Return TRUE if @b contains a valid ntfs boot sector and FALSE if not.
 */
BOOL ntfs_boot_sector_is_ntfs(NTFS_BOOT_SECTOR *b)
{
    u32 i;
    BOOL ret = FALSE;
    u16 sectors_per_cluster;

    C_LOG_VERB("Beginning bootsector check.");

    C_LOG_VERB("Checking OEMid, NTFS signature.");
    if (b->oem_id != const_cpu_to_le64(0x202020205346544eULL)) { /* "Andsec  " */
        C_LOG_WARNING("Sandbox FS signature is missing.: %x == %x", b->oem_id, const_cpu_to_le64(0x202020205346544eULL));
        goto not_ntfs;
    }

    C_LOG_VERB("Checking bytes per sector.");
    if (le16_to_cpu(b->bpb.bytes_per_sector) <  256 ||
        le16_to_cpu(b->bpb.bytes_per_sector) > 4096) {
        C_LOG_WARNING("Unexpected bytes per sector value (%d).", le16_to_cpu(b->bpb.bytes_per_sector));
        goto not_ntfs;
    }

    C_LOG_VERB("Checking sectors per cluster.");
    switch (b->bpb.sectors_per_cluster) {
    case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128: {
        break;
    }
    default:
        if ((b->bpb.sectors_per_cluster < 240) || (b->bpb.sectors_per_cluster > 253)) {
            if (b->bpb.sectors_per_cluster > 128) {
                C_LOG_WARNING("Unexpected sectors per cluster value (code 0x%x)", b->bpb.sectors_per_cluster);
            }
            else {
                C_LOG_WARNING("Unexpected sectors per cluster value (%d).", b->bpb.sectors_per_cluster);
            }
            goto not_ntfs;
        }
    }

    C_LOG_VERB("Checking cluster size.");
    if (b->bpb.sectors_per_cluster > 128) {
        sectors_per_cluster = 1 << (256 - b->bpb.sectors_per_cluster);
    }
    else {
        sectors_per_cluster = b->bpb.sectors_per_cluster;
    }
    i = (u32)le16_to_cpu(b->bpb.bytes_per_sector) * sectors_per_cluster;
    if (i > NTFS_MAX_CLUSTER_SIZE) {
        C_LOG_WARNING("Unexpected cluster size (%d).", i);
        goto not_ntfs;
    }

    C_LOG_VERB("Checking reserved fields are zero.");
    if (le16_to_cpu(b->bpb.reserved_sectors) || le16_to_cpu(b->bpb.root_entries) || le16_to_cpu(b->bpb.sectors) || le16_to_cpu(b->bpb.sectors_per_fat) || le32_to_cpu(b->bpb.large_sectors) ||
        b->bpb.fats) {
        C_LOG_WARNING("Reserved fields aren't zero (%d, %d, %d, %d, %d, %d).", le16_to_cpu(b->bpb.reserved_sectors), le16_to_cpu(b->bpb.root_entries), le16_to_cpu(b->bpb.sectors), le16_to_cpu(b->bpb.sectors_per_fat), le32_to_cpu(b->bpb.large_sectors), b->bpb.fats);
        goto not_ntfs;
    }

    C_LOG_VERB("Checking clusters per mft record.");
    if ((u8)b->clusters_per_mft_record < 0xe1 || (u8)b->clusters_per_mft_record > 0xf7) {
        switch (b->clusters_per_mft_record) {
        case 1: case 2: case 4: case 8: case 0x10: case 0x20: case 0x40: {
            break;
        }
        default: {
            C_LOG_WARNING("Unexpected clusters per mft record (%d).", b->clusters_per_mft_record);
            goto not_ntfs;
        }
        }
    }

    C_LOG_VERB("Checking clusters per index block.");
    if ((u8)b->clusters_per_index_record < 0xe1 || (u8)b->clusters_per_index_record > 0xf7) {
        switch (b->clusters_per_index_record) {
        case 1: case 2: case 4: case 8: case 0x10: case 0x20: case 0x40: {
            break;
        }
        default: {
            C_LOG_WARNING("Unexpected clusters per index record (%d).", b->clusters_per_index_record);
            goto not_ntfs;
        }
        }
    }

    /* MFT and MFTMirr may not overlap the boot sector or be the same */
    if (((s64)sle64_to_cpu(b->mft_lcn) <= 0) || ((s64)sle64_to_cpu(b->mftmirr_lcn) <= 0) || (b->mft_lcn == b->mftmirr_lcn)) {
        C_LOG_WARNING("Invalid location of MFT or MFTMirr.");
        goto not_ntfs;
    }

    if (b->end_of_sector_marker != const_cpu_to_le16(0xaa55)) {
        C_LOG_VERB("Warning: Bootsector has invalid end of sector marker.");
    }

    C_LOG_VERB("Bootsector check completed successfully.");

    ret = TRUE;

not_ntfs:
    return ret;
}

static const char *last_sector_error =
"HINTS: Either the volume is a RAID/LDM but it wasn't setup yet,\n"
"   or it was not setup correctly (e.g. by not using mdadm --build ...),\n"
"   or a wrong device is tried to be mounted,\n"
"   or the partition table is corrupt (partition is smaller than NTFS),\n"
"   or the NTFS boot sector is corrupt (NTFS size is not valid).\n";

/**
 * ntfs_boot_sector_parse - setup an ntfs volume from an ntfs boot sector
 * @vol:    ntfs_volume to setup
 * @bs:        buffer containing ntfs boot sector to parse
 *
 * Parse the ntfs bootsector @bs and setup the ntfs volume @vol with the
 * obtained values.
 *
 * Return 0 on success or -1 on error with errno set to the error code EINVAL.
 */
int ntfs_boot_sector_parse(ntfs_volume *vol, const NTFS_BOOT_SECTOR *bs)
{
    s64 sectors;
    u16  sectors_per_cluster;
    s8  c;

    /* We return -1 with errno = EINVAL on error. */
    errno = EINVAL;

    vol->sector_size = le16_to_cpu(bs->bpb.bytes_per_sector);           // 512B
    vol->sector_size_bits = ffs(vol->sector_size) - 1;                  //
    // C_LOG_INFO("parse boot:\n"
               // "SectorSize = 0x%x\n"
               // "SectorSizeBits = %u",
               // vol->sector_size, vol->sector_size_bits);
    /*
     * The bounds checks on mft_lcn and mft_mirr_lcn (i.e. them being
     * below or equal the number_of_clusters) really belong in the
     * ntfs_boot_sector_is_ntfs but in this way we can just do this once.
     */
    if (bs->bpb.sectors_per_cluster > 128) {
        sectors_per_cluster = 1 << (256 - bs->bpb.sectors_per_cluster);
    }
    else {
        sectors_per_cluster = bs->bpb.sectors_per_cluster;              // 8
    }

    C_LOG_VERB("SectorsPerCluster = 0x%x", sectors_per_cluster);
    if (sectors_per_cluster & (sectors_per_cluster - 1)) {
        C_LOG_WARNING("sectors_per_cluster (%d) is not a power of 2.", sectors_per_cluster);
        return -1;
    }

    sectors = sle64_to_cpu(bs->number_of_sectors);                      // 100MB -- 204799 ==> (204799 / 2 / 1024) = 99MB
    C_LOG_VERB("NumberOfSectors = %lld", (long long)sectors);
    if (!sectors) {
        C_LOG_WARNING("Volume size is set to zero.");
        return -1;
    }

    gVolumeSize = sectors * vol->sector_size + 1024;

    // 最后一个扇区
    if (vol->dev->d_ops->seek(vol->dev, (sectors - 1) << vol->sector_size_bits, SEEK_SET) == -1) {
        ntfs_log_perror("Failed to read last sector (%lld)", (long long)(sectors - 1));
        C_LOG_WARNING("%s", last_sector_error);
        return -1;
    }

    vol->nr_clusters =  sectors >> (ffs(sectors_per_cluster) - 1);      // 204799 >> (ffs(8) - 1) = 25599，相当于除以8，速度优化

    vol->mft_lcn = sle64_to_cpu(bs->mft_lcn);                           // 4
    vol->mftmirr_lcn = sle64_to_cpu(bs->mftmirr_lcn);                   // 12799
    C_LOG_VERB("MFT LCN = %lld", (long long)vol->mft_lcn);
    C_LOG_VERB("MFTMirr LCN = %lld", (long long)vol->mftmirr_lcn);
    if ((vol->mft_lcn < 0 || vol->mft_lcn > vol->nr_clusters) || (vol->mftmirr_lcn < 0 || vol->mftmirr_lcn > vol->nr_clusters)) {
        C_LOG_WARNING("$MFT LCN (%lld) or $MFTMirr LCN (%lld) is greater than the number of clusters (%lld).", (long long)vol->mft_lcn, (long long)vol->mftmirr_lcn, (long long)vol->nr_clusters);
        return -1;
    }

    vol->cluster_size = sectors_per_cluster * vol->sector_size;         // 每块 sectors 数量 x sector大小 8 * 512B = 4KiB
    if (vol->cluster_size & (vol->cluster_size - 1)) {
        C_LOG_WARNING("cluster_size (%d) is not a power of 2.", vol->cluster_size);
        return -1;
    }
    vol->cluster_size_bits = ffs(vol->cluster_size) - 1;                // 12 ==> 4096

    /*
     * Need to get the clusters per mft record and handle it if it is
     * negative. Then calculate the mft_record_size. A value of 0x80 is
     * illegal, thus signed char is actually ok!
     *
     * 需要获取每个mft记录的集群，如果它是负的，则处理它。然后计算mft_record_size。值0x80是非法的，因此signed char实际上是可以的!
     */
    c = bs->clusters_per_mft_record;                                    // 1KiB
    // C_LOG_INFO(""
               // "ClusterSize = 0x%x\n"
                // "ClusterSizeBits = %u\n",
                // "ClustersPerMftRecord = 0x%x\n",
                // (unsigned)vol->cluster_size,
                // vol->cluster_size_bits, c);
    /*
     * When clusters_per_mft_record is negative, it means that it is to
     * be taken to be the negative base 2 logarithm of the mft_record_size
     * min bytes. Then:
     *     mft_record_size = 2^(-clusters_per_mft_record) bytes.
     */
    if (c < 0) {
        vol->mft_record_size = 1 << -c;                                 // 1KB
    }
    else {
        vol->mft_record_size = c << vol->cluster_size_bits;
    }

    if (vol->mft_record_size & (vol->mft_record_size - 1)) {
        C_LOG_WARNING("mft_record_size (%d) is not a power of 2.", vol->mft_record_size);
        return -1;
    }
    vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;          // 10
    // C_LOG_INFO("MftRecordSizeBits = %u\n"
               // "MftRecordSize = 0x%x",
                // vol->mft_record_size_bits,
               // (unsigned)vol->mft_record_size);

    /* Same as above for INDX record. */
    c = bs->clusters_per_index_record;                                  // 1
    // C_LOG_INFO("ClustersPerINDXRecord = 0x%x", c);
    if (c < 0) {
        vol->indx_record_size = 1 << -c;
    }
    else {
        vol->indx_record_size = c << vol->cluster_size_bits;            // 1 << 12, 4KiB
    }
    vol->indx_record_size_bits = ffs(vol->indx_record_size) - 1;        // 12
    // C_LOG_INFO("INDXRecordSize = 0x%x", (unsigned)vol->indx_record_size);
    // C_LOG_INFO("INDXRecordSizeBits = %u", vol->indx_record_size_bits);

    /**
     * Work out the size of the MFT mirror in number of mft records. If the
     * cluster size is less than or equal to the size taken by four mft
     * records, the mft mirror stores the first four mft records. If the
     * cluster size is bigger than the size taken by four mft records, the
     * mft mirror contains as many mft records as will fit into one
     * cluster.
     */
    if (vol->cluster_size <= 4 * vol->mft_record_size) {
        vol->mftmirr_size = 4;
    }
    else {
        vol->mftmirr_size = vol->cluster_size / vol->mft_record_size;   // 4KiB / 1KiB = 4
    }

    return 0;
}

