//
// Created by dingjing on 6/10/24.
//

#include "volume.h"

#include <fcntl.h>
#include <locale.h>

#include "mft.h"
#include "attr.h"
#include "cache.h"
#include "inode.h"
#include "device.h"
#include "layout.h"
#include "unistr.h"
#include "runlist.h"
#include "log-file.h"
#include "bootsect.h"
#include "security.h"


const char * ntfs_home =
    "News, support and information:  https://github.com/tuxera/ntfs-3g/\n";

static const char * invalid_ntfs_msg =
    "The device '%s' doesn't seem to have a valid NTFS.\n"
    "Maybe the wrong device is used? Or the whole disk instead of a\n"
    "partition (e.g. /dev/sda, not /dev/sda1)? Or the other way around?\n";

static const char * corrupt_volume_msg =
    "NTFS is either inconsistent, or there is a hardware fault, or it's a\n"
    "SoftRAID/FakeRAID hardware. In the first case run chkdsk /f on Windows\n"
    "then reboot into Windows twice. The usage of the /f parameter is very\n"
    "important! If the device is a SoftRAID/FakeRAID then first activate\n"
    "it and mount a different device under the /dev/mapper/ directory, (e.g.\n"
    "/dev/mapper/nvidia_eahaabcc1). Please see the 'dmraid' documentation\n"
    "for more details.\n";

static const char * hibernated_volume_msg =
    "The NTFS partition is in an unsafe state. Please resume and shutdown\n"
    "Windows fully (no hibernation or fast restarting), or mount the volume\n"
    "read-only with the 'ro' mount option.\n";

static const char * fallback_readonly_msg =
    "Falling back to read-only mount because the NTFS partition is in an\n"
    "unsafe state. Please resume and shutdown Windows fully (no hibernation\n"
    "or fast restarting.)\n";

static const char * unclean_journal_msg =
    "Write access is denied because the disk wasn't safely powered\n"
    "off and the 'norecover' mount option was specified.\n";

static const char * opened_volume_msg =
    "Mount is denied because the NTFS volume is already exclusively opened.\n"
    "The volume may be already mounted, or another software may use it which\n"
    "could be identified for example by the help of the 'fuser' command.\n";

static const char * fakeraid_msg =
    "Either the device is missing or it's powered down, or you have\n"
    "SoftRAID hardware and must use an activated, different device under\n"
    "/dev/mapper/, (e.g. /dev/mapper/nvidia_eahaabcc1) to mount NTFS.\n"
    "Please see the 'dmraid' documentation for help.\n";

static const char * access_denied_msg =
    "Please check '%s' and the ntfs-3g binary permissions,\n"
    "and the mounting user ID. More explanation is provided at\n"
    "https://github.com/tuxera/ntfs-3g/wiki/NTFS-3G-FAQ\n";


FSVolume* fs_volume_alloc(void)
{
    return c_malloc0(sizeof(FSVolume));
}

static void fs_attr_free(FSAttr** na)
{
    if (na && *na) {
        fs_attr_close(*na);
        *na = NULL;
    }
}

static int fs_inode_free(FSInode ** ni)
{
    int ret = -1;

    if (ni && *ni) {
        ret = fs_inode_close(*ni);
        *ni = NULL;
    }

    return ret;
}

static void fs_error_set(int * err)
{
    if (!*err) {
        *err = errno;
    }
}

static int __fs_volume_release(FSVolume * v)
{
    int err = 0;

    if (fs_close_secure(v))
        fs_error_set(&err);

    if (fs_inode_free(&v->volNi))
        fs_error_set(&err);
    /*
     * FIXME: Inodes must be synced before closing
     * attributes, otherwise unmount could fail.
     */
    if (v->lcnbmpNi && NInoDirty(v->lcnbmpNi))
        fs_inode_sync(v->lcnbmpNi);
    fs_attr_free(&v->lcnbmpNa);
    if (fs_inode_free(&v->lcnbmpNi))
        fs_error_set(&err);

    if (v->mftNi && NInoDirty(v->mftNi))
        fs_inode_sync(v->mftNi);
    fs_attr_free(&v->mftbmpNa);
    fs_attr_free(&v->mftNa);
    if (fs_inode_free(&v->mftNi))
        fs_error_set(&err);

    if (v->mftmirrNi && NInoDirty(v->mftmirrNi))
        fs_inode_sync(v->mftmirrNi);
    fs_attr_free(&v->mftmirrNa);
    if (fs_inode_free(&v->mftmirrNi))
        fs_error_set(&err);

    if (v->dev) {
        FSDevice * dev = v->dev;

        if (dev->dOps->sync(dev))
            fs_error_set(&err);
        if (dev->dOps->close(dev))
            fs_error_set(&err);
    }

    fs_free_lru_caches(v);
    free(v->volName);
    free(v->upcase);
    if (v->locase) free(v->locase);
    free(v->attrdef);
    free(v);

    errno = err;
    return errno ? -1 : 0;
}

static int fs_attr_setup_flag(FSInode* ni)
{
    STANDARD_INFORMATION * si;
    cint64 lth;
    int r;

    si = (STANDARD_INFORMATION*)fs_attr_readall(ni, AT_STANDARD_INFORMATION, AT_UNNAMED, 0, &lth);
    if (si) {
        if ((cuint64)lth >= offsetof(STANDARD_INFORMATION, owner_id))
            ni->flags = si->file_attributes;
        free(si);
        r = 0;
    }
    else {
        C_LOG_ERROR("Failed to get standard information of $MFT\n");
        r = -1;
    }
    return (r);
}


