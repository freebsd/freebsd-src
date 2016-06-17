/*
 * file.c
 *
 * PURPOSE
 *  File handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *  E-mail regarding any portion of the Linux UDF file system should be
 *  directed to the development team mailing list (run by majordomo):
 *    linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *  This file is distributed under the terms of the GNU General Public
 *  License (GPL). Copies of the GPL can be obtained from:
 *    ftp://prep.ai.mit.edu/pub/gnu/GPL
 *  Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-1999 Dave Boynton
 *  (C) 1998-2001 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  10/02/98 dgb  Attempt to integrate into udf.o
 *  10/07/98      Switched to using generic_readpage, etc., like isofs
 *                And it works!
 *  12/06/98 blf  Added udf_file_read. uses generic_file_read for all cases but
 *                ICBTAG_FLAG_AD_IN_ICB.
 *  04/06/99      64 bit file handling on 32 bit systems taken from ext2 file.c
 *  05/12/99      Preliminary file write support
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/udf_fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h> /* memset */
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>

#include "udf_i.h"
#include "udf_sb.h"

static int udf_adinicb_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;

	struct buffer_head *bh;
	int block;
	char *kaddr;
	int err = 0;

	if (!PageLocked(page))
		PAGE_BUG(page);

	kaddr = kmap(page);
	memset(kaddr, 0, PAGE_CACHE_SIZE);
	block = udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0);
	bh = sb_bread(inode->i_sb, block);
	if (!bh)
	{
		SetPageError(page);
		err = -EIO;
		goto out;
	}
	memcpy(kaddr, bh->b_data + udf_ext0_offset(inode), inode->i_size);
	brelse(bh);
	flush_dcache_page(page);
	SetPageUptodate(page);
out:
	kunmap(page);
	UnlockPage(page);
	return err;
}

static int udf_adinicb_writepage(struct page *page)
{
	struct inode *inode = page->mapping->host;

	struct buffer_head *bh;
	int block;
	char *kaddr;
	int err = 0;

	if (!PageLocked(page))
		PAGE_BUG(page);

	kaddr = kmap(page);
	block = udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0);
	bh = sb_bread(inode->i_sb, block);
	if (!bh)
	{
		SetPageError(page);
		err = -EIO;
		goto out;
	}
	memcpy(bh->b_data + udf_ext0_offset(inode), kaddr, inode->i_size);
	mark_buffer_dirty(bh);
	brelse(bh);
	SetPageUptodate(page);
out:
	kunmap(page);
	UnlockPage(page);
	return err;
}

static int udf_adinicb_prepare_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	kmap(page);
	return 0;
}

static int udf_adinicb_commit_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	struct inode *inode = page->mapping->host;

	struct buffer_head *bh;
	int block;
	char *kaddr = page_address(page);
	int err = 0;

	block = udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0);
	bh = sb_bread(inode->i_sb, block);
	if (!bh)
	{
		SetPageError(page);
		err = -EIO;
		goto out;
	}
	memcpy(bh->b_data + udf_file_entry_alloc_offset(inode) + offset,
		kaddr + offset, to - offset);
	mark_buffer_dirty(bh);
	brelse(bh);
	SetPageUptodate(page);
out:
	kunmap(page);
	/* only one page here */
	if (to > inode->i_size)
		inode->i_size = to;
	return err;
}

struct address_space_operations udf_adinicb_aops = {
	readpage:		udf_adinicb_readpage,
	writepage:		udf_adinicb_writepage,
	sync_page:		block_sync_page,
	prepare_write:		udf_adinicb_prepare_write,
	commit_write:		udf_adinicb_commit_write,
};

static ssize_t udf_file_write(struct file * file, const char * buf,
	size_t count, loff_t *ppos)
{
	ssize_t retval;
	struct inode *inode = file->f_dentry->d_inode;
	int err, pos;

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
	{
		if (file->f_flags & O_APPEND)
			pos = inode->i_size;
		else
			pos = *ppos;

		if (inode->i_sb->s_blocksize < (udf_file_entry_alloc_offset(inode) +
			pos + count))
		{
			udf_expand_file_adinicb(inode, pos + count, &err);
			if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
			{
				udf_debug("udf_expand_adinicb: err=%d\n", err);
				return err;
			}
		}
		else
		{
			if (pos + count > inode->i_size)
				UDF_I_LENALLOC(inode) = pos + count;
			else
				UDF_I_LENALLOC(inode) = inode->i_size;
		}
	}

	retval = generic_file_write(file, buf, count, ppos);

	if (retval > 0)
	{
		UDF_I_UCTIME(inode) = UDF_I_UMTIME(inode) = CURRENT_UTIME;
		mark_inode_dirty(inode);
	}
	return retval;
}

