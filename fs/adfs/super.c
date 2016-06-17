/*
 *  linux/fs/adfs/super.c
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/adfs_fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/init.h>

#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <stdarg.h>

#include "adfs.h"
#include "dir_f.h"
#include "dir_fplus.h"

void __adfs_error(struct super_block *sb, const char *function, const char *fmt, ...)
{
	char error_buf[128];
	va_list args;

	va_start(args, fmt);
	vsprintf(error_buf, fmt, args);
	va_end(args);

	printk(KERN_CRIT "ADFS-fs error (device %s)%s%s: %s\n",
		bdevname(sb->s_dev), function ? ": " : "",
		function ? function : "", error_buf);
}

static int adfs_checkdiscrecord(struct adfs_discrecord *dr)
{
	int i;

	/* sector size must be 256, 512 or 1024 bytes */
	if (dr->log2secsize != 8 &&
	    dr->log2secsize != 9 &&
	    dr->log2secsize != 10)
		return 1;

	/* idlen must be at least log2secsize + 3 */
	if (dr->idlen < dr->log2secsize + 3)
		return 1;

	/* we cannot have such a large disc that we
	 * are unable to represent sector offsets in
	 * 32 bits.  This works out at 2.0 TB.
	 */
	if (dr->disc_size_high >> dr->log2secsize)
		return 1;

	/*
	 * The following checks are not required for F+
	 * stage 1.
	 */
#if 0
	/* idlen must be smaller be no greater than 15 */
	if (dr->idlen > 15)
		return 1;

	/* nzones must be less than 128 for the root
	 * directory to be addressable
	 */
	if (dr->nzones >= 128 && dr->nzones_high == 0)
		return 1;

	/* root must be of the form 0x2.. */
	if ((le32_to_cpu(dr->root) & 0xffffff00) != 0x00000200)
		return 1;
#else
	/*
	 * Stage 2 F+ does not require the following check
	 */
#if 0
	/* idlen must be no greater than 16 v2 [1.0] */
	if (dr->idlen > 16)
		return 1;

	/* we can't handle F+ discs yet */
	if (dr->format_version || dr->root_size)
		return 1;

#else
	/* idlen must be no greater than 19 v2 [1.0] */
	if (dr->idlen > 19)
		return 1;
#endif
#endif

	/* reserved bytes should be zero */
	for (i = 0; i < sizeof(dr->unused52); i++)
		if (dr->unused52[i] != 0)
			return 1;

	return 0;
}

static unsigned char adfs_calczonecheck(struct super_block *sb, unsigned char *map)
{
	unsigned int v0, v1, v2, v3;
	int i;

	v0 = v1 = v2 = v3 = 0;
	for (i = sb->s_blocksize - 4; i; i -= 4) {
		v0 += map[i]     + (v3 >> 8);
		v3 &= 0xff;
		v1 += map[i + 1] + (v0 >> 8);
		v0 &= 0xff;
		v2 += map[i + 2] + (v1 >> 8);
		v1 &= 0xff;
		v3 += map[i + 3] + (v2 >> 8);
		v2 &= 0xff;
	}
	v0 +=           v3 >> 8;
	v1 += map[1] + (v0 >> 8);
	v2 += map[2] + (v1 >> 8);
	v3 += map[3] + (v2 >> 8);

	return v0 ^ v1 ^ v2 ^ v3;
}

static int adfs_checkmap(struct super_block *sb, struct adfs_discmap *dm)
{
	unsigned char crosscheck = 0, zonecheck = 1;
	int i;

	for (i = 0; i < sb->u.adfs_sb.s_map_size; i++) {
		unsigned char *map;

		map = dm[i].dm_bh->b_data;

		if (adfs_calczonecheck(sb, map) != map[0]) {
			adfs_error(sb, "zone %d fails zonecheck", i);
			zonecheck = 0;
		}
		crosscheck ^= map[3];
	}
	if (crosscheck != 0xff)
		adfs_error(sb, "crosscheck != 0xff");
	return crosscheck == 0xff && zonecheck;
}

static void adfs_put_super(struct super_block *sb)
{
	int i;

	for (i = 0; i < sb->u.adfs_sb.s_map_size; i++)
		brelse(sb->u.adfs_sb.s_map[i].dm_bh);
	kfree(sb->u.adfs_sb.s_map);
}

static int parse_options(struct super_block *sb, char *options)
{
	char *value, *opt;

	if (!options)
		return 0;

	for (opt = strtok(options, ","); opt != NULL; opt = strtok(NULL, ",")) {
		value = strchr(opt, '=');
		if (value)
			*value++ = '\0';

		if (!strcmp(opt, "uid")) {	/* owner of all files */
			if (!value || !*value)
				return -EINVAL;
			sb->u.adfs_sb.s_uid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		} else
		if (!strcmp(opt, "gid")) {	/* group owner of all files */
			if (!value || !*value)
				return -EINVAL;
			sb->u.adfs_sb.s_gid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		} else
		if (!strcmp(opt, "ownmask")) {	/* owner permission mask */
			if (!value || !*value)
				return -EINVAL;
			sb->u.adfs_sb.s_owner_mask = simple_strtoul(value, &value, 8);
			if (*value)
				return -EINVAL;
		} else
		if (!strcmp(opt, "othmask")) {	/* others permission mask */
			if (!value || !*value)
				return -EINVAL;
			sb->u.adfs_sb.s_other_mask = simple_strtoul(value, &value, 8);
			if (*value)
				return -EINVAL;
		} else {			/* eh? say again. */
			printk("ADFS-fs: unrecognised mount option %s\n", opt);
			return -EINVAL;
		}
	}
	return 0;
}