static int fs_mft_load(FSVolume * vol)
{
    VCN next_vcn, last_vcn, highest_vcn;
    cint64 l;
    MFT_RECORD * mb = NULL;
    FSAttrSearchCtx * ctx = NULL;
    ATTR_RECORD * a;
    int eo;

    /* Manually setup an fs_inode. */
    vol->mftNi = fs_inode_allocate(vol);
    mb = c_malloc0(vol->mftRecordSize);
    if (!vol->mftNi || !mb) {
        C_LOG_ERROR("Error allocating memory for $MFT");
        goto error_exit;
    }
    vol->mftNi->mftNo = 0;
    vol->mftNi->mrec = mb;
    /* Can't use any of the higher level functions yet! */
    l = fs_mst_pread(vol->dev, vol->mftLcn << vol->clusterSizeBits, 1, vol->mftRecordSize, mb);
    if (l != 1) {
        if (l != -1)
            errno = EIO;
        C_LOG_ERROR("Error reading $MFT");
        goto error_exit;
    }

    if (fs_mft_record_check(vol, 0, mb))
        goto error_exit;

    ctx = fs_attr_get_search_ctx(vol->mftNi, NULL);
    if (!ctx)
        goto error_exit;

    /* Find the $ATTRIBUTE_LIST attribute in $MFT if present. */
    if (fs_attr_lookup(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0, 0, 0, NULL, 0, ctx)) {
        if (errno != ENOENT) {
            C_LOG_ERROR("$MFT has corrupt attribute list.");
            goto io_error_exit;
        }
        goto mft_has_no_attr_list;
    }
    NInoSetAttrList(vol->mftNi);
    l = fs_get_attribute_value_length(ctx->attr);
    if (l <= 0 || l > 0x40000) {
        C_LOG_ERROR("$MFT/$ATTR_LIST invalid length (%lld).", (long long)l);
        goto io_error_exit;
    }
    vol->mftNi->attr_list_size = l;
    vol->mftNi->attr_list = c_malloc0(l);
    if (!vol->mftNi->attr_list)
        goto error_exit;

    l = fs_get_attribute_value(vol, ctx->attr, vol->mftNi->attr_list);
    if (!l) {
        C_LOG_ERROR("Failed to get value of $MFT/$ATTR_LIST.");
        goto io_error_exit;
    }
    if ((l != vol->mftNi->attr_list_size) || (l < (cint64)offsetof(ATTR_LIST_ENTRY, name))) {
        C_LOG_ERROR("Partial read of $MFT/$ATTR_LIST (%lld != %u or < %d).", (long long)l, vol->mftNi->attr_list_size, (int)offsetof(ATTR_LIST_ENTRY, name));
        goto io_error_exit;
    }

mft_has_no_attr_list:
    if (fs_attr_setup_flag(vol->mftNi))
        goto error_exit;

    /* We now have a fully setup ntfs inode for $MFT in vol->mftNi. */

    /* Get an ntfs attribute for $MFT/$DATA and set it up, too. */
    vol->mftNa = fs_attr_open(vol->mftNi, AT_DATA, AT_UNNAMED, 0);
    if (!vol->mftNa) {
        C_LOG_ERROR("Failed to open ntfs attribute");
        goto error_exit;
    }
    /* Read all extents from the $DATA attribute in $MFT. */
    fs_attr_reinit_search_ctx(ctx);
    last_vcn = vol->mftNa->allocatedSize >> vol->clusterSizeBits;
    highest_vcn = next_vcn = 0;
    a = NULL;
    while (!fs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, next_vcn, NULL, 0, ctx)) {
        RunlistElement * nrl;

        a = ctx->attr;
        /* $MFT must be non-resident. */
        if (!a->non_resident) {
            C_LOG_ERROR("$MFT must be non-resident.\n");
            goto io_error_exit;
        }
        /* $MFT must be uncompressed and unencrypted. */
        if (a->flags & ATTR_COMPRESSION_MASK ||
            a->flags & ATTR_IS_ENCRYPTED) {
            C_LOG_ERROR("$MFT must be uncompressed and "
                "unencrypted.\n");
            goto io_error_exit;
        }
        /*
         * Decompress the mapping pairs array of this extent and merge
         * the result into the existing runlist. No need for locking
         * as we have exclusive access to the inode at this time and we
         * are a mount in progress task, too.
         */
        nrl = fs_mapping_pairs_decompress(vol, a, vol->mftNa->rl);
        if (!nrl) {
            C_LOG_ERROR("fs_mapping_pairs_decompress() failed");
            goto error_exit;
        }
        /* Make sure $DATA is the MFT itself */
        if (nrl->lcn != vol->mftLcn) {
            C_LOG_ERROR("The MFT is not self-contained");
            goto error_exit;
        }
        vol->mftNa->rl = nrl;

        /* Get the lowest vcn for the next extent. */
        highest_vcn = sle64_to_cpu(a->highest_vcn);
        next_vcn = highest_vcn + 1;

        /* Only one extent or error, which we catch below. */
        if (next_vcn <= 0)
            break;

        /* Avoid endless loops due to corruption. */
        if (next_vcn < sle64_to_cpu(a->lowest_vcn)) {
            C_LOG_ERROR("$MFT has corrupt attribute list.\n");
            goto io_error_exit;
        }
    }
    if (!a) {
        C_LOG_ERROR("$MFT/$DATA attribute not found.\n");
        goto io_error_exit;
    }
    if (highest_vcn && highest_vcn != last_vcn - 1) {
        C_LOG_ERROR("Failed to load runlist for $MFT/$DATA.");
        C_LOG_ERROR("highest_vcn = 0x%llx, last_vcn - 1 = 0x%llx", (long long)highest_vcn, (long long)last_vcn - 1);
        goto io_error_exit;
    }
    /* Done with the $Mft mft record. */
    fs_attr_put_search_ctx(ctx);
    ctx = NULL;

    /* Update the size fields in the inode. */
    vol->mftNi->data_size = vol->mftNa->dataSize;
    vol->mftNi->allocated_size = vol->mftNa->allocatedSize;
    set_nino_flag(vol->mftNi, KnownSize);

    /*
     * The volume is now setup so we can use all read access functions.
     */
    vol->mftbmpNa = fs_attr_open(vol->mftNi, AT_BITMAP, AT_UNNAMED, 0);
    if (!vol->mftbmpNa) {
        C_LOG_ERROR("Failed to open $MFT/$BITMAP");
        goto error_exit;
    }
    return 0;
io_error_exit:
    errno = EIO;
error_exit:
    eo = errno;
    if (ctx)
        fs_attr_put_search_ctx(ctx);
    if (vol->mftNa) {
        fs_attr_close(vol->mftNa);
        vol->mftNa = NULL;
    }
    if (vol->mftNi) {
        fs_inode_close(vol->mftNi);
        vol->mftNi = NULL;
    }
    errno = eo;
    return -1;
}

static int fs_mftmirr_load(FSVolume * vol)
{
    int err;

    vol->mftmirrNi = fs_inode_open(vol, FILE_MFTMirr);
    if (!vol->mftmirrNi) {
        C_LOG_ERROR("Failed to open inode $MFTMirr");
        return -1;
    }

    vol->mftmirrNa = fs_attr_open(vol->mftmirrNi, AT_DATA, AT_UNNAMED, 0);
    if (!vol->mftmirrNa) {
        C_LOG_ERROR("Failed to open $MFTMirr/$DATA");
        goto error_exit;
    }

    if (fs_attr_map_runlist(vol->mftmirrNa, 0) < 0) {
        C_LOG_ERROR("Failed to map runlist of $MFTMirr/$DATA");
        goto error_exit;
    }
    if (vol->mftmirrNa->rl->lcn != vol->mftmirrLcn) {
        C_LOG_ERROR("Bad $MFTMirr lcn 0x%llx, want 0x%llx", (long long)vol->mftmirrNa->rl->lcn, (long long)vol->mftmirrLcn);
        goto error_exit;
    }

    return 0;

error_exit:
    err = errno;
    if (vol->mftmirrNa) {
        fs_attr_close(vol->mftmirrNa);
        vol->mftmirrNa = NULL;
    }
    fs_inode_close(vol->mftmirrNi);
    vol->mftmirrNi = NULL;
    errno = err;
    return -1;
}


