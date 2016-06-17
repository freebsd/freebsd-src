/*
 *  fs/partitions/mac.h
 */

#define MAC_PARTITION_MAGIC	0x504d

/* type field value for A/UX or other Unix partitions */
#define APPLE_AUX_TYPE	"Apple_UNIX_SVR2"

struct mac_partition {
	__u16	signature;	/* expected to be MAC_PARTITION_MAGIC */
	__u16	res1;
	__u32	map_count;	/* # blocks in partition map */
	__u32	start_block;	/* absolute starting block # of partition */
	__u32	block_count;	/* number of blocks in partition */
	char	name[32];	/* partition name */
	char	type[32];	/* string type description */
	__u32	data_start;	/* rel block # of first data block */
	__u32	data_count;	/* number of data blocks */
	__u32	status;		/* partition status bits */
	__u32	boot_start;
	__u32	boot_size;
	__u32	boot_load;
	__u32	boot_load2;
	__u32	boot_entry;
	__u32	boot_entry2;
	__u32	boot_cksum;
	char	processor[16];	/* identifies ISA of boot */
	/* there is more stuff after this that we don't need */
};

static inline void macpart_fix_string(char *stg, int len)
{
	int i;

	for (i = len - 1; i >= 0 && stg[i] == ' '; i--)
		stg[i] = 0;
}

#define MAC_STATUS_BOOTABLE	8	/* partition is bootable */

#define MAC_DRIVER_MAGIC	0x4552

/* Driver descriptor structure, in block 0 */
struct mac_driver_desc {
	__u16	signature;	/* expected to be MAC_DRIVER_MAGIC */
	__u16	block_size;
	__u32	block_count;
    /* ... more stuff */
};

int mac_partition(struct gendisk *hd, struct block_device *bdev, unsigned long fsec, int first_part_minor);
