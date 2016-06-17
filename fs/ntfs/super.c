/*
 * super.c
 *
 * Copyright (C) 1995-1997, 1999 Martin von Löwis
 * Copyright (C) 1996-1997 Régis Duchesne
 * Copyright (C) 1999 Steve Dodd
 * Copyright (C) 2000-2001 Anton Altparmakov (AIA)
 */

#include <linux/ntfs_fs.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include "ntfstypes.h"
#include "struct.h"
#include "super.h"
#include "macros.h"
#include "inode.h"
#include "support.h"
#include "util.h"
#include <linux/smp_lock.h>

/* All important structures in NTFS use 2 consistency checks:
 * . a magic structure identifier (FILE, INDX, RSTR, RCRD...)
 * . a fixup technique : the last word of each sector (called a fixup) of a
 *   structure's record should end with the word at offset <n> of the first
 *   sector, and if it is the case, must be replaced with the words following
 *   <n>. The value of <n> and the number of fixups is taken from the fields
 *   at the offsets 4 and 6. Note that the sector size is defined as
 *   NTFS_SECTOR_SIZE and not as the hardware sector size (this is concordant
 *   with what the Windows NTFS driver does).
 *
 * This function performs these 2 checks, and _fails_ if:
 * . the input size is invalid
 * . the fixup header is invalid
 * . the size does not match the number of sectors
 * . the magic identifier is wrong
 * . a fixup is invalid
 */
int ntfs_fixup_record(char *record, char *magic, int size)
{
	int start, count, offset;
	ntfs_u16 fixup;

	if (!IS_MAGIC(record, magic))
		return 0;
	start = NTFS_GETU16(record + 4);
	count = NTFS_GETU16(record + 6) - 1;
	if (size & (NTFS_SECTOR_SIZE - 1) || start & 1 ||
			start + count * 2 > size || size >> 9 != count) {
		if (size <= 0)
			printk(KERN_ERR "NTFS: BUG: ntfs_fixup_record() got "
					"zero size! Please report this to "
					"linux-ntfs-dev@lists.sf.net\n");
		return 0;
	}
	fixup = NTFS_GETU16(record + start);
	start += 2;
	offset = NTFS_SECTOR_SIZE - 2;
	while (count--) {
		if (NTFS_GETU16(record + offset) != fixup)
			return 0;
		NTFS_PUTU16(record + offset, NTFS_GETU16(record + start));
		start += 2;
		offset += NTFS_SECTOR_SIZE;
	}
	return 1;
}

/*
 * Get vital informations about the ntfs partition from the boot sector.
 * Return 0 on success or -1 on error.
 */
int ntfs_init_volume(ntfs_volume *vol, char *boot)
{
	int sectors_per_cluster_bits;
	__s64 ll;
	ntfs_cluster_t mft_zone_size, tc;

	/* System defined default values, in case we don't load $AttrDef. */
	vol->at_standard_information = 0x10;
	vol->at_attribute_list = 0x20;
	vol->at_file_name = 0x30;
	vol->at_volume_version = 0x40;
	vol->at_security_descriptor = 0x50;
	vol->at_volume_name = 0x60;
	vol->at_volume_information = 0x70;
	vol->at_data = 0x80;
	vol->at_index_root = 0x90;
	vol->at_index_allocation = 0xA0;
	vol->at_bitmap = 0xB0;
	vol->at_symlink = 0xC0;
	/* Sector size. */
	vol->sector_size = NTFS_GETU16(boot + 0xB);
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->sector_size = 0x%x\n",
				vol->sector_size);
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: sectors_per_cluster = "
				"0x%x\n", NTFS_GETU8(boot + 0xD));
	sectors_per_cluster_bits = ffs(NTFS_GETU8(boot + 0xD)) - 1;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: sectors_per_cluster_bits "
				"= 0x%x\n", sectors_per_cluster_bits); 
	vol->mft_clusters_per_record = NTFS_GETS8(boot + 0x40);
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->mft_clusters_per_record"
				" = 0x%x\n", vol->mft_clusters_per_record); 
	vol->index_clusters_per_record = NTFS_GETS8(boot + 0x44);
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: "
				"vol->index_clusters_per_record = 0x%x\n",
				vol->index_clusters_per_record); 
	vol->cluster_size = vol->sector_size << sectors_per_cluster_bits;
	vol->cluster_size_bits = ffs(vol->cluster_size) - 1;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->cluster_size = 0x%x\n",
				vol->cluster_size); 
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->cluster_size_bits = "
				"0x%x\n", vol->cluster_size_bits); 
	if (vol->mft_clusters_per_record > 0)
		vol->mft_record_size = vol->cluster_size <<
				(ffs(vol->mft_clusters_per_record) - 1);
	else
		/*
		 * When mft_record_size < cluster_size, mft_clusters_per_record
		 * = -log2(mft_record_size) bytes. mft_record_size normaly is
		 * 1024 bytes, which is encoded as 0xF6 (-10 in decimal).
		 */
		vol->mft_record_size = 1 << -vol->mft_clusters_per_record;
	vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->mft_record_size = 0x%x"
				"\n", vol->mft_record_size); 
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->mft_record_size_bits = "
				"0x%x\n", vol->mft_record_size_bits); 
	if (vol->index_clusters_per_record > 0)
		vol->index_record_size = vol->cluster_size <<
				(ffs(vol->index_clusters_per_record) - 1);
	else
		/*
		 * When index_record_size < cluster_size,
		 * index_clusters_per_record = -log2(index_record_size) bytes.
		 * index_record_size normaly equals 4096 bytes, which is
		 * encoded as 0xF4 (-12 in decimal).
		 */
		vol->index_record_size = 1 << -vol->index_clusters_per_record;
	vol->index_record_size_bits = ffs(vol->index_record_size) - 1;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->index_record_size = "
				"0x%x\n", vol->index_record_size);
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->index_record_size_bits "
				"= 0x%x\n", vol->index_record_size_bits);
	/*
	 * Get the size of the volume in clusters (ofs 0x28 is nr_sectors) and
	 * check for 64-bit-ness. Windows currently only uses 32 bits to save
	 * the clusters so we do the same as it is much faster on 32-bit CPUs.
	 */
	ll = NTFS_GETS64(boot + 0x28) >> sectors_per_cluster_bits;
	if (ll >= (__s64)1 << 31) {
		ntfs_error("Cannot handle 64-bit clusters. Please inform "
				"linux-ntfs-dev@lists.sf.net that you got this "
				"error.\n");
		return -1;
	}
	vol->nr_clusters = (ntfs_cluster_t)ll;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->nr_clusters = 0x%x\n",
			vol->nr_clusters);
	vol->mft_lcn = (ntfs_cluster_t)NTFS_GETS64(boot + 0x30);
	vol->mft_mirr_lcn = (ntfs_cluster_t)NTFS_GETS64(boot + 0x38);
	/* Determine MFT zone size. */
	mft_zone_size = vol->nr_clusters;
	switch (vol->mft_zone_multiplier) {  /* % of volume size in clusters */
	case 4:
		mft_zone_size >>= 1;			/* 50%   */
		break;
	case 3:
		mft_zone_size = mft_zone_size * 3 >> 3;	/* 37.5% */
		break;
	case 2:
		mft_zone_size >>= 2;			/* 25%   */
		break;
	/* case 1: */
	default:
		mft_zone_size >>= 3;			/* 12.5% */
		break;
	}
	/* Setup mft zone. */
	vol->mft_zone_start = vol->mft_zone_pos = vol->mft_lcn;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->mft_zone_pos = %x\n",
			vol->mft_zone_pos);
	/*
	 * Calculate the mft_lcn for an unmodified NTFS volume (see mkntfs
	 * source) and if the actual mft_lcn is in the expected place or even
	 * further to the front of the volume, extend the mft_zone to cover the
	 * beginning of the volume as well. This is in order to protect the
	 * area reserved for the mft bitmap as well within the mft_zone itself.
	 * On non-standard volumes we don't protect it as well as the overhead
	 * would be higher than the speed increase we would get by doing it.
	 */
	tc = (8192 + 2 * vol->cluster_size - 1) / vol->cluster_size;
	if (tc * vol->cluster_size < 16 * 1024)
		tc = (16 * 1024 + vol->cluster_size - 1) / vol->cluster_size;
	if (vol->mft_zone_start <= tc)
		vol->mft_zone_start = (ntfs_cluster_t)0;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->mft_zone_start = %x\n",
			vol->mft_zone_start);
	/*
	 * Need to cap the mft zone on non-standard volumes so that it does
	 * not point outside the boundaries of the volume, we do this by
	 * halving the zone size until we are inside the volume.
	 */
	vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	while (vol->mft_zone_end >= vol->nr_clusters) {
		mft_zone_size >>= 1;
		vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	}
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->mft_zone_end = %x\n",
			vol->mft_zone_end);
	/*
	 * Set the current position within each data zone to the start of the
	 * respective zone.
	 */
	vol->data1_zone_pos = vol->mft_zone_end;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->data1_zone_pos = %x\n",
			vol->data1_zone_pos);
	vol->data2_zone_pos = (ntfs_cluster_t)0;
	ntfs_debug(DEBUG_FILE3, "ntfs_init_volume: vol->data2_zone_pos = %x\n",
			vol->data2_zone_pos);
	/* Set the mft data allocation position to mft record 24. */
	vol->mft_data_pos = 24UL;
	/* This will be initialized later. */
	vol->upcase = 0;
	vol->upcase_length = 0;
	vol->mft_ino = 0;
	return 0;
}