static int adfs_remount(struct super_block *sb, int *flags, char *data)
{
	return parse_options(sb, data);
}

static int adfs_statfs(struct super_block *sb, struct statfs *buf)
{
	struct adfs_sb_info *asb = &sb->u.adfs_sb;

	buf->f_type    = ADFS_SUPER_MAGIC;
	buf->f_namelen = asb->s_namelen;
	buf->f_bsize   = sb->s_blocksize;
	buf->f_blocks  = asb->s_size;
	buf->f_files   = asb->s_ids_per_zone * asb->s_map_size;
	buf->f_bavail  =
	buf->f_bfree   = adfs_map_free(sb);
	buf->f_ffree   = buf->f_bfree * buf->f_files / buf->f_blocks;

	return 0;
}

static struct super_operations adfs_sops = {
	write_inode:	adfs_write_inode,
	put_super:	adfs_put_super,
	statfs:		adfs_statfs,
	remount_fs:	adfs_remount,
};

static struct adfs_discmap *adfs_read_map(struct super_block *sb, struct adfs_discrecord *dr)
{
	struct adfs_discmap *dm;
	unsigned int map_addr, zone_size, nzones;
	int i, zone;

	nzones    = sb->u.adfs_sb.s_map_size;
	zone_size = (8 << dr->log2secsize) - le16_to_cpu(dr->zone_spare);
	map_addr  = (nzones >> 1) * zone_size -
		     ((nzones > 1) ? ADFS_DR_SIZE_BITS : 0);
	map_addr  = signed_asl(map_addr, sb->u.adfs_sb.s_map2blk);

	sb->u.adfs_sb.s_ids_per_zone = zone_size / (sb->u.adfs_sb.s_idlen + 1);

	dm = kmalloc(nzones * sizeof(*dm), GFP_KERNEL);
	if (dm == NULL) {
		adfs_error(sb, "not enough memory");
		return NULL;
	}

	for (zone = 0; zone < nzones; zone++, map_addr++) {
		dm[zone].dm_startbit = 0;
		dm[zone].dm_endbit   = zone_size;
		dm[zone].dm_startblk = zone * zone_size - ADFS_DR_SIZE_BITS;
		dm[zone].dm_bh       = sb_bread(sb, map_addr);

		if (!dm[zone].dm_bh) {
			adfs_error(sb, "unable to read map");
			goto error_free;
		}
	}

	/* adjust the limits for the first and last map zones */
	i = zone - 1;
	dm[0].dm_startblk = 0;
	dm[0].dm_startbit = ADFS_DR_SIZE_BITS;
	dm[i].dm_endbit   = (dr->disc_size_high << (32 - dr->log2bpmb)) +
			    (dr->disc_size >> dr->log2bpmb) +
			    (ADFS_DR_SIZE_BITS - i * zone_size);

	if (adfs_checkmap(sb, dm))
		return dm;

	adfs_error(sb, NULL, "map corrupted");

error_free:
	while (--zone >= 0)
		brelse(dm[zone].dm_bh);

	kfree(dm);
	return NULL;
}

static inline unsigned long adfs_discsize(struct adfs_discrecord *dr, int block_bits)
{
	unsigned long discsize;

	discsize  = le32_to_cpu(dr->disc_size_high) << (32 - block_bits);
	discsize |= le32_to_cpu(dr->disc_size) >> block_bits;

	return discsize;
}

struct super_block *adfs_read_super(struct super_block *sb, void *data, int silent)
{
	struct adfs_discrecord *dr;
	struct buffer_head *bh;
	struct object_info root_obj;
	unsigned char *b_data;
	kdev_t dev = sb->s_dev;

	/* set default options */
	sb->u.adfs_sb.s_uid = 0;
	sb->u.adfs_sb.s_gid = 0;
	sb->u.adfs_sb.s_owner_mask = S_IRWXU;
	sb->u.adfs_sb.s_other_mask = S_IRWXG | S_IRWXO;

	if (parse_options(sb, data))
		goto error;

	sb->s_blocksize = BLOCK_SIZE;
	set_blocksize(dev, BLOCK_SIZE);
	if (!(bh = sb_bread(sb, ADFS_DISCRECORD / BLOCK_SIZE))) {
		adfs_error(sb, "unable to read superblock");
		goto error;
	}

