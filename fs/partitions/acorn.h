/*
 * fs/partitions/acorn.h
 *
 * Copyright (C) 1996-1998 Russell King
 */
#include <linux/adfs_fs.h>

/*
 * Partition types. (Oh for reusability)
 */
#define PARTITION_RISCIX_MFM	1
#define PARTITION_RISCIX_SCSI	2
#define PARTITION_LINUX		9

struct riscix_part {
	__u32  start;
	__u32  length;
	__u32  one;
	char name[16];
};

struct riscix_record {
	__u32  magic;
#define RISCIX_MAGIC	(0x4a657320)
	__u32  date;
	struct riscix_part part[8];
};

#define LINUX_NATIVE_MAGIC 0xdeafa1de
#define LINUX_SWAP_MAGIC   0xdeafab1e

struct linux_part {
	__u32 magic;
	__u32 start_sect;
	__u32 nr_sects;
};

struct ics_part {
	__u32 start;
	__s32 size;
};

struct ptec_partition {
	__u32 unused1;
	__u32 unused2;
	__u32 start;
	__u32 size;
	__u32 unused5;
	char type[8];
};
	

int acorn_partition(struct gendisk *hd, struct block_device *bdev,
		   unsigned long first_sect, int first_minor);