static void ntfs_init_upcase(ntfs_inode *upcase)
{
	ntfs_io io;
#define UPCASE_LENGTH  256
	upcase->vol->upcase = ntfs_malloc(UPCASE_LENGTH << 1);
	if (!upcase->vol->upcase)
		return;
	io.fn_put = ntfs_put;
	io.fn_get = 0;
	io.param = (char*)upcase->vol->upcase;
	io.size = UPCASE_LENGTH << 1;
	ntfs_read_attr(upcase, upcase->vol->at_data, 0, 0, &io);
	upcase->vol->upcase_length = io.size >> 1;
}

static int process_attrdef(ntfs_inode* attrdef, ntfs_u8* def)
{
	int type = NTFS_GETU32(def+0x80);
	int check_type = 0;
	ntfs_volume *vol = attrdef->vol;
	ntfs_u16* name = (ntfs_u16*)def;

	if (!type) {
		ntfs_debug(DEBUG_OTHER, "process_atrdef: finished processing "
							"and returning 1\n");
		return 1;
	}
	if (ntfs_ua_strncmp(name, "$STANDARD_INFORMATION", 64) == 0) {
		vol->at_standard_information = type;
		check_type = 0x10;
	} else if (ntfs_ua_strncmp(name, "$ATTRIBUTE_LIST", 64) == 0) {
		vol->at_attribute_list = type;
		check_type = 0x20;
	} else if (ntfs_ua_strncmp(name, "$FILE_NAME", 64) == 0) {
		vol->at_file_name = type;
		check_type = 0x30;
	} else if (ntfs_ua_strncmp(name, "$VOLUME_VERSION", 64) == 0) {
		vol->at_volume_version = type;
		check_type = 0x40;
	} else if (ntfs_ua_strncmp(name, "$SECURITY_DESCRIPTOR", 64) == 0) {
		vol->at_security_descriptor = type;
		check_type = 0x50;
	} else if (ntfs_ua_strncmp(name, "$VOLUME_NAME", 64) == 0) {
		vol->at_volume_name = type;
		check_type = 0x60;
	} else if (ntfs_ua_strncmp(name, "$VOLUME_INFORMATION", 64) == 0) {
		vol->at_volume_information = type;
		check_type = 0x70;
	} else if (ntfs_ua_strncmp(name, "$DATA", 64) == 0) {
		vol->at_data = type;
		check_type = 0x80;
	} else if (ntfs_ua_strncmp(name, "$INDEX_ROOT", 64) == 0) {
		vol->at_index_root = type;
		check_type = 0x90;
	} else if (ntfs_ua_strncmp(name, "$INDEX_ALLOCATION", 64) == 0) {
		vol->at_index_allocation = type;
		check_type = 0xA0;
	} else if (ntfs_ua_strncmp(name, "$BITMAP", 64) == 0) {
		vol->at_bitmap = type;
		check_type = 0xB0;
	} else if (ntfs_ua_strncmp(name, "$SYMBOLIC_LINK", 64) == 0 ||
		 ntfs_ua_strncmp(name, "$REPARSE_POINT", 64) == 0) {
		vol->at_symlink = type;
		check_type = 0xC0;
	}
	if (check_type && check_type != type) {
		ntfs_error("process_attrdef: unexpected type 0x%x for 0x%x\n",
							type, check_type);
		return -EINVAL;
	}
	ntfs_debug(DEBUG_OTHER, "process_attrdef: found %s attribute of type "
			"0x%x\n", check_type ? "known" : "unknown", type);
	return 0;
}

int ntfs_init_attrdef(ntfs_inode* attrdef)
{
	ntfs_u8 *buf;
	ntfs_io io;
	__s64 offset;
	unsigned i;
	int error;
	ntfs_attribute *data;

	ntfs_debug(DEBUG_BSD, "Entered ntfs_init_attrdef()\n");
	buf = ntfs_malloc(4050); /* 90*45 */
	if (!buf)
		return -ENOMEM;
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	io.do_read = 1;
	offset = 0;
	data = ntfs_find_attr(attrdef, attrdef->vol->at_data, 0);
	ntfs_debug(DEBUG_BSD, "In ntfs_init_attrdef() after call to "
			"ntfs_find_attr.\n");
	if (!data) {
		ntfs_free(buf);
		return -EINVAL;
	}
	do {
		io.param = buf;
		io.size = 4050;
		ntfs_debug(DEBUG_BSD, "In ntfs_init_attrdef() going to call "
				"ntfs_readwrite_attr.\n");
		error = ntfs_readwrite_attr(attrdef, data, offset, &io);
		ntfs_debug(DEBUG_BSD, "In ntfs_init_attrdef() after call to "
				"ntfs_readwrite_attr.\n");
		for (i = 0; !error && i <= io.size - 0xA0; i += 0xA0) {
			ntfs_debug(DEBUG_BSD, "In ntfs_init_attrdef() going "
					"to call process_attrdef.\n");
			error = process_attrdef(attrdef, buf + i);
			ntfs_debug(DEBUG_BSD, "In ntfs_init_attrdef() after "
					"call to process_attrdef.\n");
		}
		offset += 4096;
	} while (!error && io.size);
	ntfs_debug(DEBUG_BSD, "Exiting ntfs_init_attrdef()\n");
	ntfs_free(buf);
	return error == 1 ? 0 : error;
}

