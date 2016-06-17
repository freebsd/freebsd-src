/*
 *  fs/partitions/msdos.h
 */

#define MSDOS_LABEL_MAGIC		0xAA55

int msdos_partition(struct gendisk *hd, struct block_device *bdev,
		    unsigned long first_sector, int first_part_minor);

