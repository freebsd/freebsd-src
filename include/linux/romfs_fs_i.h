#ifndef __ROMFS_FS_I
#define __ROMFS_FS_I

/* inode in-kernel data */

struct romfs_inode_info {
	unsigned long i_metasize;	/* size of non-data area */
	unsigned long i_dataoffset;	/* from the start of fs */
};

#endif