/* ntfs_get_version will determine the NTFS version of the volume and will
 * return the version in a BCD format, with the MSB being the major version
 * number and the LSB the minor one. Otherwise return <0 on error.
 * Example: version 3.1 will be returned as 0x0301. This has the obvious
 * limitation of not coping with version numbers above 0x80 but that shouldn't
 * be a problem... */
int ntfs_get_version(ntfs_inode* volume)
{
	ntfs_attribute *volinfo;

	volinfo = ntfs_find_attr(volume, volume->vol->at_volume_information, 0);
	if (!volinfo) 
		return -EINVAL;
	if (!volinfo->resident) {
		ntfs_error("Volume information attribute is not resident!\n");
		return -EINVAL;
	}
	return ((ntfs_u8*)volinfo->d.data)[8] << 8 | 
	       ((ntfs_u8*)volinfo->d.data)[9];
}

int ntfs_load_special_files(ntfs_volume *vol)
{
	int error;
	ntfs_inode upcase, attrdef, volume;

	vol->mft_ino = (ntfs_inode*)ntfs_calloc(sizeof(ntfs_inode));
	vol->mftmirr = (ntfs_inode*)ntfs_calloc(sizeof(ntfs_inode));
	vol->bitmap = (ntfs_inode*)ntfs_calloc(sizeof(ntfs_inode));
	vol->ino_flags = 4 | 2 | 1;
	error = -ENOMEM;
	ntfs_debug(DEBUG_BSD, "Going to load MFT\n");
	if (!vol->mft_ino || (error = ntfs_init_inode(vol->mft_ino, vol,
			FILE_Mft))) {
		ntfs_error("Problem loading MFT\n");
		return error;
	}
	ntfs_debug(DEBUG_BSD, "Going to load MIRR\n");
	if ((error = ntfs_init_inode(vol->mftmirr, vol, FILE_MftMirr))) {
		ntfs_error("Problem %d loading MFTMirr\n", error);
		return error;
	}
	ntfs_debug(DEBUG_BSD, "Going to load BITMAP\n");
	if ((error = ntfs_init_inode(vol->bitmap, vol, FILE_BitMap))) {
		ntfs_error("Problem loading Bitmap\n");
		return error;
	}
	ntfs_debug(DEBUG_BSD, "Going to load UPCASE\n");
	error = ntfs_init_inode(&upcase, vol, FILE_UpCase);
	if (error)
		return error;
	ntfs_init_upcase(&upcase);
	ntfs_clear_inode(&upcase);
	ntfs_debug(DEBUG_BSD, "Going to load ATTRDEF\n");
	error = ntfs_init_inode(&attrdef, vol, FILE_AttrDef);
	if (error)
		return error;
	error = ntfs_init_attrdef(&attrdef);
	ntfs_clear_inode(&attrdef);
	if (error)
		return error;

	/* Check for NTFS version and if Win2k version (ie. 3.0+) do not allow
	 * write access since the driver write support is broken. */
	ntfs_debug(DEBUG_BSD, "Going to load VOLUME\n");
	error = ntfs_init_inode(&volume, vol, FILE_Volume);
	if (error)
		return error;
	if ((error = ntfs_get_version(&volume)) >= 0x0300 &&
	    !(NTFS_SB(vol)->s_flags & MS_RDONLY)) {
		NTFS_SB(vol)->s_flags |= MS_RDONLY;
		ntfs_error("Warning! NTFS volume version is Win2k+: Mounting "
			   "read-only\n");
	}
	ntfs_clear_inode(&volume);
	if (error < 0)
		return error;
	ntfs_debug(DEBUG_BSD, "NTFS volume is v%d.%d\n", error >> 8,
			error & 0xff);
	return 0;
}

int ntfs_release_volume(ntfs_volume *vol)
{
	if (((vol->ino_flags & 1) == 1) && vol->mft_ino) {
		ntfs_clear_inode(vol->mft_ino);
		ntfs_free(vol->mft_ino);
		vol->mft_ino = 0;
	}
	if (((vol->ino_flags & 2) == 2) && vol->mftmirr) {
		ntfs_clear_inode(vol->mftmirr);
		ntfs_free(vol->mftmirr);
		vol->mftmirr = 0;
	}
	if (((vol->ino_flags & 4) == 4) && vol->bitmap) {
		ntfs_clear_inode(vol->bitmap);
		ntfs_free(vol->bitmap);
		vol->bitmap = 0;
	}
	ntfs_free(vol->mft);
	ntfs_free(vol->upcase);
	return 0;
}

/*
 * Writes the volume size (units of clusters) into vol_size.
 * Returns 0 if successful or error.
 */
int ntfs_get_volumesize(ntfs_volume *vol, ntfs_s64 *vol_size)
{
	ntfs_io io;
	char *cluster0;

	if (!vol_size)
		return -EFAULT;
	cluster0 = ntfs_malloc(vol->cluster_size);
	if (!cluster0)
		return -ENOMEM;
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	io.param = cluster0;
	io.do_read = 1;
	io.size = vol->cluster_size;
	ntfs_getput_clusters(vol, 0, 0, &io);
	*vol_size = NTFS_GETU64(cluster0 + 0x28) >>
					(ffs(NTFS_GETU8(cluster0 + 0xD)) - 1);
	ntfs_free(cluster0);
	return 0;
}

static int nc[16]={4,3,3,2,3,2,2,1,3,2,2,1,2,1,1,0};

int ntfs_get_free_cluster_count(ntfs_inode *bitmap)
{
	ntfs_io io;
	int offset, error, clusters;
	unsigned char *bits = ntfs_malloc(2048);
	if (!bits)
		return -ENOMEM;
	offset = clusters = 0;
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	while (1) {
		register int i;
		io.param = bits;
		io.size = 2048;
		error = ntfs_read_attr(bitmap, bitmap->vol->at_data, 0, offset,
									&io);
		if (error || io.size == 0)
			break;
		/* I never thought I would do loop unrolling some day */
		for (i = 0; i < io.size - 8; ) {
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
			clusters+=nc[bits[i]>>4];clusters+=nc[bits[i++] & 0xF];
		}
		while (i < io.size) {
			clusters += nc[bits[i] >> 4];
			clusters += nc[bits[i++] & 0xF];
		}
		offset += io.size;
	}
	ntfs_free(bits);
	return clusters;
}

/*
 * Insert the fixups for the record. The number and location of the fixes
 * is obtained from the record header but we double check with @rec_size and
 * use that as the upper boundary, if necessary overwriting the count value in
 * the record header.
 *
 * We return 0 on success or -1 if fixup header indicated the beginning of the
 * update sequence array to be beyond the valid limit.
 */
