/*
 *	include/linux/bfs_fs_sb.h
 *	Copyright (C) 1999 Tigran Aivazian <tigran@veritas.com>
 */

#ifndef _LINUX_BFS_FS_SB
#define _LINUX_BFS_FS_SB

/*
 * BFS file system in-core superblock info
 */
struct bfs_sb_info {
	unsigned long si_blocks;
	unsigned long si_freeb;
	unsigned long si_freei;
	unsigned long si_lf_ioff;
	unsigned long si_lf_sblk;
	unsigned long si_lf_eblk;
	unsigned long si_lasti;
	char * si_imap;
	struct buffer_head * si_sbh;		/* buffer header w/superblock */
	struct bfs_super_block * si_bfs_sb;	/* superblock in si_sbh->b_data */
};

#endif	/* _LINUX_BFS_FS_SB */
