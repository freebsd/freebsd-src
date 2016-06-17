/*
 * Copyright (c) 2000-2001 Christoph Hellwig.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ident "$Id: vxfs_immed.c,v 1.10 2001/04/25 18:11:23 hch Exp hch $"

/*
 * Veritas filesystem driver - support for 'immed' inodes.
 */
#include <linux/fs.h>
#include <linux/pagemap.h>

#include "vxfs.h"
#include "vxfs_inode.h"


static int	vxfs_immed_readlink(struct dentry *, char *, int);
static int	vxfs_immed_follow_link(struct dentry *, struct nameidata *);

static int	vxfs_immed_readpage(struct file *, struct page *);

/*
 * Inode operations for immed symlinks.
 *
 * Unliked all other operations we do not go through the pagecache,
 * but do all work directly on the inode.
 */
struct inode_operations vxfs_immed_symlink_iops = {
	.readlink =		vxfs_immed_readlink,
	.follow_link =		vxfs_immed_follow_link,
};

/*
 * Adress space operations for immed files and directories.
 */
struct address_space_operations vxfs_immed_aops = {
	.readpage =		vxfs_immed_readpage,
};


/**
 * vxfs_immed_readlink - read immed symlink
 * @dp:		dentry for the link
 * @bp:		output buffer
 * @buflen:	length of @bp
 *
 * Description:
 *   vxfs_immed_readlink calls vfs_readlink to read the link
 *   described by @dp into userspace.
 *
 * Returns:
 *   Number of bytes successfully copied to userspace.
 */
static int
vxfs_immed_readlink(struct dentry *dp, char *bp, int buflen)
{
	struct vxfs_inode_info		*vip = VXFS_INO(dp->d_inode);

	return (vfs_readlink(dp, bp, buflen, vip->vii_immed.vi_immed));
}

/**
 * vxfs_immed_follow_link - follow immed symlink
 * @dp:		dentry for the link
 * @np:		pathname lookup data for the current path walk
 *
 * Description:
 *   vxfs_immed_follow_link restarts the pathname lookup with
 *   the data obtained from @dp.
 *
 * Returns:
 *   Zero on success, else a negative error code.
 */
static int
vxfs_immed_follow_link(struct dentry *dp, struct nameidata *np)
{
	struct vxfs_inode_info		*vip = VXFS_INO(dp->d_inode);

	return (vfs_follow_link(np, vip->vii_immed.vi_immed));
}

/**
 * vxfs_immed_readpage - read part of an immed inode into pagecache
 * @file:	file context (unused)
 * @page:	page frame to fill in.
 *
 * Description:
 *   vxfs_immed_readpage reads a part of the immed area of the
 *   file that hosts @pp into the pagecache.
 *
 * Returns:
 *   Zero on success, else a negative error code.
 *
 * Locking status:
 *   @page is locked and will be unlocked.
 */
static int
vxfs_immed_readpage(struct file *fp, struct page *pp)
{
	struct vxfs_inode_info	*vip = VXFS_INO(pp->mapping->host);
	u_int64_t		offset = pp->index << PAGE_CACHE_SHIFT;
	caddr_t			kaddr;

	kaddr = kmap(pp);
	memcpy(kaddr, vip->vii_immed.vi_immed + offset, PAGE_CACHE_SIZE);
	kunmap(pp);
	
	flush_dcache_page(pp);
	SetPageUptodate(pp);
        UnlockPage(pp);

	return 0;
}