int ntfs_insert_fixups(unsigned char *rec, int rec_size)
{
	int first;
	int count;
	int offset = -2;
	ntfs_u16 fix;
	
	first = NTFS_GETU16(rec + 4);
	count = (rec_size >> NTFS_SECTOR_BITS) + 1;
	if (first + count * 2 > NTFS_SECTOR_SIZE - 2) {
		printk(KERN_CRIT "NTFS: ntfs_insert_fixups() detected corrupt "
				"NTFS record update sequence array position. - "
				"Cannot hotfix.\n");
		return -1;
	}
	if (count != NTFS_GETU16(rec + 6)) {
		printk(KERN_ERR "NTFS: ntfs_insert_fixups() detected corrupt "
				"NTFS record update sequence array size. - "
				"Applying hotfix.\n");
		NTFS_PUTU16(rec + 6, count);
	}
	fix = (NTFS_GETU16(rec + first) + 1) & 0xffff;
	if (fix == 0xffff || !fix)
		fix = 1;
	NTFS_PUTU16(rec + first, fix);
	count--;
	while (count--) {
		first += 2;
		offset += NTFS_SECTOR_SIZE;
		NTFS_PUTU16(rec + first, NTFS_GETU16(rec + offset));
		NTFS_PUTU16(rec + offset, fix);
	}
	return 0;
}

/**
 * ntfs_allocate_clusters - allocate logical clusters on an ntfs volume
 * @vol:	volume on which to allocate clusters
 * @location:	preferred location for first allocated cluster
 * @count:	number of clusters to allocate
 * @rl:		address of pointer in which to return the allocated run list
 * @rl_len:	the number of elements returned in @*rl
 *
 * Allocate @*count clusters (LCNs), preferably beginning at @*location in the
 * bitmap of the volume @vol. If @*location is -1, it does not matter where the
 * clusters are. @rl is the address of a ntfs_runlist pointer which this
 * function will allocate and fill with the runlist of the allocated clusters.
 * It is the callers responsibility to ntfs_vfree() @*rl after she is finished
 * with it. If the function was not successful, @*rl will be set to NULL.
 * @*rl_len will contain the number of ntfs_runlist elements in @*rl or 0 if
 * @*rl is NULL.
 *
 * Return 0 on success, or -errno on error. On success, @*location and @*count
 * say what was really allocated. On -ENOSPC, @*location and @*count say what
 * could have been allocated. If nothing could be allocated or a different
 * error occured, @*location = -1 and @*count = 0.
 *
 * There are two data zones. First is the area between the end of the mft zone
 * and the end of the volume, and second is the area between the start of the
 * volume and the start of the mft zone. On unmodified/standard volumes, the
 * second mft zone doesn't exist due to the mft zone being expanded to cover
 * the start of volume in order to reserve space for the mft bitmap attribute.
 *
 * This is not the prettiest function but the complexity stems from the need of
 * implementing the mft vs data zoned approach and from the fact that we have
 * access to the lcn bitmap in portions of PAGE_SIZE bytes at a time, so we
 * need to cope with crossing over boundaries of two pages. Further, the fact
 * that the allocator allows for caller supplied hints as to the location of
 * where allocation should begin and the fact that the allocator keeps track of
 * where in the data zones the next natural allocation should occur, contribute
 * to the complexity of the function. But it should all be worthwhile, because
 * this allocator should: 1) be a full implementation of the MFT zone approach
 * used by Windows, 2) cause reduction in fragmentation as much as possible,
 * and 3) be speedy in allocations (the code is not optimized for speed, but
 * the algorithm is, so further speed improvements are probably possible).
 *
 * FIXME: Really need finer-grained locking but this will do for the moment. I
 * just want to kill all races and have a working allocator. When that is done,
 * we can beautify... (AIA)
 * 
 * FIXME: We should be monitoring cluster allocation and increment the MFT zone
 * size dynamically but this is something for the future. We will just cause
 * heavier fragmentation by not doing it and I am not even sure Windows would
 * grow the MFT zone dynamically, so might even be correct not doing this. The
 * overhead in doing dynamic MFT zone expansion would be very large and unlikely
 * worth the effort. (AIA)
 *
 * TODO: I have added in double the required zone position pointer wrap around
 * logic which can be optimized to having only one of the two logic sets.
 * However, having the double logic will work fine, but if we have only one of
 * the sets and we get it wrong somewhere, then we get into trouble, so
 * removing the duplicate logic requires _very_ careful consideration of _all_
 * possible code paths. So at least for now, I am leaving the double logic -
 * better safe than sorry... (AIA)
 */
