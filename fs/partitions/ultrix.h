/*
 *  fs/partitions/ultrix.h
 */

int ultrix_partition(struct gendisk *hd, struct block_device *bdev,
                     unsigned long first_sector, int first_part_minor);

