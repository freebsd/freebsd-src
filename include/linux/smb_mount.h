/*
 *  smb_mount.h
 *
 *  Copyright (C) 1995, 1996 by Paal-Kr. Engstad and Volker Lendecke
 *  Copyright (C) 1997 by Volker Lendecke
 *
 */

#ifndef _LINUX_SMB_MOUNT_H
#define _LINUX_SMB_MOUNT_H

#include <linux/types.h>

#define SMB_MOUNT_VERSION	6

struct smb_mount_data {
	int version;
	__kernel_uid_t mounted_uid; /* Who may umount() this filesystem? */
	__kernel_uid_t uid;
	__kernel_gid_t gid;
	__kernel_mode_t file_mode;
	__kernel_mode_t dir_mode;
};


#ifdef __KERNEL__

/* "vers" in big-endian */
#define SMB_MOUNT_ASCII 0x76657273

#define SMB_MOUNT_OLDVERSION	6
#undef SMB_MOUNT_VERSION
#define SMB_MOUNT_VERSION	7

/* flags */
#define SMB_MOUNT_WIN95		0x0001	/* Win 95 server */
#define SMB_MOUNT_OLDATTR	0x0002	/* Use core getattr (Win 95 speedup) */
#define SMB_MOUNT_DIRATTR	0x0004	/* Use find_first for getattr */
#define SMB_MOUNT_CASE		0x0008	/* Be case sensitive */


struct smb_mount_data_kernel {
	int version;

	uid_t mounted_uid;	/* Who may umount() this filesystem? */
	__kernel_uid_t uid;
	__kernel_gid_t gid;
	__kernel_mode_t file_mode;
	__kernel_mode_t dir_mode;

	u32 flags;

	int ttl;	/* maximum age in jiffies (inode, dentry & dircache) */
	int timeo;	/* timeout for requests */

	struct smb_nls_codepage codepage;
};

#endif

#endif