int ntfs_allocate_clusters(ntfs_volume *vol, ntfs_cluster_t *location,
		ntfs_cluster_t *count, ntfs_runlist **rl, int *rl_len,
		const NTFS_CLUSTER_ALLOCATION_ZONES zone)
{
	ntfs_runlist *rl2 = NULL, *rlt;
	ntfs_attribute *data;
	ntfs_cluster_t buf_pos, zone_start, zone_end, mft_zone_size;
	ntfs_cluster_t lcn, last_read_pos, prev_lcn = (ntfs_cluster_t)0;
	ntfs_cluster_t initial_location, prev_run_len = (ntfs_cluster_t)0;
	ntfs_cluster_t clusters = (ntfs_cluster_t)0;
	unsigned char *buf, *byte, bit, search_zone, done_zones;
	unsigned char pass, need_writeback;
	int rlpos = 0, rlsize, buf_size, err = 0;
	ntfs_io io;

	ntfs_debug(DEBUG_OTHER, "%s(): Entering with *location = 0x%x, "
			"*count = 0x%x, zone = %s_ZONE.\n", __FUNCTION__,
			*location, *count, zone == DATA_ZONE ? "DATA" : "MFT");
	buf = (char*)__get_free_page(GFP_NOFS);
	if (!buf) {
		ntfs_debug(DEBUG_OTHER, "%s(): Returning -ENOMEM.\n",
				__FUNCTION__);
		return -ENOMEM;
	}
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	lock_kernel();
	/* Get the $DATA attribute of $Bitmap. */
	data = ntfs_find_attr(vol->bitmap, vol->at_data, 0);
	if (!data) {
		err = -EINVAL;
		goto err_ret;
	}
	/*
	 * If no specific location was requested, use the current data zone
	 * position, otherwise use the requested location but make sure it lies
	 * outside the mft zone. Also set done_zones to 0 (no zones done) and
	 * pass depending on whether we are starting inside a zone (1) or
	 * at the beginning of a zone (2). If requesting from the MFT_ZONE, then
	 * we either start at the current position within the mft zone or at the
	 * specified position and if the latter is out of bounds then we start
	 * at the beginning of the MFT_ZONE.
	 */
	done_zones = 0;
	pass = 1;
	/*
	 * zone_start and zone_end are the current search range. search_zone
	 * is 1 for mft zone, 2 for data zone 1 (end of mft zone till end of
	 * volume) and 4 for data zone 2 (start of volume till start of mft
	 * zone).
	 */
	zone_start = *location;
	if (zone_start < 0) {
		if (zone == DATA_ZONE)
			zone_start = vol->data1_zone_pos;
		else
			zone_start = vol->mft_zone_pos;
		if (!zone_start)
			/*
			 * Zone starts at beginning of volume which means a
			 * single pass is sufficient.
			 */
			pass = 2;
	} else if (zone_start >= vol->mft_zone_start && zone_start <
			vol->mft_zone_end && zone == DATA_ZONE) {
		zone_start = vol->mft_zone_end;
		pass = 2;
	} else if ((zone_start < vol->mft_zone_start || zone_start >=
			vol->mft_zone_end) && zone == MFT_ZONE) {
		zone_start = vol->mft_lcn;
		if (!vol->mft_zone_end)
			zone_start = (ntfs_cluster_t)0;
		pass = 2;
	}
	if (zone == DATA_ZONE) {
		/* Skip searching the mft zone. */
		done_zones |= 1;
		if (zone_start >= vol->mft_zone_end) {
			zone_end = vol->nr_clusters;
			search_zone = 2;
		} else {
			zone_end = vol->mft_zone_start;
			search_zone = 4;
		}
	} else /* if (zone == MFT_ZONE) */ {
		zone_end = vol->mft_zone_end;
		search_zone = 1;
	}
	/*
	 * buf_pos is the current bit position inside the bitmap. We use
	 * initial_location to determine whether or not to do a zone switch.
	 */
	buf_pos = initial_location = zone_start;
	/* Loop until all clusters are allocated, i.e. clusters == 0. */
	clusters = *count;
	rlpos = rlsize = 0;
	if (*count <= 0) {
		ntfs_debug(DEBUG_OTHER, "%s(): *count <= 0, "
				"returning -EINVAL.\n", __FUNCTION__);
		err = -EINVAL;
		goto err_ret;
	}
	while (1) {
		ntfs_debug(DEBUG_OTHER, "%s(): Start of outer while "
				"loop: done_zones = 0x%x, search_zone = %i, "
				"pass = %i, zone_start = 0x%x, zone_end = "
				"0x%x, initial_location = 0x%x, buf_pos = "
				"0x%x, rlpos = %i, rlsize = %i.\n",
				__FUNCTION__, done_zones, search_zone, pass,
				zone_start, zone_end, initial_location, buf_pos,
				rlpos, rlsize);
		/* Loop until we run out of free clusters. */
		io.param = buf;
		io.size = PAGE_SIZE;
		io.do_read = 1;
		last_read_pos = buf_pos >> 3;
		ntfs_debug(DEBUG_OTHER, "%s(): last_read_pos = 0x%x.\n",
				__FUNCTION__, last_read_pos);
		err = ntfs_readwrite_attr(vol->bitmap, data, last_read_pos,
				&io);
		if (err) {
			ntfs_debug(DEBUG_OTHER, "%s(): ntfs_read_attr failed "
					"with error code %i, going to "
					"err_ret.\n", __FUNCTION__, -err);
			goto err_ret;
		}
		if (!io.size) {
			ntfs_debug(DEBUG_OTHER, "%s(): !io.size, going to "
					"zone_pass_done.\n", __FUNCTION__);
			goto zone_pass_done;
		}
		buf_size = io.size << 3;
		lcn = buf_pos & 7;
		buf_pos &= ~7;
		need_writeback = 0;
		ntfs_debug(DEBUG_OTHER, "%s(): Before inner while "
				"loop: buf_size = 0x%x, lcn = 0x%x, buf_pos = "
				"0x%x, need_writeback = %i.\n", __FUNCTION__,
				buf_size, lcn, buf_pos, need_writeback);
		while (lcn < buf_size && lcn + buf_pos < zone_end) {
			byte = buf + (lcn >> 3);
			ntfs_debug(DEBUG_OTHER, "%s(): In inner while loop: "
					"buf_size = 0x%x, lcn = 0x%x, buf_pos "
					"= 0x%x, need_writeback = %i, byte ofs "
					"= 0x%x, *byte = 0x%x.\n", __FUNCTION__,
					buf_size, lcn, buf_pos,	need_writeback,
					lcn >> 3, *byte);
			/* Skip full bytes. */
			if (*byte == 0xff) {
				lcn += 8;
				ntfs_debug(DEBUG_OTHER, "%s(): continuing while"
					    " loop 1.\n", __FUNCTION__);
				continue;
			}
			bit = 1 << (lcn & 7);
			ntfs_debug(DEBUG_OTHER, "%s(): bit = %i.\n",
					__FUNCTION__, bit);
			/* If the bit is already set, go onto the next one. */
			if (*byte & bit) {
				lcn++;
				ntfs_debug(DEBUG_OTHER, "%s(): continuing while"
					    " loop 2.\n", __FUNCTION__);
				continue;
			}
			/* Allocate the bitmap bit. */
			*byte |= bit;
			/* We need to write this bitmap buffer back to disk! */
			need_writeback = 1;
			ntfs_debug(DEBUG_OTHER, "%s(): *byte = 0x%x, "
					"need_writeback = %i.\n", __FUNCTION__,
					*byte, need_writeback);
			/* Reallocate memory if necessary. */
			if ((rlpos + 2) * sizeof(ntfs_runlist) >= rlsize) {
				ntfs_debug(DEBUG_OTHER, "%s(): Reallocating "
						"space.\n", __FUNCTION__);
				/* Setup first free bit return value. */
				if (!rl2) {
					*location = lcn + buf_pos;
					ntfs_debug(DEBUG_OTHER,	"%s(): "
							"*location = 0x%x.\n",
							__FUNCTION__,
							*location);
				}
				rlsize += PAGE_SIZE;
				rlt = ntfs_vmalloc(rlsize);
				if (!rlt) {
					err = -ENOMEM;
					ntfs_debug(DEBUG_OTHER, "%s(): Failed "
							"to allocate memory, "
							"returning -ENOMEM, "
							"going to wb_err_ret.\n",
							__FUNCTION__);
					goto wb_err_ret;
				}
				if (rl2) {
					ntfs_memcpy(rlt, rl2, rlsize -
							PAGE_SIZE);
					ntfs_vfree(rl2);
				}
				rl2 = rlt;
				ntfs_debug(DEBUG_OTHER, "%s(): Reallocated "
						"memory, rlsize = 0x%x.\n",
						__FUNCTION__, rlsize);
			}
			/*
			 * Coalesce with previous run if adjacent LCNs.
			 * Otherwise, append a new run.
			 */
			ntfs_debug(DEBUG_OTHER, "%s(): Adding run (lcn 0x%x, "
					"len 0x%x), prev_lcn = 0x%x, lcn = "
					"0x%x, buf_pos = 0x%x, prev_run_len = "
					"0x%x, rlpos = %i.\n", __FUNCTION__,
					lcn + buf_pos, 1, prev_lcn, lcn,
					buf_pos, prev_run_len, rlpos);
			if (prev_lcn == lcn + buf_pos - prev_run_len && rlpos) {
				ntfs_debug(DEBUG_OTHER, "%s(): Coalescing to "
						"run (lcn 0x%x, len 0x%x).\n",
						__FUNCTION__,
						rl2[rlpos - 1].lcn,
						rl2[rlpos - 1].len);
				rl2[rlpos - 1].len = ++prev_run_len;
				ntfs_debug(DEBUG_OTHER, "%s(): Run now (lcn "
						"0x%x, len 0x%x), prev_run_len "
						"= 0x%x.\n", __FUNCTION__,
						rl2[rlpos - 1].lcn,
						rl2[rlpos - 1].len,
						prev_run_len);
			} else {
				if (rlpos)
					ntfs_debug(DEBUG_OTHER, "%s(): Adding "
							"new run, (previous "
							"run lcn 0x%x, "
							"len 0x%x).\n",
							__FUNCTION__,
							rl2[rlpos - 1].lcn,
							rl2[rlpos - 1].len);
				else
					ntfs_debug(DEBUG_OTHER, "%s(): Adding "
							"new run, is first "
							"run.\n", __FUNCTION__);
				rl2[rlpos].lcn = prev_lcn = lcn + buf_pos;
				rl2[rlpos].len = prev_run_len =
						(ntfs_cluster_t)1;
				
				rlpos++;
			}
			/* Done? */
			if (!--clusters) {
				ntfs_cluster_t tc;
				/*
				 * Update the current zone position. Positions
				 * of already scanned zones have been updated
				 * during the respective zone switches.
				 */
				tc = lcn + buf_pos + 1;
				ntfs_debug(DEBUG_OTHER, "%s(): Done. Updating "
						"current zone position, tc = "
						"0x%x, search_zone = %i.\n",
						__FUNCTION__, tc, search_zone);
				switch (search_zone) {
				case 1:
					ntfs_debug(DEBUG_OTHER,
							"%s(): Before checks, "
							"vol->mft_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->mft_zone_pos);
					if (tc >= vol->mft_zone_end) {
						vol->mft_zone_pos =
								vol->mft_lcn;
						if (!vol->mft_zone_end)
							vol->mft_zone_pos =
							     (ntfs_cluster_t)0;
					} else if ((initial_location >=
							vol->mft_zone_pos ||
							tc > vol->mft_zone_pos)
							&& tc >= vol->mft_lcn)
						vol->mft_zone_pos = tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): After checks, "
							"vol->mft_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->mft_zone_pos);
					break;
				case 2:
					ntfs_debug(DEBUG_OTHER,
							"%s(): Before checks, "
							"vol->data1_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data1_zone_pos);
					if (tc >= vol->nr_clusters)
						vol->data1_zone_pos =
							     vol->mft_zone_end;
					else if ((initial_location >=
						    vol->data1_zone_pos ||
						    tc > vol->data1_zone_pos)
						    && tc >= vol->mft_zone_end)
						vol->data1_zone_pos = tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): After checks, "
							"vol->data1_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data1_zone_pos);
					break;
				case 4:
					ntfs_debug(DEBUG_OTHER,
							"%s(): Before checks, "
							"vol->data2_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data2_zone_pos);
					if (tc >= vol->mft_zone_start)
						vol->data2_zone_pos =
							(ntfs_cluster_t)0;
					else if (initial_location >=
						      vol->data2_zone_pos ||
						      tc > vol->data2_zone_pos)
						vol->data2_zone_pos = tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): After checks, "
							"vol->data2_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data2_zone_pos);
					break;
				default:
					BUG();
				}
				ntfs_debug(DEBUG_OTHER, "%s(): Going to "
						"done_ret.\n", __FUNCTION__);
				goto done_ret;
			}
			lcn++;
		}
		buf_pos += buf_size;
		ntfs_debug(DEBUG_OTHER, "%s(): After inner while "
				"loop: buf_size = 0x%x, lcn = 0x%x, buf_pos = "
				"0x%x, need_writeback = %i.\n", __FUNCTION__,
				buf_size, lcn, buf_pos, need_writeback);
		if (need_writeback) {
			ntfs_debug(DEBUG_OTHER, "%s(): Writing back.\n",
					__FUNCTION__);
			need_writeback = 0;
			io.param = buf;
			io.do_read = 0;
			err = ntfs_readwrite_attr(vol->bitmap, data,
					last_read_pos, &io);
			if (err) {
				ntfs_error("%s(): Bitmap writeback failed "
						"in read next buffer code "
						"path with error code %i.\n",
						__FUNCTION__, -err);
				goto err_ret;
			}
		}
		if (buf_pos < zone_end) {
			ntfs_debug(DEBUG_OTHER, "%s(): Continuing "
					"outer while loop, buf_pos = 0x%x, "
					"zone_end = 0x%x.\n", __FUNCTION__,
					buf_pos, zone_end);
			continue;
		}
zone_pass_done:	/* Finished with the current zone pass. */
		ntfs_debug(DEBUG_OTHER, "%s(): At zone_pass_done, pass = %i.\n",
				__FUNCTION__, pass);
		if (pass == 1) {
			/*
			 * Now do pass 2, scanning the first part of the zone
			 * we omitted in pass 1.
			 */
			pass = 2;
			zone_end = zone_start;
			switch (search_zone) {
			case 1: /* mft_zone */
				zone_start = vol->mft_zone_start;
				break;
			case 2: /* data1_zone */
				zone_start = vol->mft_zone_end;
				break;
			case 4: /* data2_zone */
				zone_start = (ntfs_cluster_t)0;
				break;
			default:
				BUG();
			}
			/* Sanity check. */
			if (zone_end < zone_start)
				zone_end = zone_start;
			buf_pos = zone_start;
			ntfs_debug(DEBUG_OTHER, "%s(): Continuing "
					"outer while loop, pass = 2, "
					"zone_start = 0x%x, zone_end = 0x%x, "
					"buf_pos = 0x%x.\n", __FUNCTION__,
					zone_start, zone_end, buf_pos);
			continue;
		} /* pass == 2 */
done_zones_check:
		ntfs_debug(DEBUG_OTHER, "%s(): At done_zones_check, "
				"search_zone = %i, done_zones before = 0x%x, "
				"done_zones after = 0x%x.\n", __FUNCTION__,
				search_zone, done_zones, done_zones |
				search_zone);
		done_zones |= search_zone;
		if (done_zones < 7) {
			ntfs_debug(DEBUG_OTHER, "%s(): Switching zone.\n",
					__FUNCTION__);
			/* Now switch to the next zone we haven't done yet. */
			pass = 1;
			switch (search_zone) {
			case 1:
				ntfs_debug(DEBUG_OTHER, "%s(): Switching from "
						"mft zone to data1 zone.\n",
						__FUNCTION__);
				/* Update mft zone position. */
				if (rlpos) {
					ntfs_cluster_t tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): Before checks, "
							"vol->mft_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->mft_zone_pos);
					tc = rl2[rlpos - 1].lcn +
							rl2[rlpos - 1].len;
					if (tc >= vol->mft_zone_end) {
						vol->mft_zone_pos =
								vol->mft_lcn;
						if (!vol->mft_zone_end)
							vol->mft_zone_pos =
							     (ntfs_cluster_t)0;
					} else if ((initial_location >=
							vol->mft_zone_pos ||
							tc > vol->mft_zone_pos)
							&& tc >= vol->mft_lcn)
						vol->mft_zone_pos = tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): After checks, "
							"vol->mft_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->mft_zone_pos);
				}
				/* Switch from mft zone to data1 zone. */
switch_to_data1_zone:		search_zone = 2;
				zone_start = initial_location =
						vol->data1_zone_pos;
				zone_end = vol->nr_clusters;
				if (zone_start == vol->mft_zone_end)
					pass = 2;
				if (zone_start >= zone_end) {
					vol->data1_zone_pos = zone_start =
							vol->mft_zone_end;
					pass = 2;
				}
				break;
			case 2:
				ntfs_debug(DEBUG_OTHER, "%s(): Switching from "
						"data1 zone to data2 zone.\n",
						__FUNCTION__);
				/* Update data1 zone position. */
				if (rlpos) {
					ntfs_cluster_t tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): Before checks, "
							"vol->data1_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data1_zone_pos);
					tc = rl2[rlpos - 1].lcn +
							rl2[rlpos - 1].len;
					if (tc >= vol->nr_clusters)
						vol->data1_zone_pos =
							     vol->mft_zone_end;
					else if ((initial_location >=
						    vol->data1_zone_pos ||
						    tc > vol->data1_zone_pos)
						    && tc >= vol->mft_zone_end)
						vol->data1_zone_pos = tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): After checks, "
							"vol->data1_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data1_zone_pos);
				}
				/* Switch from data1 zone to data2 zone. */
				search_zone = 4;
				zone_start = initial_location =
						vol->data2_zone_pos;
				zone_end = vol->mft_zone_start;
				if (!zone_start)
					pass = 2;
				if (zone_start >= zone_end) {
					vol->data2_zone_pos = zone_start =
							initial_location =
							(ntfs_cluster_t)0;
					pass = 2;
				}
				break;
			case 4:
				ntfs_debug(DEBUG_OTHER, "%s(): Switching from "
						"data2 zone to data1 zone.\n",
						__FUNCTION__);
				/* Update data2 zone position. */
				if (rlpos) {
					ntfs_cluster_t tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): Before checks, "
							"vol->data2_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data2_zone_pos);
					tc = rl2[rlpos - 1].lcn +
							rl2[rlpos - 1].len;
					if (tc >= vol->mft_zone_start)
						vol->data2_zone_pos =
							     (ntfs_cluster_t)0;
					else if (initial_location >=
						      vol->data2_zone_pos ||
						      tc > vol->data2_zone_pos)
						vol->data2_zone_pos = tc;
					ntfs_debug(DEBUG_OTHER,
							"%s(): After checks, "
							"vol->data2_zone_pos = "
							"0x%x.\n", __FUNCTION__,
							vol->data2_zone_pos);
				}
				/* Switch from data2 zone to data1 zone. */
				goto switch_to_data1_zone; /* See above. */
			default:
				BUG();
			}
			ntfs_debug(DEBUG_OTHER, "%s(): After zone switch, "
					"search_zone = %i, pass = %i, "
					"initial_location = 0x%x, zone_start "
					"= 0x%x, zone_end = 0x%x.\n",
					__FUNCTION__, search_zone, pass,
					initial_location, zone_start, zone_end);
			buf_pos = zone_start;
			if (zone_start == zone_end) {
				ntfs_debug(DEBUG_OTHER, "%s(): Empty zone, "
						"going to done_zones_check.\n",
						__FUNCTION__);
				/* Empty zone. Don't bother searching it. */
				goto done_zones_check;
			}
			ntfs_debug(DEBUG_OTHER, "%s(): Continuing outer while "
					"loop.\n", __FUNCTION__);
			continue;
		} /* done_zones == 7 */
		ntfs_debug(DEBUG_OTHER, "%s(): All zones are finished.\n",
				__FUNCTION__);
		/*
		 * All zones are finished! If DATA_ZONE, shrink mft zone. If
		 * MFT_ZONE, we have really run out of space.
		 */
		mft_zone_size = vol->mft_zone_end - vol->mft_zone_start;
		ntfs_debug(DEBUG_OTHER, "%s(): vol->mft_zone_start = 0x%x, "
				"vol->mft_zone_end = 0x%x, mft_zone_size = "
				"0x%x.\n", __FUNCTION__, vol->mft_zone_start,
				vol->mft_zone_end, mft_zone_size);
		if (zone == MFT_ZONE || mft_zone_size <= (ntfs_cluster_t)0) {
			ntfs_debug(DEBUG_OTHER, "%s(): No free clusters left, "
					"returning -ENOSPC, going to "
					"fail_ret.\n", __FUNCTION__);
			/* Really no more space left on device. */
			err = -ENOSPC;
			goto fail_ret;
		} /* zone == DATA_ZONE && mft_zone_size > 0 */
		ntfs_debug(DEBUG_OTHER, "%s(): Shrinking mft zone.\n",
				__FUNCTION__);
		zone_end = vol->mft_zone_end;
		mft_zone_size >>= 1;
		if (mft_zone_size > (ntfs_cluster_t)0)
			vol->mft_zone_end = vol->mft_zone_start + mft_zone_size;
		else /* mft zone and data2 zone no longer exist. */
			vol->data2_zone_pos = vol->mft_zone_start =
					vol->mft_zone_end = (ntfs_cluster_t)0;
		if (vol->mft_zone_pos >= vol->mft_zone_end) {
			vol->mft_zone_pos = vol->mft_lcn;
			if (!vol->mft_zone_end)
				vol->mft_zone_pos = (ntfs_cluster_t)0;
		}
		buf_pos = zone_start = initial_location =
				vol->data1_zone_pos = vol->mft_zone_end;
		search_zone = 2;
		pass = 2;
		done_zones &= ~2;
		ntfs_debug(DEBUG_OTHER, "%s(): After shrinking mft "
				"zone, mft_zone_size = 0x%x, "
				"vol->mft_zone_start = 0x%x, vol->mft_zone_end "
				"= 0x%x, vol->mft_zone_pos = 0x%x, search_zone "
				"= 2, pass = 2, dones_zones = 0x%x, zone_start "
				"= 0x%x, zone_end = 0x%x, vol->data1_zone_pos "
				"= 0x%x, continuing outer while loop.\n",
				__FUNCTION__, mft_zone_size,
				vol->mft_zone_start, vol->mft_zone_end,
				vol->mft_zone_pos, done_zones, zone_start,
				zone_end, vol->data1_zone_pos);
	}
	ntfs_debug(DEBUG_OTHER, "%s(): After outer while loop.\n",
			__FUNCTION__);
