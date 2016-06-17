/*
 *  symlink.c
 *
 *  Copyright (C) 2002 by John Newbigin
 *
 *  Please add a note about your changes to smbfs in the ChangeLog file.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>
#include <linux/net.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/smbno.h>
#include <linux/smb_fs.h>

#include "smb_debug.h"
#include "proto.h"

int smb_read_link(struct dentry *dentry, char *buffer, int len)
{
	char *link;
	int result;
	DEBUG1("read link buffer len = %d\n", len);

	result = -ENOMEM;
	link = kmalloc(SMB_MAXNAMELEN + 1, GFP_KERNEL);
	if (!link)
		goto out;

	result = smb_proc_read_link(server_from_dentry(dentry), dentry, link,
				    SMB_MAXNAMELEN);
	if (result < 0)
		goto out_free;
	result = vfs_readlink(dentry, buffer, len, link);

out_free:
	kfree(link);
out:
	return result;
}

int smb_symlink(struct inode *dir, struct dentry *dentry, const char *oldname)
{
	DEBUG1("create symlink %s -> %s/%s\n", oldname, DENTRY_PATH(dentry));

	smb_invalid_dir_cache(dir);
	return smb_proc_symlink(server_from_dentry(dentry), dentry, oldname);
}

int smb_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *link;
	int result;

	DEBUG1("followlink of %s/%s\n", DENTRY_PATH(dentry));

	result = -ENOMEM;
	link = kmalloc(SMB_MAXNAMELEN + 1, GFP_KERNEL);
	if (!link)
		goto out;

	result = smb_proc_read_link(server_from_dentry(dentry), dentry, link,
				    SMB_MAXNAMELEN);
	if (result < 0 || result >= SMB_MAXNAMELEN)
		goto out_free;
	link[result] = 0;

	result = vfs_follow_link(nd, link);

out_free:
	kfree(link);
out:
	return result;
}

struct inode_operations smb_link_inode_operations =
{
	.readlink	= smb_read_link,
	.follow_link	= smb_follow_link,
};
