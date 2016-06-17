/*
 *   Copyright (c) International Business Machines Corp., 2000-2002
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
#include "jfs_filsys.h"
#include "jfs_imap.h"
#include "jfs_dinode.h"
#include "jfs_debug.h"

kmem_cache_t *jfs_inode_cachep;

/*
 * NAME:	ialloc()
 *
 * FUNCTION:	Allocate a new inode
 *
 */
struct inode *ialloc(struct inode *parent, umode_t mode)
{
	struct super_block *sb = parent->i_sb;
	struct inode *inode;
	struct jfs_inode_info *jfs_inode;
	int rc;

	inode = new_inode(sb);
	if (!inode) {
		jfs_warn("ialloc: new_inode returned NULL!");
		return inode;
	}

	rc = alloc_jfs_inode(inode);
	if (rc) {
		make_bad_inode(inode);
		iput(inode);
		return NULL;
	}
	jfs_inode = JFS_IP(inode);

	rc = diAlloc(parent, S_ISDIR(mode), inode);
	if (rc) {
		jfs_warn("ialloc: diAlloc returned %d!", rc);
		free_jfs_inode(inode);
		make_bad_inode(inode);
		iput(inode);
		return NULL;
	}

	inode->i_uid = current->fsuid;
	if (parent->i_mode & S_ISGID) {
		inode->i_gid = parent->i_gid;
		if (S_ISDIR(mode))
			mode |= S_ISGID;
	} else
		inode->i_gid = current->fsgid;

	inode->i_mode = mode;
	if (S_ISDIR(mode))
		jfs_inode->mode2 = IDIRECTORY | mode;
	else
		jfs_inode->mode2 = INLINEEA | ISPARSE | mode;
	inode->i_blksize = sb->s_blocksize;
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	jfs_inode->otime = inode->i_ctime;
	inode->i_generation = JFS_SBI(sb)->gengen++;

	jfs_inode->cflag = 0;
	set_cflag(COMMIT_New, inode);

	/* Zero remaining fields */
	memset(&jfs_inode->acl, 0, sizeof(dxd_t));
	memset(&jfs_inode->ea, 0, sizeof(dxd_t));
	jfs_inode->next_index = 0;
	jfs_inode->acltype = 0;
	jfs_inode->btorder = 0;
	jfs_inode->btindex = 0;
	jfs_inode->bxflag = 0;
	jfs_inode->blid = 0;
	jfs_inode->atlhead = 0;
	jfs_inode->atltail = 0;
	jfs_inode->xtlid = 0;

	jfs_info("ialloc returns inode = 0x%p\n", inode);

	return inode;
}

/*
 * NAME:	alloc_jfs_inode()
 *
 * FUNCTION:	Allocate jfs portion of in-memory inode
 *
 */
int alloc_jfs_inode(struct inode *inode)
{
	struct jfs_inode_info *jfs_inode;

	jfs_inode = kmem_cache_alloc(jfs_inode_cachep, GFP_NOFS);
	inode->u.generic_ip = jfs_inode;
	if (!jfs_inode)
		return -ENOSPC;
	jfs_inode->inode = inode;

	return 0;
}