done_ret:
	ntfs_debug(DEBUG_OTHER, "%s(): At done_ret.\n", __FUNCTION__);
	rl2[rlpos].lcn = (ntfs_cluster_t)-1;
	rl2[rlpos].len = (ntfs_cluster_t)0;
	*rl = rl2;
	*rl_len = rlpos;
	if (need_writeback) {
		ntfs_debug(DEBUG_OTHER, "%s(): Writing back.\n", __FUNCTION__);
		need_writeback = 0;
		io.param = buf;
		io.do_read = 0;
		err = ntfs_readwrite_attr(vol->bitmap, data, last_read_pos,
				&io);
		if (err) {
			ntfs_error("%s(): Bitmap writeback failed in done "
					"code path with error code %i.\n",
					__FUNCTION__, -err);
			goto err_ret;
		}
		ntfs_debug(DEBUG_OTHER, "%s(): Wrote 0x%Lx bytes.\n",
				__FUNCTION__, io.size);
	}
done_fail_ret:
	ntfs_debug(DEBUG_OTHER, "%s(): At done_fail_ret (follows done_ret).\n",
			 __FUNCTION__);
	unlock_kernel();
	free_page((unsigned long)buf);
	if (err)
		ntfs_debug(DEBUG_FILE3, "%s(): Failed to allocate "
				"clusters. Returning with error code %i.\n",
				__FUNCTION__, -err);
	ntfs_debug(DEBUG_OTHER, "%s(): Syncing $Bitmap inode.\n", __FUNCTION__);
	if (ntfs_update_inode(vol->bitmap))
		ntfs_error("%s(): Failed to sync inode $Bitmap. "
				"Continuing anyway.\n", __FUNCTION__);
	ntfs_debug(DEBUG_OTHER, "%s(): Returning with code %i.\n", __FUNCTION__,
			err);
	return err;
