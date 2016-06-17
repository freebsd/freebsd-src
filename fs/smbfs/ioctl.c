/*
 *  ioctl.c
 *
 *  Copyright (C) 1995, 1996 by Volker Lendecke
 *  Copyright (C) 1997 by Volker Lendecke
 *
 *  Please add a note about your changes to smbfs in the ChangeLog file.
 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/highuid.h>

#include <linux/smb_fs.h>
#include <linux/smb_mount.h>

#include <asm/uaccess.h>

#include "proto.h"

int
smb_ioctl(struct inode *inode, struct file *filp,
	  unsigned int cmd, unsigned long arg)
{
	struct smb_sb_info *server = server_from_inode(inode);
	struct smb_conn_opt opt;
	int result = -EINVAL;

	switch (cmd) {
	case SMB_IOC_GETMOUNTUID:
		result = put_user(NEW_TO_OLD_UID(server->mnt->mounted_uid),
				  (uid16_t *) arg);
		break;
	case SMB_IOC_GETMOUNTUID32:
		result = put_user(server->mnt->mounted_uid, (uid_t *) arg);
		break;

	case SMB_IOC_NEWCONN:
		/* arg is smb_conn_opt, or NULL if no connection was made */
		if (!arg) {
			result = smb_wakeup(server);
			break;
		}

		result = -EFAULT;
		if (!copy_from_user(&opt, (void *)arg, sizeof(opt)))
			result = smb_newconn(server, &opt);
		break;
	default:
		break;
	}

	return result;
}
