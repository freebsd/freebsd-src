/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/fs/autofs/dir.c
 *
 *  Copyright 1997-1998 Transmeta Corporation -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include "autofs_i.h"

/*
 * No entries except for "." and "..", both of which are handled by the VFS
 * layer. So all children are negative and dcache-based versions of operations
 * are OK.
 */
static struct dentry *autofs_dir_lookup(struct inode *dir,struct dentry *dentry)
{
	d_add(dentry, NULL);
	return NULL;
}

struct inode_operations autofs_dir_inode_operations = {
	lookup:		autofs_dir_lookup,
};