fail_ret:
	ntfs_debug(DEBUG_OTHER, "%s(): At fail_ret.\n", __FUNCTION__);
	if (rl2) {
		if (err == -ENOSPC) {
			/* Return first free lcn and count of free clusters. */
			*location = rl2[0].lcn;
			*count -= clusters;
			ntfs_debug(DEBUG_OTHER, "%s(): err = -ENOSPC, "
					"*location = 0x%x, *count = 0x%x.\n",
					__FUNCTION__, *location, *count);
		}
		/* Deallocate all allocated clusters. */
		ntfs_debug(DEBUG_OTHER, "%s(): Deallocating allocated "
				"clusters.\n", __FUNCTION__);
		ntfs_deallocate_clusters(vol, rl2, rlpos);
		/* Free the runlist. */
		ntfs_vfree(rl2);
	} else {
		if (err == -ENOSPC) {
			/* Nothing free at all. */
			*location = vol->data1_zone_pos; /* Irrelevant... */
			*count = 0;
			ntfs_debug(DEBUG_OTHER, "%s(): No space left at all, "
					"err = -ENOSPC, *location = 0x%x, "
					"*count = 0.\n",
					__FUNCTION__, *location);
		}
	}
	*rl = NULL;
	*rl_len = 0;
	ntfs_debug(DEBUG_OTHER, "%s(): *rl = NULL, *rl_len = 0, "
			"going to done_fail_ret.\n", __FUNCTION__);
	goto done_fail_ret;
