#ifndef _MINIX_FS_I
#define _MINIX_FS_I

/*
 * minix fs inode data in memory
 */
struct minix_inode_info {
	union {
		__u16 i1_data[16];
		__u32 i2_data[16];
	} u;
};

#endif