FSVolume* fs_volume_startup(FSDevice* dev, FSMountFlags flags)
{
    LCN mft_zone_size, mft_lcn;
    s64 br;
    FSVolume * vol;
    FS_BOOT_SECTOR * bs;
    int eo;

    if (!dev || !dev->dOps || !dev->dName) {
        errno = EINVAL;
        C_LOG_ERROR("%s: dev = %p", __FUNCTION__, dev);
        return NULL;
    }

    bs = c_malloc0(sizeof(FS_BOOT_SECTOR));
    if (!bs)
        return NULL;

    /* Allocate the volume structure. */
    vol = fs_volume_alloc();
    if (!vol)
        goto error_exit;

    /* Create the default upcase table. */
    vol->upcaseLen = fs_upcase_build_default(&vol->upcase);
    if (!vol->upcaseLen || !vol->upcase)
        goto error_exit;

    /* Default with no locase table and case sensitive file names */
    vol->locase = (FSChar*)NULL;
    NVolSetCaseSensitive(vol);

    /* by default, all files are shown and not marked hidden */
    NVolSetShowSysFiles(vol);
    NVolSetShowHidFiles(vol);
    NVolClearHideDotFiles(vol);
    /* set default compression */
#if DEFAULT_COMPRESSION
	NVolSetCompression(vol);
#else
    NVolClearCompression(vol);
#endif
    if (flags & FS_MNT_ReadOnly)
        NVolSetReadOnly(vol);

    /* ...->open needs bracketing to compile with glibc 2.7 */
    if ((dev->dOps->open)(dev, NVolReadOnly(vol) ? O_RDONLY : O_RDWR)) {
        if (!NVolReadOnly(vol) && (errno == EROFS)) {
            if ((dev->dOps->open)(dev, O_RDONLY)) {
                C_LOG_ERROR("Error opening read-only '%s'", dev->dName);
                goto error_exit;
            }
            else {
                C_LOG_INFO("Error opening '%s' read-write\n", dev->dName);
                NVolSetReadOnly(vol);
            }
        }
        else {
            C_LOG_ERROR("Error opening '%s'", dev->dName);
            goto error_exit;
        }
    }
    /* Attach the device to the volume. */
    vol->dev = dev;

    /* Now read the bootsector. */
    br = fs_pread(dev, 0, sizeof(FS_BOOT_SECTOR), bs);
    if (br != sizeof(FS_BOOT_SECTOR)) {
        if (br != -1)
            errno = EINVAL;
        if (!br) {
            C_LOG_ERROR("Failed to read bootsector (size=0)");
        }
        else {
            C_LOG_ERROR("Error reading bootsector");
        }
        goto error_exit;
    }
    if (!fs_boot_sector_is_ntfs(bs)) {
        errno = EINVAL;
        goto error_exit;
    }
    if (fs_boot_sector_parse(vol, bs) < 0)
        goto error_exit;

    free(bs);
    bs = NULL;
    /* Now set the device block size to the sector size. */
    if (fs_device_block_size_set(vol->dev, vol->sectorSize))
        C_LOG_DEBUG("Failed to set the device block size to the "
                       "sector size.  This may affect performance "
                       "but should be harmless otherwise.  Error: "
                       "%s", strerror(errno));

    /* We now initialize the cluster allocator. */
    vol->fullZones = 0;
    mft_zone_size = vol->nrClusters >> 3; /* 12.5% */

    /* Setup the mft zone. */
    vol->mftZoneStart = vol->mftZonePos = vol->mftLcn;
    C_LOG_DEBUG("mft_zone_pos = 0x%llx\n", (long long)vol->mftZonePos);

    /*
     * Calculate the mft_lcn for an unmodified NTFS volume (see mkntfs
     * source) and if the actual mft_lcn is in the expected place or even
     * further to the front of the volume, extend the mft_zone to cover the
     * beginning of the volume as well. This is in order to protect the
     * area reserved for the mft bitmap as well within the mft_zone itself.
     * On non-standard volumes we don't protect it as the overhead would be
     * higher than the speed increase we would get by doing it.
     */
    mft_lcn = (8192 + 2 * vol->clusterSize - 1) / vol->clusterSize;
    if (mft_lcn * vol->clusterSize < 16 * 1024)
        mft_lcn = (16 * 1024 + vol->clusterSize - 1) / vol->clusterSize;
    if (vol->mftZoneStart <= mft_lcn)
        vol->mftZoneStart = 0;
    C_LOG_DEBUG("mft_zone_start = 0x%llx\n", (long long)vol->mftZoneStart);

    /*
     * Need to cap the mft zone on non-standard volumes so that it does
     * not point outside the boundaries of the volume. We do this by
     * halving the zone size until we are inside the volume.
     */
    vol->mftZoneEnd = vol->mftLcn + mft_zone_size;
    while (vol->mftZoneEnd >= vol->nrClusters) {
        mft_zone_size >>= 1;
        if (!mft_zone_size) {
            errno = EINVAL;
            goto error_exit;
        }
        vol->mftZoneEnd = vol->mftLcn + mft_zone_size;
    }
    C_LOG_DEBUG("mft_zone_end = 0x%llx\n", (long long)vol->mftZoneEnd);

    /*
     * Set the current position within each data zone to the start of the
     * respective zone.
     */
    vol->data1ZonePos = vol->mftZoneEnd;
    C_LOG_DEBUG("data1_zone_pos = %lld\n", (long long)vol->data1ZonePos);
    vol->data2ZonePos = 0;
    C_LOG_DEBUG("data2_zone_pos = %lld\n", (long long)vol->data2ZonePos);

    /* Set the mft data allocation position to mft record 24. */
    vol->mftDataPos = 24;

    /*
     * The cluster allocator is now fully operational.
     */

    /* Need to setup $MFT so we can use the library read functions. */
    if (fs_mft_load(vol) < 0) {
        C_LOG_ERROR("Failed to load $MFT");
        goto error_exit;
    }

    /* Need to setup $MFTMirr so we can use the write functions, too. */
    if (fs_mftmirr_load(vol) < 0) {
        C_LOG_ERROR("Failed to load $MFTMirr");
        goto error_exit;
    }
    return vol;
error_exit:
    eo = errno;
    free(bs);
    if (vol)
        __fs_volume_release(vol);
    errno = eo;
    return NULL;
}


static int fs_volume_check_logfile(FSVolume * vol)
{
    FSInode * ni;
    FSAttr * na = NULL;
    RESTART_PAGE_HEADER * rp = NULL;
    int err = 0;

    ni = fs_inode_open(vol, FILE_LogFile);
    if (!ni) {
        C_LOG_ERROR("Failed to open inode FILE_LogFile");
        errno = EIO;
        return -1;
    }

    na = fs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
    if (!na) {
        C_LOG_ERROR("Failed to open $FILE_LogFile/$DATA");
        err = EIO;
        goto out;
    }

    if (!fs_check_logfile(na, &rp) || !fs_is_logfile_clean(na, rp))
        err = EOPNOTSUPP;
    /*
     * If the latest restart page was identified as version
     * 2.0, then Windows may have kept a cached copy of
     * metadata for fast restarting, and we should not mount.
     * Hibernation will be seen the same way on a non
     * Windows-system partition, so we have to use the same
     * error code (EPERM).
     * The restart page may also be identified as version 2.0
     * when access to the file system is terminated abruptly
     * by unplugging or power cut, so mounting is also rejected
     * after such an event.
     */
    if (rp
        && (rp->major_ver == const_cpu_to_le16(2))
        && (rp->minor_ver == const_cpu_to_le16(0))) {
        C_LOG_ERROR("Metadata kept in Windows cache, refused to mount.\n");
        err = EPERM;
    }
    free(rp);
    fs_attr_close(na);
out:
    if (fs_inode_close(ni))
        fs_error_set(&err);
    if (err) {
        errno = err;
        return -1;
    }
    return 0;
}