	b_data = bh->b_data + (ADFS_DISCRECORD % BLOCK_SIZE);

	if (adfs_checkbblk(b_data)) {
		if (!silent)
			printk("VFS: Can't find an adfs filesystem on dev "
				"%s.\n", bdevname(dev));
		goto error_free_bh;
	}

	dr = (struct adfs_discrecord *)(b_data + ADFS_DR_OFFSET);

	/*
	 * Do some sanity checks on the ADFS disc record
	 */
	if (adfs_checkdiscrecord(dr)) {
		if (!silent)
			printk("VPS: Can't find an adfs filesystem on dev "
				"%s.\n", bdevname(dev));
		goto error_free_bh;
	}

	sb->s_blocksize_bits = dr->log2secsize;
	sb->s_blocksize = 1 << sb->s_blocksize_bits;
	if (sb->s_blocksize != BLOCK_SIZE &&
	    (sb->s_blocksize == 512 || sb->s_blocksize == 1024 ||
	     sb->s_blocksize == 2048 || sb->s_blocksize == 4096)) {

		brelse(bh);
		set_blocksize(dev, sb->s_blocksize);
		bh = sb_bread(sb, ADFS_DISCRECORD / sb->s_blocksize);
		if (!bh) {
			adfs_error(sb, "couldn't read superblock on "
				"2nd try.");
			goto error;
		}
		b_data = bh->b_data + (ADFS_DISCRECORD % sb->s_blocksize);
		if (adfs_checkbblk(b_data)) {
			adfs_error(sb, "disc record mismatch, very weird!");
			goto error_free_bh;
		}
		dr = (struct adfs_discrecord *)(b_data + ADFS_DR_OFFSET);
	}
	if (sb->s_blocksize != bh->b_size) {
		if (!silent)
			printk(KERN_ERR "VFS: Unsupported blocksize on dev "
				"%s.\n", bdevname(dev));
		goto error_free_bh;
	}

	/*
	 * blocksize on this device should now be set to the ADFS log2secsize
	 */

	sb->s_magic		 = ADFS_SUPER_MAGIC;
	sb->u.adfs_sb.s_idlen	 = dr->idlen;
	sb->u.adfs_sb.s_map_size = dr->nzones | (dr->nzones_high << 8);
	sb->u.adfs_sb.s_map2blk	 = dr->log2bpmb - dr->log2secsize;
	sb->u.adfs_sb.s_size     = adfs_discsize(dr, sb->s_blocksize_bits);
	sb->u.adfs_sb.s_version  = dr->format_version;
	sb->u.adfs_sb.s_log2sharesize = dr->log2sharesize;
	
	sb->u.adfs_sb.s_map = adfs_read_map(sb, dr);
	if (!sb->u.adfs_sb.s_map)
		goto error_free_bh;

	brelse(bh);

	/*
	 * set up enough so that we can read an inode
	 */
	sb->s_op = &adfs_sops;

	dr = (struct adfs_discrecord *)(sb->u.adfs_sb.s_map[0].dm_bh->b_data + 4);

	root_obj.parent_id = root_obj.file_id = le32_to_cpu(dr->root);
	root_obj.name_len  = 0;
	root_obj.loadaddr  = 0;
	root_obj.execaddr  = 0;
	root_obj.size	   = ADFS_NEWDIR_SIZE;
	root_obj.attr	   = ADFS_NDA_DIRECTORY   | ADFS_NDA_OWNER_READ |
			     ADFS_NDA_OWNER_WRITE | ADFS_NDA_PUBLIC_READ;

	/*
	 * If this is a F+ disk with variable length directories,
	 * get the root_size from the disc record.
	 */
	if (sb->u.adfs_sb.s_version) {
		root_obj.size = dr->root_size;
		sb->u.adfs_sb.s_dir     = &adfs_fplus_dir_ops;
		sb->u.adfs_sb.s_namelen = ADFS_FPLUS_NAME_LEN;
	} else {
		sb->u.adfs_sb.s_dir     = &adfs_f_dir_ops;
		sb->u.adfs_sb.s_namelen = ADFS_F_NAME_LEN;
	}

	sb->s_root = d_alloc_root(adfs_iget(sb, &root_obj));
	if (!sb->s_root) {
		int i;

		for (i = 0; i < sb->u.adfs_sb.s_map_size; i++)
			brelse(sb->u.adfs_sb.s_map[i].dm_bh);
		kfree(sb->u.adfs_sb.s_map);
		adfs_error(sb, "get root inode failed\n");
		goto error;
	} else
		sb->s_root->d_op = &adfs_dentry_operations;
	return sb;

error_free_bh:
	brelse(bh);
error:
	return NULL;
}

static DECLARE_FSTYPE_DEV(adfs_fs_type, "adfs", adfs_read_super);

static int __init init_adfs_fs(void)
{
	return register_filesystem(&adfs_fs_type);
}

static void __exit exit_adfs_fs(void)
{
	unregister_filesystem(&adfs_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_adfs_fs)
module_exit(exit_adfs_fs)
