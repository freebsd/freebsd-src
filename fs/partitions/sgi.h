/*
 *  fs/partitions/sgi.h
 */

extern int sgi_partition(struct gendisk *hd, struct block_device *bdev,
	 unsigned long first_sector, int first_part_minor);

#define SGI_LABEL_MAGIC 0x0be5a941