wb_err_ret:
	ntfs_debug(DEBUG_OTHER, "%s(): At wb_err_ret.\n", __FUNCTION__);
	if (need_writeback) {
		int __err;
		ntfs_debug(DEBUG_OTHER, "%s(): Writing back.\n", __FUNCTION__);
		io.param = buf;
		io.do_read = 0;
		__err = ntfs_readwrite_attr(vol->bitmap, data, last_read_pos,
				&io);
		if (__err)
			ntfs_error("%s(): Bitmap writeback failed in error "
					"code path with error code %i.\n",
					__FUNCTION__, -__err);
		need_writeback = 0;
	}
err_ret:
	ntfs_debug(DEBUG_OTHER, "%s(): At err_ret, *location = -1, "
			"*count = 0, going to fail_ret.\n", __FUNCTION__);
	*location = -1;
	*count = 0;
	goto fail_ret;
}

/*
 * IMPORTANT: Caller has to hold big kernel lock or the race monster will come
 * to get you! (-;
 * TODO: Need our own lock for bitmap accesses but BKL is more secure for now,
 * considering we might not have covered all places with a lock yet. In that
 * case the BKL offers a one way exclusion which is better than no exclusion
 * at all... (AIA)
 */
static int ntfs_clear_bitrange(ntfs_inode *bitmap,
		const ntfs_cluster_t start_bit, const ntfs_cluster_t count)
{
	ntfs_cluster_t buf_size, bit, nr_bits = count;
	unsigned char *buf, *byte;
	int err;
	ntfs_io io;

	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	/* Calculate the required buffer size in bytes. */
	buf_size = (ntfs_cluster_t)((start_bit & 7) + nr_bits + 7) >> 3;
	if (buf_size <= (ntfs_cluster_t)(64 * 1024))
		buf = ntfs_malloc(buf_size);
	else
		buf = ntfs_vmalloc(buf_size);
	if (!buf)
		return -ENOMEM;
	/* Read the bitmap from the data attribute. */
	io.param = byte = buf;
	io.size = buf_size;
	err = ntfs_read_attr(bitmap, bitmap->vol->at_data, 0, start_bit >> 3,
			&io);
	if (err || io.size != buf_size)
		goto err_out;
	/* Now clear the bits in the read bitmap. */
	bit = start_bit & 7;
	while (bit && nr_bits) { /* Process first partial byte, if present. */
		*byte &= ~(1 << bit++);
		nr_bits--;
		bit &= 7;
		if (!bit)
			byte++;
	}
	while (nr_bits >= 8) { /* Process full bytes. */
		*byte = 0;
		nr_bits -= 8;
		byte++;
	}
	bit = 0;
	while (nr_bits) { /* Process last partial byte, if present. */
		*byte &= ~(1 << bit);
		nr_bits--;
		bit++;
	}
	/* Write the modified bitmap back to disk. */
	io.param = buf;
	io.size = buf_size;
	err = ntfs_write_attr(bitmap, bitmap->vol->at_data, 0, start_bit >> 3,
			&io);
err_out:
	if (buf_size <= (ntfs_cluster_t)(64 * 1024))
		ntfs_free(buf);
	else
		ntfs_vfree(buf);
	if (!err && io.size != buf_size)
		err = -EIO;
	return err;
}

/*
 * See comments for lack of zone adjustments below in the description of the
 * function ntfs_deallocate_clusters().
 */
int ntfs_deallocate_cluster_run(const ntfs_volume *vol,
		const ntfs_cluster_t lcn, const ntfs_cluster_t len)
{
	int err;

	lock_kernel();
	err = ntfs_clear_bitrange(vol->bitmap, lcn, len);
	unlock_kernel();
	return err;
}

/*
 * This is inefficient, but logically trivial, so will do for now. Note, we
 * do not touch the mft nor the data zones here because we want to minimize
 * recycling of clusters to enhance the chances of data being undeleteable.
 * Also we don't want the overhead. Instead we do one additional sweep of the
 * current data zone during cluster allocation to check for freed clusters.
 */
int ntfs_deallocate_clusters(const ntfs_volume *vol, const ntfs_runlist *rl,
		const int rl_len)
{
	int i, err;

	lock_kernel();
	for (i = err = 0; i < rl_len && !err; i++)
		err = ntfs_clear_bitrange(vol->bitmap, rl[i].lcn, rl[i].len);
	unlock_kernel();
	return err;
}