static FSInode* fs_hiberfile_open(FSVolume * vol)
{
    u64 inode;
    FSInode * ni_root;
    FSInode * ni_hibr = NULL;
    FSChar * unicode = NULL;
    int unicode_len;
    const char * hiberfile = "hiberfil.sys";

    if (!vol) {
        errno = EINVAL;
        return NULL;
    }

    ni_root = fs_inode_open(vol, FILE_root);
    if (!ni_root) {
        C_LOG_DEBUG("Couldn't open the root directory.\n");
        return NULL;
    }

    unicode_len = fs_mbstoucs(hiberfile, &unicode);
    if (unicode_len < 0) {
        C_LOG_ERROR("Couldn't convert 'hiberfil.sys' to Unicode");
        goto out;
    }

    inode = fs_inode_lookup_by_name(ni_root, unicode, unicode_len);
    if (inode == (u64) - 1) {
        C_LOG_DEBUG("Couldn't find file '%s'.\n", hiberfile);
        goto out;
    }

    inode = MREF(inode);
    ni_hibr = fs_inode_open(vol, inode);
    if (!ni_hibr) {
        C_LOG_DEBUG("Couldn't open inode %lld.\n", (long long)inode);
        goto out;
    }
out:
    if (fs_inode_close(ni_root)) {
        fs_inode_close(ni_hibr);
        ni_hibr = NULL;
    }
    free(unicode);
    return ni_hibr;
}


#define NTFS_HIBERFILE_HEADER_SIZE	4096


int fs_volume_check_hiberfile(FSVolume * vol, int verbose)
{
    FSInode * ni;
    FSAttr * na = NULL;
    int bytes_read, err;
    char * buf = NULL;

    ni = fs_hiberfile_open(vol);
    if (!ni) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    buf = c_malloc0(NTFS_HIBERFILE_HEADER_SIZE);
    if (!buf)
        goto out;

    na = fs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
    if (!na) {
        C_LOG_ERROR("Failed to open hiberfil.sys data attribute");
        goto out;
    }

    bytes_read = fs_attr_pread(na, 0, NTFS_HIBERFILE_HEADER_SIZE, buf);
    if (bytes_read == -1) {
        C_LOG_ERROR("Failed to read hiberfil.sys");
        goto out;
    }
    if (bytes_read < NTFS_HIBERFILE_HEADER_SIZE) {
        if (verbose)
            C_LOG_ERROR("Hibernated non-system partition, "
                "refused to mount.\n");
        errno = EPERM;
        goto out;
    }
    if ((memcmp(buf, "hibr", 4) == 0)
        || (memcmp(buf, "HIBR", 4) == 0)) {
        if (verbose)
            C_LOG_ERROR("Windows is hibernated, refused to mount.\n");
        errno = EPERM;
        goto out;
    }
    /* All right, all header bytes are zero */
    errno = 0;
out:
    if (na)
        fs_attr_close(na);
    free(buf);
    err = errno;
    if (fs_inode_close(ni))
        fs_error_set(&err);
    errno = err;
    return errno ? -1 : 0;
}


static int fix_txf_data(FSVolume * vol)
{
    void * txf_data;
    s64 txf_data_size;
    FSInode * ni;
    FSAttr * na;
    int res;

    res = 0;
    C_LOG_DEBUG("Loading root directory\n");
    ni = fs_inode_open(vol, FILE_root);
    if (!ni) {
        C_LOG_ERROR("Failed to open root directory");
        res = -1;
    }
    else {
        /* Get the $TXF_DATA attribute */
        na = fs_attr_open(ni, AT_LOGGED_UTILITY_STREAM, TXF_DATA, 9);
        if (na) {
            if (NAttrNonResident(na)) {
                /*
                 * Fix the attribute by truncating, then
                 * rewriting it.
                 */
                C_LOG_DEBUG("Making $TXF_DATA resident\n");
                txf_data = fs_attr_readall(ni,
                                             AT_LOGGED_UTILITY_STREAM,
                                             TXF_DATA, 9, &txf_data_size);
                if (txf_data) {
                    if (fs_attr_truncate(na, 0)
                        || (fs_attr_pwrite(na, 0,
                                             txf_data_size, txf_data)
                            != txf_data_size))
                        res = -1;
                    free(txf_data);
                }
                if (res) {
                    C_LOG_ERROR("Failed to make $TXF_DATA resident\n");
                }
                else {
                    C_LOG_ERROR("$TXF_DATA made resident\n");
                }
            }
            fs_attr_close(na);
        }
        if (fs_inode_close(ni)) {
            C_LOG_ERROR("Failed to close root");
            res = -1;
        }
    }
    return (res);
}


