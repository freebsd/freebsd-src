/*
 *   Copyright (c) Christoph Hellwig, 2001-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/fs.h>
#include "jfs_incore.h"
#include "jfs_xattr.h"

static int jfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *s = JFS_IP(dentry->d_inode)->i_inline;
	return vfs_follow_link(nd, s);
}

static int jfs_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	char *s = JFS_IP(dentry->d_inode)->i_inline;
	return vfs_readlink(dentry, buffer, buflen, s);
}

struct inode_operations jfs_symlink_inode_operations = {
	.readlink	= jfs_readlink,
	.follow_link	= jfs_follow_link,
	.setxattr	= jfs_setxattr,
	.getxattr	= jfs_getxattr,
	.listxattr	= jfs_listxattr,
	.removexattr	= jfs_removexattr,
};

