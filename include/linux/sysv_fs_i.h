#ifndef _SYSV_FS_I
#define _SYSV_FS_I

/*
 * SystemV/V7/Coherent FS inode data in memory
 */
struct sysv_inode_info {
	u32 i_data[10+1+1+1];	/* zone numbers: max. 10 data blocks,
				 * then 1 indirection block,
				 * then 1 double indirection block,
				 * then 1 triple indirection block.
				 */
	u32 i_dir_start_lookup;
};

#endif