FSVolume* fs_device_mount(FSDevice* dev, FSMountFlags flags)
{
    cint64 l;
    FSVolume * vol;
    cuint8 * m = NULL, * m2 = NULL;
    FSAttrSearchCtx * ctx = NULL;
    FSInode * ni;
    FSAttr * na;
    ATTR_RECORD * a;
    VOLUME_INFORMATION * vinf;
    FSChar * vname;
    cuint32 record_size;
    int i, j, eo;
    unsigned int k;
    cuint32 u;
    bool need_fallback_ro;

    need_fallback_ro = false;
    vol = fs_volume_startup(dev, flags);
    if (!vol)
        return NULL;

    /* Load data from $MFT and $MFTMirr and compare the contents. */
    m = c_malloc0(vol->mftmirrSize << vol->mftRecordSizeBits);
    m2 = c_malloc0(vol->mftmirrSize << vol->mftRecordSizeBits);
    if (!m || !m2)
        goto error_exit;

    l = fs_attr_mst_pread(vol->mftNa, 0, vol->mftmirrSize, vol->mftRecordSize, m);
    if (l != vol->mftmirrSize) {
        if (l == -1) {
            C_LOG_ERROR("Failed to read $MFT");
        }
        else {
            C_LOG_ERROR("Failed to read $MFT, unexpected length (%lld != %d).", (long long)l, vol->mftmirrSize);
            errno = EIO;
        }
        goto error_exit;
    }
    for (i = 0; (i < l) && (i < FILE_first_user); ++i) {
        if (fs_mft_record_check(vol, FILE_MFT + i, (MFT_RECORD*)(m + i * vol->mftRecordSize))) {
            goto error_exit;
        }
    }
    l = fs_attr_mst_pread(vol->mftmirrNa, 0, vol->mftmirrSize, vol->mftRecordSize, m2);
    if (l != vol->mftmirrSize) {
        if (l == -1) {
            C_LOG_ERROR("Failed to read $MFTMirr");
            goto error_exit;
        }
        vol->mftmirrSize = l;
    }
    for (i = 0; (i < l) && (i < FILE_first_user); ++i)
        if (fs_mft_record_check(vol, FILE_MFT + i, (MFT_RECORD*)(m2 + i * vol->mftRecordSize)))
            goto error_exit;
    C_LOG_DEBUG("Comparing $MFTMirr to $MFT...\n");
    /* Windows 10 does not update the full $MFTMirr any more */
    for (i = 0; (i < vol->mftmirrSize) && (i < FILE_first_user); ++i) {
        MFT_RECORD * mrec, * mrec2;
        const char * ESTR[12] = {
            "$MFT", "$MFTMirr", "$LogFile",
            "$Volume", "$AttrDef", "root directory", "$Bitmap",
            "$Boot", "$BadClus", "$Secure", "$UpCase", "$Extend"
        };
        const char * s;

        if (i < 12)
            s = ESTR[i];
        else if (i < 16)
            s = "system file";
        else
            s = "mft record";

        mrec = (MFT_RECORD*)(m + i * vol->mftRecordSize);
        if (mrec->flags & MFT_RECORD_IN_USE) {
            if (fs_is_baad_record(mrec->magic)) {
                C_LOG_ERROR("$MFT error: Incomplete multi "
                               "sector transfer detected in "
                               "'%s'.\n", s);
                goto io_error_exit;
            }
            if (!fs_is_mft_record(mrec->magic)) {
                C_LOG_ERROR("$MFT error: Invalid mft "
                               "record for '%s'.\n", s);
                goto io_error_exit;
            }
        }
        mrec2 = (MFT_RECORD*)(m2 + i * vol->mftRecordSize);
        if (mrec2->flags & MFT_RECORD_IN_USE) {
            if (fs_is_baad_record(mrec2->magic)) {
                C_LOG_ERROR("$MFTMirr error: Incomplete "
                               "multi sector transfer "
                               "detected in '%s'.\n", s);
                goto io_error_exit;
            }
            if (!fs_is_mft_record(mrec2->magic)) {
                C_LOG_ERROR("$MFTMirr error: Invalid mft "
                               "record for '%s'.\n", s);
                goto io_error_exit;
            }
        }
        record_size = fs_mft_record_get_data_size(mrec);
        if ((record_size <= sizeof(MFT_RECORD))
            || (record_size > vol->mftRecordSize)
            || memcmp(mrec, mrec2, record_size)) {
            C_LOG_ERROR("$MFTMirr does not match $MFT (record %d).", i);
            goto io_error_exit;
        }
    }

    free(m2);
    free(m);
    m = m2 = NULL;

    /* Now load the bitmap from $Bitmap. */
    C_LOG_DEBUG("Loading $Bitmap...\n");
    vol->lcnbmpNi = fs_inode_open(vol, FILE_Bitmap);
    if (!vol->lcnbmpNi) {
        C_LOG_ERROR("Failed to open inode FILE_Bitmap");
        goto error_exit;
    }

    vol->lcnbmpNa = fs_attr_open(vol->lcnbmpNi, AT_DATA, AT_UNNAMED, 0);
    if (!vol->lcnbmpNa) {
        C_LOG_ERROR("Failed to open ntfs attribute");
        goto error_exit;
    }

    if (vol->lcnbmpNa->dataSize > vol->lcnbmpNa->allocatedSize) {
        C_LOG_ERROR("Corrupt cluster map size (%lld > %lld)\n",
                       (long long)vol->lcnbmpNa->dataSize,
                       (long long)vol->lcnbmpNa->allocatedSize);
        goto io_error_exit;
    }

    /* Now load the upcase table from $UpCase. */
    C_LOG_DEBUG("Loading $UpCase...\n");
    ni = fs_inode_open(vol, FILE_UpCase);
    if (!ni) {
        C_LOG_ERROR("Failed to open inode FILE_UpCase");
        goto error_exit;
    }
    /* Get an ntfs attribute for $UpCase/$DATA. */
    na = fs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
    if (!na) {
        C_LOG_ERROR("Failed to open ntfs attribute");
        fs_inode_close(ni);
        goto error_exit;
    }
    /*
     * Note: Normally, the upcase table has a length equal to 65536
     * 2-byte Unicode characters. Anyway we currently can only process
     * such characters.
     */
    if ((na->dataSize - 2) & ~0x1fffeULL) {
        C_LOG_ERROR("Error: Upcase table is invalid (want size even "
            "<= 131072).\n");
        errno = EINVAL;
        goto bad_upcase;
    }
    if (vol->upcaseLen != na->dataSize >> 1) {
        vol->upcaseLen = na->dataSize >> 1;
        /* Throw away default table. */
        free(vol->upcase);
        vol->upcase = c_malloc0(na->dataSize);
        if (!vol->upcase)
            goto bad_upcase;
    }
    /* Read in the $DATA attribute value into the buffer. */
    l = fs_attr_pread(na, 0, na->dataSize, vol->upcase);
    if (l != na->dataSize) {
        C_LOG_ERROR("Failed to read $UpCase, unexpected length (%lld != %lld).\n", (long long)l, (long long)na->dataSize);
        errno = EIO;
        goto bad_upcase;
    }
    /* Done with the $UpCase mft record. */
    fs_attr_close(na);
    if (fs_inode_close(ni)) {
        C_LOG_ERROR("Failed to close $UpCase");
        goto error_exit;
    }
    /* Consistency check of $UpCase, restricted to plain ASCII chars */
    k = 0x20;
    while ((k < vol->upcaseLen)
        && (k < 0x7f)
        && (le16_to_cpu(vol->upcase[k])
            == ((k < 'a') || (k > 'z') ? k : k + 'A' - 'a')))
        k++;
    if (k < 0x7f) {
        C_LOG_ERROR("Corrupted file $UpCase\n");
        goto io_error_exit;
    }

    /*
     * Now load $Volume and set the version information and flags in the
     * vol structure accordingly.
     */
    C_LOG_DEBUG("Loading $Volume...\n");
    vol->volNi = fs_inode_open(vol, FILE_Volume);
    if (!vol->volNi) {
        C_LOG_ERROR("Failed to open inode FILE_Volume");
        goto error_exit;
    }
    /* Get a search context for the $Volume/$VOLUME_INFORMATION lookup. */
    ctx = fs_attr_get_search_ctx(vol->volNi, NULL);
    if (!ctx)
        goto error_exit;

    /* Find the $VOLUME_INFORMATION attribute. */
    if (fs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, 0, NULL,
                         0, ctx)) {
        C_LOG_ERROR("$VOLUME_INFORMATION attribute not found in "
            "$Volume");
        goto error_exit;
    }
    a = ctx->attr;
    /* Has to be resident. */
    if (a->non_resident) {
        C_LOG_ERROR("Attribute $VOLUME_INFORMATION must be "
            "resident but it isn't.\n");
        errno = EIO;
        goto error_exit;
    }
    /* Get a pointer to the value of the attribute. */
    vinf = (VOLUME_INFORMATION*)(le16_to_cpu(a->value_offset) + (char*)a);
    /* Sanity checks. */
    if ((char*)vinf + le32_to_cpu(a->value_length) > (char*)ctx->mrec +
        le32_to_cpu(ctx->mrec->bytes_in_use) ||
        le16_to_cpu(a->value_offset) + le32_to_cpu(
            a->value_length) > le32_to_cpu(a->length)) {
        C_LOG_ERROR("$VOLUME_INFORMATION in $Volume is corrupt.\n");
        errno = EIO;
        goto error_exit;
    }
    /* Setup vol from the volume information attribute value. */
    vol->majorVer = vinf->majorVer;
    vol->minorVer = vinf->minorVer;
    /* Do not use le16_to_cpu() macro here as our VOLUME_FLAGS are
       defined using cpu_to_le16() macro and hence are consistent. */
    vol->flags = vinf->flags;
    /*
     * Reinitialize the search context for the $Volume/$VOLUME_NAME lookup.
     */
    fs_attr_reinit_search_ctx(ctx);
    if (fs_attr_lookup(AT_VOLUME_NAME, AT_UNNAMED, 0, 0, 0, NULL, 0, ctx)) {
        if (errno != ENOENT) {
            C_LOG_ERROR("Failed to lookup of $VOLUME_NAME in "
                "$Volume failed");
            goto error_exit;
        }
        /*
         * Attribute not present.  This has been seen in the field.
         * Treat this the same way as if the attribute was present but
         * had zero length.
         */
        vol->volName = c_malloc0(1);
        if (!vol->volName)
            goto error_exit;
        vol->volName[0] = '\0';
    }
    else {
        a = ctx->attr;
        /* Has to be resident. */
        if (a->non_resident) {
            C_LOG_ERROR("$VOLUME_NAME must be resident.\n");
            errno = EIO;
            goto error_exit;
        }
        /* Get a pointer to the value of the attribute. */
        vname = (FSChar*)(le16_to_cpu(a->value_offset) + (char*)a);
        u = le32_to_cpu(a->value_length) / 2;
        /*
         * Convert Unicode volume name to current locale multibyte
         * format.
         */
        vol->volName = NULL;
        if (fs_ucstombs(vname, u, &vol->volName, 0) == -1) {
            C_LOG_ERROR("Volume name could not be converted "
                "to current locale");
            C_LOG_DEBUG("Forcing name into ASCII by replacing "
                "non-ASCII characters with underscores.\n");
            vol->volName = c_malloc0(u + 1);
            if (!vol->volName)
                goto error_exit;

            for (j = 0; j < (s32)u; j++) {
                u16 uc = le16_to_cpu(vname[j]);
                if (uc > 0xff)
                    uc = (u16)'_';
                vol->volName[j] = (char)uc;
            }
            vol->volName[u] = '\0';
        }
    }
    fs_attr_put_search_ctx(ctx);
    ctx = NULL;
    /* Now load the attribute definitions from $AttrDef. */
    C_LOG_DEBUG("Loading $AttrDef...\n");
    ni = fs_inode_open(vol, FILE_AttrDef);
    if (!ni) {
        C_LOG_ERROR("Failed to open $AttrDef");
        goto error_exit;
    }
    /* Get an ntfs attribute for $AttrDef/$DATA. */
    na = fs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
    if (!na) {
        C_LOG_ERROR("Failed to open ntfs attribute");
        goto error_exit;
    }
    /* Check we don't overflow 24-bits. */
    if ((u64)na->dataSize > 0xffffffLL) {
        C_LOG_ERROR("Attribute definition table is too big (max "
            "24-bit allowed).\n");
        errno = EINVAL;
        goto error_exit;
    }
    vol->attrdefLen = na->dataSize;
    vol->attrdef = c_malloc0(na->dataSize);
    if (!vol->attrdef)
        goto error_exit;
    /* Read in the $DATA attribute value into the buffer. */
    l = fs_attr_pread(na, 0, na->dataSize, vol->attrdef);
    if (l != na->dataSize) {
        C_LOG_ERROR("Failed to read $AttrDef, unexpected length (%lld != %lld).", (long long)l, (long long)na->dataSize);
        errno = EIO;
        goto error_exit;
    }
    /* Done with the $AttrDef mft record. */
    fs_attr_close(na);
    if (fs_inode_close(ni)) {
        C_LOG_ERROR("Failed to close $AttrDef");
        goto error_exit;
    }

    /* Open $Secure. */
    if (fs_open_secure(vol))
        goto error_exit;

    /*
     * Check for dirty logfile and hibernated Windows.
     * We care only about read-write mounts.
     */
    if (!(flags & (FS_MNT_ReadOnly | FS_MNT_FORENSIC))) {
        if (!(flags & FS_MNT_IGNORE_HIBERFILE) &&
            fs_volume_check_hiberfile(vol, 1) < 0) {
            if (flags & FS_MNT_MayReadOnly)
                need_fallback_ro = true;
            else
                goto error_exit;
        }
        if (fs_volume_check_logfile(vol) < 0) {
            /* Always reject cached metadata for now */
            if (!(flags & FS_MNT_RECOVER) || (errno == EPERM)) {
                if (flags & FS_MNT_MayReadOnly)
                    need_fallback_ro = true;
                else
                    goto error_exit;
            }
            else {
                C_LOG_INFO("The file system wasn't safely closed on Windows. Fixing.");
                if (fs_logfile_reset(vol))
                    goto error_exit;
            }
        }
        /* make $TXF_DATA resident if present on the root directory */
        if (!(flags & FS_MNT_ReadOnly) && !need_fallback_ro) {
            if (fix_txf_data(vol))
                goto error_exit;
        }
    }
    if (need_fallback_ro) {
        NVolSetReadOnly(vol);
        C_LOG_ERROR("%s", fallback_readonly_msg);
    }

    return vol;
