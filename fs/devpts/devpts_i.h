/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/fs/devpts/devpts_i.h
 *
 *  Copyright 1998 H. Peter Anvin -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/types.h>

#define BUILDING_DEVPTS 1
#include <linux/devpts_fs.h>

struct devpts_sb_info {
	u32 magic;
	int setuid;
	int setgid;
	uid_t   uid;
	gid_t   gid;
	umode_t mode;

	unsigned int max_ptys;
	struct inode **inodes;
};

#define DEVPTS_SUPER_MAGIC 0x1cd1
#define DEVPTS_SBI_MAGIC   0x01da1d02

extern inline struct devpts_sb_info *SBI(struct super_block *sb)
{
	return (struct devpts_sb_info *)(sb->u.generic_sbp);
}

extern struct inode_operations devpts_root_inode_operations;
extern struct file_operations devpts_root_operations;
