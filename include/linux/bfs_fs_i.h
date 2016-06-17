/*
 *	include/linux/bfs_fs_i.h
 *	Copyright (C) 1999 Tigran Aivazian <tigran@veritas.com>
 */

#ifndef _LINUX_BFS_FS_I
#define _LINUX_BFS_FS_I

/*
 * BFS file system in-core inode info
 */
struct bfs_inode_info {
	unsigned long i_dsk_ino; /* inode number from the disk, can be 0 */
	unsigned long i_sblock;
	unsigned long i_eblock;
};

#endif	/* _LINUX_BFS_FS_I */
