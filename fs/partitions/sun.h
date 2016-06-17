/*
 *  fs/partitions/sun.h
 */

#define SUN_LABEL_MAGIC          0xDABE

int sun_partition(struct gendisk *hd, struct block_device *bdev,
		  unsigned long first_sector, int first_part_minor);