bad_upcase :
    fs_attr_close(na);
    fs_inode_close(ni);
    goto error_exit;
io_error_exit:
    errno = EIO;
error_exit:
    eo = errno;
    if (ctx)
        fs_attr_put_search_ctx(ctx);
    free(m);
    free(m2);
    __fs_volume_release(vol);
    errno = eo;
    return NULL;
}

int fs_set_shown_files(FSVolume * vol, bool show_sys_files, bool show_hid_files, bool hide_dot_files)
{
    int res;

    res = -1;
    if (vol) {
        NVolClearShowSysFiles(vol);
        NVolClearShowHidFiles(vol);
        NVolClearHideDotFiles(vol);
        if (show_sys_files)
            NVolSetShowSysFiles(vol);
        if (show_hid_files)
            NVolSetShowHidFiles(vol);
        if (hide_dot_files)
            NVolSetHideDotFiles(vol);
        res = 0;
    }
    if (res)
        C_LOG_ERROR("Failed to set file visibility\n");
    return (res);
}

/*
 *		Set ignore case mode
 */

int fs_set_ignore_case(FSVolume * vol)
{
    int res;

    res = -1;
    if (vol && vol->upcase) {
        vol->locase = fs_locase_table_build(vol->upcase, vol->upcaseLen);
        if (vol->locase) {
            NVolClearCaseSensitive(vol);
            res = 0;
        }
    }
    if (res)
        C_LOG_ERROR("Failed to set ignore_case mode\n");
    return (res);
}