/*
 * udf_ioctl
 *
 * PURPOSE
 *	Issue an ioctl.
 *
 * DESCRIPTION
 *	Optional - sys_ioctl() will return -ENOTTY if this routine is not
 *	available, and the ioctl cannot be handled without filesystem help.
 *
 *	sys_ioctl() handles these ioctls that apply only to regular files:
 *		FIBMAP [requires udf_block_map()], FIGETBSZ, FIONREAD
 *	These ioctls are also handled by sys_ioctl():
 *		FIOCLEX, FIONCLEX, FIONBIO, FIOASYNC
 *	All other ioctls are passed to the filesystem.
 *
 *	Refer to sys_ioctl() in fs/ioctl.c
 *	sys_ioctl() -> .
 *
 * PRE-CONDITIONS
 *	inode			Pointer to inode that ioctl was issued on.
 *	filp			Pointer to file that ioctl was issued on.
 *	cmd			The ioctl command.
 *	arg			The ioctl argument [can be interpreted as a
 *				user-space pointer if desired].
 *
 * POST-CONDITIONS
 *	<return>		Success (>=0) or an error code (<=0) that
 *				sys_ioctl() will return.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int result = -EINVAL;
	struct buffer_head *bh = NULL;
	long_ad eaicb;
	uint8_t *ea = NULL;

	if ( permission(inode, MAY_READ) != 0 )
	{
		udf_debug("no permission to access inode %lu\n",
						inode->i_ino);
		return -EPERM;
	}

	if ( !arg )
	{
		udf_debug("invalid argument to udf_ioctl\n");
		return -EINVAL;
	}

	/* first, do ioctls that don't need to udf_read */
	switch (cmd)
	{
		case UDF_GETVOLIDENT:
			return copy_to_user((char *)arg,
				UDF_SB_VOLIDENT(inode->i_sb), 32) ? -EFAULT : 0;
		case UDF_RELOCATE_BLOCKS:
		{
			long old, new;

			if (!capable(CAP_SYS_ADMIN)) return -EACCES;
			if (get_user(old, (long *)arg)) return -EFAULT;
			if ((result = udf_relocate_blocks(inode->i_sb,
					old, &new)) == 0)
				result = put_user(new, (long *)arg);

			return result;
		}
	}

	/* ok, we need to read the inode */
	bh = udf_tread(inode->i_sb,
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0));

	if (!bh)
	{
		udf_debug("bread failed (inode=%ld)\n", inode->i_ino);
		return -EIO;
	}

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		struct fileEntry *fe;

		fe = (struct fileEntry *)bh->b_data;
		eaicb = lela_to_cpu(fe->extendedAttrICB);
		if (UDF_I_LENEATTR(inode))
			ea = fe->extendedAttr;
	}
	else
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)bh->b_data;
		eaicb = lela_to_cpu(efe->extendedAttrICB);
		if (UDF_I_LENEATTR(inode))
			ea = efe->extendedAttr;
	}

	switch (cmd) 
	{
		case UDF_GETEASIZE:
			result = put_user(UDF_I_LENEATTR(inode), (int *)arg);
			break;

		case UDF_GETEABLOCK:
			result = copy_to_user((char *)arg, ea,
				UDF_I_LENEATTR(inode)) ? -EFAULT : 0;
			break;
	}

	udf_release_data(bh);
	return result;
}

/*
 * udf_release_file
 *
 * PURPOSE
 *  Called when all references to the file are closed
 *
 * DESCRIPTION
 *  Discard prealloced blocks
 *
 * HISTORY
 *
 */
static int udf_release_file(struct inode * inode, struct file * filp)
{
	if (filp->f_mode & FMODE_WRITE)
	{
		lock_kernel();
		udf_discard_prealloc(inode);
		unlock_kernel();
	}
	return 0;
}

/*
 * udf_open_file
 *
 * PURPOSE
 *  Called when an inode is about to be open.
 *
 * DESCRIPTION
 *  Use this to disallow opening RW large files on 32 bit systems.
 *  On 64 bit systems we force on O_LARGEFILE in sys_open.
 *
 * HISTORY
 *
 */
static int udf_open_file(struct inode * inode, struct file * filp)
{
	if ((inode->i_size & 0xFFFFFFFF80000000ULL) && !(filp->f_flags & O_LARGEFILE))
		return -EFBIG;
	return 0;
}

struct file_operations udf_file_operations = {
	read:			generic_file_read,
	ioctl:			udf_ioctl,
	open:			udf_open_file,
	mmap:			generic_file_mmap,
	write:			udf_file_write,
	release:		udf_release_file,
	fsync:			udf_fsync_file,
};

struct inode_operations udf_file_inode_operations = {
	truncate:		udf_truncate,
};
