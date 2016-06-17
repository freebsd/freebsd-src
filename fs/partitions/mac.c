/*
 *  fs/partitions/mac.c
 *
 *  Code extracted from drivers/block/genhd.c
 *  Copyright (C) 1991-1998  Linus Torvalds
 *  Re-organised Feb 1998 Russell King
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/blk.h>
#include <linux/ctype.h>

#include <asm/system.h>

#include "check.h"
#include "mac.h"

/*
 * Code to understand MacOS partition tables.
 */

int mac_partition(struct gendisk *hd, struct block_device *bdev,
		unsigned long fsec, int first_part_minor)
{
	Sector sect;
	unsigned char *data;
	int blk, blocks_in_map;
	unsigned secsize;
	struct mac_partition *part;
	struct mac_driver_desc *md;

	/* Get 0th block and look at the first partition map entry. */
	md = (struct mac_driver_desc *) read_dev_sector(bdev, 0, &sect);
	if (!md)
		return -1;
	if (be16_to_cpu(md->signature) != MAC_DRIVER_MAGIC) {
		put_dev_sector(sect);
		return 0;
	}
	secsize = be16_to_cpu(md->block_size);
	put_dev_sector(sect);
	data = read_dev_sector(bdev, secsize/512, &sect);
	if (!data)
		return -1;
	part = (struct mac_partition *) (data + secsize%512);
	if (be16_to_cpu(part->signature) != MAC_PARTITION_MAGIC) {
		put_dev_sector(sect);
		return 0;		/* not a MacOS disk */
	}
	printk(" [mac]");
	blocks_in_map = be32_to_cpu(part->map_count);
	for (blk = 1; blk <= blocks_in_map; ++blk) {
		int pos = blk * secsize;
		put_dev_sector(sect);
		data = read_dev_sector(bdev, pos/512, &sect);
		if (!data)
			return -1;
		part = (struct mac_partition *) (data + pos%512);
		if (be16_to_cpu(part->signature) != MAC_PARTITION_MAGIC)
			break;
		add_gd_partition(hd, first_part_minor,
			fsec + be32_to_cpu(part->start_block) * (secsize/512),
			be32_to_cpu(part->block_count) * (secsize/512));

		++first_part_minor;
	}

	put_dev_sector(sect);
	printk("\n");
	return 1;
}