FSVolume* fs_mount(const char * name __attribute__((unused)), FSMountFlags flags __attribute__((unused)))
{
#ifndef NO_NTFS_DEVICE_DEFAULT_IO_OPS
    FSDevice* dev;
    FSVolume * vol;

    /* Allocate an fs_device structure. */
    dev = fs_device_alloc(name, 0, &FSDeviceDefaultIoOps, NULL);
    if (!dev)
        return NULL;
    /* Call fs_device_mount() to do the actual mount. */
    vol = fs_device_mount(dev, flags);
    if (!vol) {
        int eo = errno;
        fs_device_free(dev);
        errno = eo;
    }
    else
        fs_create_lru_caches(vol);
    return vol;
#else
	/*
	 * fs_mount() makes no sense if NO_NTFS_DEVICE_DEFAULT_IO_OPS is
	 * defined as there are no device operations available in libntfs in
	 * this case.
	 */
	errno = EOPNOTSUPP;
	return NULL;
#endif
}


int fs_umount(FSVolume * vol, const bool force __attribute__((unused)))
{
    FSDevice* dev;
    int ret;

    if (!vol) {
        errno = EINVAL;
        return -1;
    }
    dev = vol->dev;
    ret = __fs_volume_release(vol);
    fs_device_free(dev);
    return ret;
}

#ifdef HAVE_MNTENT_H

static int fs_mntent_check(const char *file, unsigned long *mnt_flags)
{
	struct mntent *mnt;
	char *real_file = NULL, *real_fsname = NULL;
	FILE *f;
	int err = 0;

	real_file = c_malloc0(PATH_MAX + 1);
	if (!real_file)
		return -1;
	real_fsname = c_malloc0(PATH_MAX + 1);
	if (!real_fsname) {
		err = errno;
		goto exit;
	}
	if (!fs_realpath_canonicalize(file, real_file)) {
		err = errno;
		goto exit;
	}
	f = setmntent("/proc/mounts", "r");
	if (!f && !(f = setmntent(MOUNTED, "r"))) {
		err = errno;
		goto exit;
	}
	while ((mnt = getmntent(f))) {
		if (!fs_realpath_canonicalize(mnt->mnt_fsname, real_fsname))
			continue;
		if (!strcmp(real_file, real_fsname))
			break;
	}
	endmntent(f);
	if (!mnt)
		goto exit;
	*mnt_flags = NTFS_MF_MOUNTED;
	if (!strcmp(mnt->mnt_dir, "/"))
		*mnt_flags |= NTFS_MF_ISROOT;
#ifdef HAVE_HASMNTOPT
	if (hasmntopt(mnt, "ro") && !hasmntopt(mnt, "rw"))
		*mnt_flags |= NTFS_MF_READONLY;
#endif
exit:
	free(real_file);
	free(real_fsname);
	if (err) {
		errno = err;
		return -1;
	}
	return 0;
}

#else /* HAVE_MNTENT_H */

#if defined(__sun) && defined (__SVR4)

static int fs_mntent_check(const char *file, unsigned long *mnt_flags)
{
	struct mnttab *mnt = NULL;
	char *real_file = NULL, *real_fsname = NULL;
	FILE *f;
	int err = 0;

	real_file = (char*)c_malloc0(PATH_MAX + 1);
	if (!real_file)
		return -1;
	real_fsname = (char*)c_malloc0(PATH_MAX + 1);
	mnt = (struct mnttab*)c_malloc0(MNT_LINE_MAX + 1);
	if (!real_fsname || !mnt) {
		err = errno;
		goto exit;
	}
	if (!fs_realpath_canonicalize(file, real_file)) {
		err = errno;
		goto exit;
	}
	if (!(f = fopen(MNTTAB, "r"))) {
		err = errno;
		goto exit;
	}
	while (!getmntent(f, mnt)) {
		if (!fs_realpath_canonicalize(mnt->mnt_special, real_fsname))
			continue;
		if (!strcmp(real_file, real_fsname)) {
			*mnt_flags = NTFS_MF_MOUNTED;
			if (!strcmp(mnt->mnt_mountp, "/"))
				*mnt_flags |= NTFS_MF_ISROOT;
			if (hasmntopt(mnt, "ro") && !hasmntopt(mnt, "rw"))
				*mnt_flags |= NTFS_MF_READONLY;
			break;
		}
	}
	fclose(f);
exit:
	free(mnt);
	free(real_file);
	free(real_fsname);
	if (err) {
		errno = err;
		return -1;
	}
	return 0;
}

#endif /* defined(__sun) && defined (__SVR4) */
#endif /* HAVE_MNTENT_H */


int fs_check_if_mounted(const char * file __attribute__((unused)), unsigned long * mnt_flags)
{
    *mnt_flags = 0;
#if defined(HAVE_MNTENT_H) || (defined(__sun) && defined (__SVR4))
	return fs_mntent_check(file, mnt_flags);
#else
    return 0;
#endif
}


int fs_version_is_supported(FSVolume * vol)
{
    cuint8 major, minor;

    if (!vol) {
        errno = EINVAL;
        return -1;
    }

    major = vol->majorVer;
    minor = vol->minorVer;

    if (NTFS_V1_1(major, minor) || NTFS_V1_2(major, minor))
        return 0;

    if (NTFS_V2_X(major, minor))
        return 0;

    if (NTFS_V3_0(major, minor) || NTFS_V3_1(major, minor))
        return 0;

    errno = EOPNOTSUPP;
    return -1;
}


int fs_logfile_reset(FSVolume * vol)
{
    FSInode * ni;
    FSAttr * na;
    int eo;

    if (!vol) {
        errno = EINVAL;
        return -1;
    }

    ni = fs_inode_open(vol, FILE_LogFile);
    if (!ni) {
        C_LOG_ERROR("Failed to open inode FILE_LogFile");
        return -1;
    }

    na = fs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
    if (!na) {
        eo = errno;
        C_LOG_ERROR("Failed to open $FILE_LogFile/$DATA");
        goto error_exit;
    }

    if (fs_empty_logfile(na)) {
        eo = errno;
        fs_attr_close(na);
        goto error_exit;
    }

    fs_attr_close(na);
    return fs_inode_close(ni);

error_exit:
    fs_inode_close(ni);
    errno = eo;
    return -1;
}


