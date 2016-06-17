/*
 *  fs/partitions/osf.c
 *
 *  Code extracted from drivers/block/genhd.c
 *
 *  Copyright (C) 1991-1998  Linus Torvalds
 *  Re-organised Feb 1998 Russell King
 */

#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/blk.h>

#include "check.h"
#include "osf.h"

int osf_partition(struct gendisk *hd, struct block_device *bdev,
		unsigned long first_sector, int current_minor)
{
	int i;
	Sector sect;
	unsigned char *data;
	int mask = (1 << hd->minor_shift) - 1;
	struct disklabel {
		u32 d_magic;
		u16 d_type,d_subtype;
		u8 d_typename[16];
		u8 d_packname[16];
		u32 d_secsize;
		u32 d_nsectors;
		u32 d_ntracks;
		u32 d_ncylinders;
		u32 d_secpercyl;
		u32 d_secprtunit;
		u16 d_sparespertrack;
		u16 d_sparespercyl;
		u32 d_acylinders;
		u16 d_rpm, d_interleave, d_trackskew, d_cylskew;
		u32 d_headswitch, d_trkseek, d_flags;
		u32 d_drivedata[5];
		u32 d_spare[5];
		u32 d_magic2;
		u16 d_checksum;
		u16 d_npartitions;
		u32 d_bbsize, d_sbsize;
		struct d_partition {
			u32 p_size;
			u32 p_offset;
			u32 p_fsize;
			u8  p_fstype;
			u8  p_frag;
			u16 p_cpg;
		} d_partitions[8];
	} * label;
	struct d_partition * partition;

	data = read_dev_sector(bdev, 0, &sect);
	if (!data)
		return -1;

	label = (struct disklabel *) (data+64);
	partition = label->d_partitions;
	if (le32_to_cpu(label->d_magic) != DISKLABELMAGIC) {
		put_dev_sector(sect);
		return 0;
	}
	if (le32_to_cpu(label->d_magic2) != DISKLABELMAGIC) {
		put_dev_sector(sect);
		return 0;
	}
	for (i = 0 ; i < le16_to_cpu(label->d_npartitions); i++, partition++) {
		if ((current_minor & mask) == 0)
		        break;
		if (le32_to_cpu(partition->p_size))
			add_gd_partition(hd, current_minor,
				first_sector+le32_to_cpu(partition->p_offset),
				le32_to_cpu(partition->p_size));
		current_minor++;
	}
	printk("\n");
	put_dev_sector(sect);
	return 1;
}

