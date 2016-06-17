/*
 *  ncp_fs_i.h
 *
 *  Copyright (C) 1995 Volker Lendecke
 *
 */

#ifndef _LINUX_NCP_FS_I
#define _LINUX_NCP_FS_I

#ifdef __KERNEL__

/*
 * This is the ncpfs part of the inode structure. This must contain
 * all the information we need to work with an inode after creation.
 */
struct ncp_inode_info {
	__u32	dirEntNum __attribute__((packed));
	__u32	DosDirNum __attribute__((packed));
	__u32	volNumber __attribute__((packed));
	__u32	nwattr;
	struct semaphore open_sem;
	atomic_t	opened;
	int	access;
	__u32	server_file_handle __attribute__((packed));
	__u8	open_create_action __attribute__((packed));
	__u8	file_handle[6] __attribute__((packed));
};

#endif	/* __KERNEL__ */

#endif	/* _LINUX_NCP_FS_I */