int fs_volume_write_flags(FSVolume * vol, const cuint16 flags)
{
    ATTR_RECORD * a;
    VOLUME_INFORMATION * c;
    FSAttrSearchCtx * ctx;
    int ret = -1; /* failure */

    if (!vol || !vol->volNi) {
        errno = EINVAL;
        return -1;
    }
    /* Get a pointer to the volume information attribute. */
    ctx = fs_attr_get_search_ctx(vol->volNi, NULL);
    if (!ctx)
        return -1;

    if (fs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, 0, NULL, 0, ctx)) {
        C_LOG_ERROR("Attribute $VOLUME_INFORMATION was not found in $Volume!");
        goto err_out;
    }
    a = ctx->attr;
    /* Sanity check. */
    if (a->non_resident) {
        C_LOG_ERROR("Attribute $VOLUME_INFORMATION must be resident but it isn't.");
        errno = EIO;
        goto err_out;
    }
    /* Get a pointer to the value of the attribute. */
    c = (VOLUME_INFORMATION*)(le16_to_cpu(a->value_offset) + (char*)a);
    /* Sanity checks. */
    if ((char*)c + le32_to_cpu(a->value_length) > (char*)ctx->mrec +
        le32_to_cpu(ctx->mrec->bytes_in_use) ||
        le16_to_cpu(a->value_offset) +
        le32_to_cpu(a->value_length) > le32_to_cpu(a->length)) {
        C_LOG_ERROR("Attribute $VOLUME_INFORMATION in $Volume is "
            "corrupt!\n");
        errno = EIO;
        goto err_out;
    }
    /* Set the volume flags. */
    vol->flags = c->flags = flags & VOLUME_FLAGS_MASK;
    /* Write them to disk. */
    fs_inode_mark_dirty(vol->volNi);
    if (fs_inode_sync(vol->volNi))
        goto err_out;

    ret = 0; /* success */
err_out:
    fs_attr_put_search_ctx(ctx);
    return ret;
}

int fs_volume_error(int err)
{
    int ret;

    switch (err) {
    case 0:
        ret = FS_VOLUME_OK;
        break;
    case EINVAL:
        ret = FS_VOLUME_NOT_NTFS;
        break;
    case EIO:
        ret = FS_VOLUME_CORRUPT;
        break;
    case EPERM:
        /*
         * Hibernation and fast restarting are seen the
         * same way on a non Windows-system partition.
         */
        ret = FS_VOLUME_HIBERNATED;
        break;
    case EOPNOTSUPP:
        ret = FS_VOLUME_UNCLEAN_UNMOUNT;
        break;
    case EBUSY:
        ret = FS_VOLUME_LOCKED;
        break;
    case ENXIO:
        ret = FS_VOLUME_RAID;
        break;
    case EACCES:
        ret = FS_VOLUME_NO_PRIVILEGE;
        break;
    default:
        ret = FS_VOLUME_UNKNOWN_REASON;
        break;
    }
    return ret;
}


void fs_mount_error(const char * volume, const char * mntpoint, int err)
{
    switch (err) {
    case FS_VOLUME_NOT_NTFS:
        C_LOG_ERROR(invalid_ntfs_msg, volume);
        break;
    case FS_VOLUME_CORRUPT:
        C_LOG_ERROR("%s", corrupt_volume_msg);
        break;
    case FS_VOLUME_HIBERNATED:
        C_LOG_ERROR(hibernated_volume_msg, volume, mntpoint);
        break;
    case FS_VOLUME_UNCLEAN_UNMOUNT:
        C_LOG_ERROR("%s", unclean_journal_msg);
        break;
    case FS_VOLUME_LOCKED:
        C_LOG_ERROR("%s", opened_volume_msg);
        break;
    case FS_VOLUME_RAID:
        C_LOG_ERROR("%s", fakeraid_msg);
        break;
    case FS_VOLUME_NO_PRIVILEGE:
        C_LOG_ERROR(access_denied_msg, volume);
        break;
    }
}

int fs_set_locale(void)
{
    const char * locale;

    locale = setlocale(LC_ALL, "");
    if (!locale) {
        locale = setlocale(LC_ALL, NULL);
        C_LOG_ERROR("Couldn't set local environment, using default '%s'.", locale);
        return 1;
    }
    return 0;
}

/*
 *		Feed the counts of free clusters and free mft records
 */

int fs_volume_get_free_space(FSVolume * vol)
{
    FSAttr * na;
    int ret;

    ret = -1; /* default return */
    vol->freeClusters = fs_attr_get_free_bits(vol->lcnbmpNa);
    if (vol->freeClusters < 0) {
        C_LOG_ERROR("Failed to read NTFS $Bitmap");
    }
    else {
        na = vol->mftbmpNa;
        vol->freeMftRecords = fs_attr_get_free_bits(na);

        if (vol->freeMftRecords >= 0)
            vol->freeMftRecords += (na->allocatedSize - na->dataSize) << 3;

        if (vol->freeMftRecords < 0) {
            C_LOG_ERROR("Failed to calculate free MFT records");
        }
        else {
            NVolSetFreeSpaceKnown(vol);
            ret = 0;
        }
    }
    return (ret);
}


int fs_volume_rename(FSVolume * vol, const FSChar * label, int label_len)
{
    FSAttr * na;
    char * old_vol_name;
    char * new_vol_name = NULL;
    int new_vol_name_len;
    int err;

    if (NVolReadOnly(vol)) {
        C_LOG_ERROR("Refusing to change label on read-only mounted volume.");
        errno = EROFS;
        return -1;
    }

    label_len *= sizeof(FSChar);
    if (label_len > 0x100) {
        C_LOG_ERROR("New label is too long. Maximum %u characters allowed.", (unsigned)(0x100 / sizeof(FSChar)));
        errno = ERANGE;
        return -1;
    }

    na = fs_attr_open(vol->volNi, AT_VOLUME_NAME, AT_UNNAMED, 0);
    if (!na) {
        if (errno != ENOENT) {
            err = errno;
            C_LOG_ERROR("Lookup of $VOLUME_NAME attribute "
                "failed");
            goto err_out;
        }

        /* The volume name attribute does not exist.  Need to add it. */
        if (fs_attr_add(vol->volNi, AT_VOLUME_NAME, AT_UNNAMED, 0, (const u8*)label, label_len)) {
            err = errno;
            C_LOG_ERROR("Encountered error while adding $VOLUME_NAME attribute");
            goto err_out;
        }
    }
    else {
        s64 written;

        if (NAttrNonResident(na)) {
            err = errno;
            C_LOG_ERROR("Error: Attribute $VOLUME_NAME must be "
                "resident.\n");
            goto err_out;
        }

        if (na->dataSize != label_len) {
            if (fs_attr_truncate(na, label_len)) {
                err = errno;
                C_LOG_ERROR("Error resizing resident "
                    "attribute");
                goto err_out;
            }
        }

        if (label_len) {
            written = fs_attr_pwrite(na, 0, label_len, label);
            if (written == -1) {
                err = errno;
                C_LOG_ERROR("Error when writing "
                    "$VOLUME_NAME data");
                goto err_out;
            }
            else if (written != label_len) {
                err = EIO;
                C_LOG_ERROR("Partial write when writing "
                    "$VOLUME_NAME data.");
                goto err_out;
            }
        }
    }

    new_vol_name_len = fs_ucstombs(label, label_len, &new_vol_name, 0);
    if (new_vol_name_len == -1) {
        err = errno;
        C_LOG_ERROR("Error while decoding new volume name");
        goto err_out;
    }

    old_vol_name = vol->volName;
    vol->volName = new_vol_name;
    free(old_vol_name);

    err = 0;
err_out:
    if (na)
        fs_attr_close(na);
    if (err)
        errno = err;
    return err ? -1 : 0;
}
