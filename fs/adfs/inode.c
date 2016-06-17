/*
 *  linux/fs/adfs/inode.c
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/adfs_fs.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/module.h>


#include "adfs.h"

/*
 * Lookup/Create a block at offset 'block' into 'inode'.  We currently do
 * not support creation of new blocks, so we return -EIO for this case.
 */
int
adfs_get_block(struct inode *inode, long block, struct buffer_head *bh, int create)
{
	if (block < 0)
		goto abort_negative;

	if (!create) {
		if (block >= inode->i_blocks)
			goto abort_toobig;

		block = __adfs_block_map(inode->i_sb, inode->i_ino, block);
		if (block) {
			bh->b_dev = inode->i_dev;
			bh->b_blocknr = block;
			bh->b_state |= (1UL << BH_Mapped);
		}
		return 0;
	}
	/* don't support allocation of blocks yet */
	return -EIO;

abort_negative:
	adfs_error(inode->i_sb, "block %d < 0", block);
	return -EIO;

abort_toobig:
	return 0;
}

static int adfs_writepage(struct page *page)
{
	return block_write_full_page(page, adfs_get_block);
}

static int adfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, adfs_get_block);
}

static int adfs_prepare_write(struct file *file, struct page *page, unsigned int from, unsigned int to)
{
	return cont_prepare_write(page, from, to, adfs_get_block,
		&page->mapping->host->u.adfs_i.mmu_private);
}

static int _adfs_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping, block, adfs_get_block);
}

static struct address_space_operations adfs_aops = {
	readpage:	adfs_readpage,
	writepage:	adfs_writepage,
	sync_page:	block_sync_page,
	prepare_write:	adfs_prepare_write,
	commit_write:	generic_commit_write,
	bmap:		_adfs_bmap
};

static inline unsigned int
adfs_filetype(struct inode *inode)
{
	unsigned int type;

	if (inode->u.adfs_i.stamped)
		type = (inode->u.adfs_i.loadaddr >> 8) & 0xfff;
	else
		type = (unsigned int) -1;

	return type;
}

/*
 * Convert ADFS attributes and filetype to Linux permission.
 */
static umode_t
adfs_atts2mode(struct super_block *sb, struct inode *inode)
{
	unsigned int filetype, attr = inode->u.adfs_i.attr;
	umode_t mode, rmask;

	if (attr & ADFS_NDA_DIRECTORY) {
		mode = S_IRUGO & sb->u.adfs_sb.s_owner_mask;
		return S_IFDIR | S_IXUGO | mode;
	}

	filetype = adfs_filetype(inode);

	switch (filetype) {
	case 0xfc0:	/* LinkFS */
		return S_IFLNK|S_IRWXUGO;

	case 0xfe6:	/* UnixExec */
		rmask = S_IRUGO | S_IXUGO;
		break;

	default:
		rmask = S_IRUGO;
	}

	mode = S_IFREG;

	if (attr & ADFS_NDA_OWNER_READ)
		mode |= rmask & sb->u.adfs_sb.s_owner_mask;

	if (attr & ADFS_NDA_OWNER_WRITE)
		mode |= S_IWUGO & sb->u.adfs_sb.s_owner_mask;

	if (attr & ADFS_NDA_PUBLIC_READ)
		mode |= rmask & sb->u.adfs_sb.s_other_mask;

	if (attr & ADFS_NDA_PUBLIC_WRITE)
		mode |= S_IWUGO & sb->u.adfs_sb.s_other_mask;
	return mode;
}

/*
 * Convert Linux permission to ADFS attribute.  We try to do the reverse
 * of atts2mode, but there is not a 1:1 translation.
 */
static int
adfs_mode2atts(struct super_block *sb, struct inode *inode)
{
	umode_t mode;
	int attr;

	/* FIXME: should we be able to alter a link? */
	if (S_ISLNK(inode->i_mode))
		return inode->u.adfs_i.attr;

	if (S_ISDIR(inode->i_mode))
		attr = ADFS_NDA_DIRECTORY;
	else
		attr = 0;

	mode = inode->i_mode & sb->u.adfs_sb.s_owner_mask;
	if (mode & S_IRUGO)
		attr |= ADFS_NDA_OWNER_READ;
	if (mode & S_IWUGO)
		attr |= ADFS_NDA_OWNER_WRITE;

	mode = inode->i_mode & sb->u.adfs_sb.s_other_mask;
	mode &= ~sb->u.adfs_sb.s_owner_mask;
	if (mode & S_IRUGO)
		attr |= ADFS_NDA_PUBLIC_READ;
	if (mode & S_IWUGO)
		attr |= ADFS_NDA_PUBLIC_WRITE;

	return attr;
}

/*
 * Convert an ADFS time to Unix time.  ADFS has a 40-bit centi-second time
 * referenced to 1 Jan 1900 (til 2248)
 */
static unsigned int
adfs_adfs2unix_time(struct inode *inode)
{
	unsigned int high, low;

	if (inode->u.adfs_i.stamped == 0)
		return CURRENT_TIME;

	high = inode->u.adfs_i.loadaddr << 24;
	low  = inode->u.adfs_i.execaddr;

	high |= low >> 8;
	low  &= 255;

	/* Files dated pre  01 Jan 1970 00:00:00. */
	if (high < 0x336e996a)
		return 0;

	/* Files dated post 18 Jan 2038 03:14:05. */
	if (high >= 0x656e9969)
		return 0x7ffffffd;

	/* discard 2208988800 (0x336e996a00) seconds of time */
	high -= 0x336e996a;

	/* convert 40-bit centi-seconds to 32-bit seconds */
	return (((high % 100) << 8) + low) / 100 + (high / 100 << 8);
}

/*
 * Convert an Unix time to ADFS time.  We only do this if the entry has a
 * time/date stamp already.
 */
static void
adfs_unix2adfs_time(struct inode *inode, unsigned int secs)
{
	unsigned int high, low;

	if (inode->u.adfs_i.stamped) {
		/* convert 32-bit seconds to 40-bit centi-seconds */
		low  = (secs & 255) * 100;
		high = (secs / 256) * 100 + (low >> 8) + 0x336e996a;

		inode->u.adfs_i.loadaddr = (high >> 24) |
				(inode->u.adfs_i.loadaddr & ~0xff);
		inode->u.adfs_i.execaddr = (low & 255) | (high << 8);
	}
}

/*
 * Fill in the inode information from the object information.
 *
 * Note that this is an inode-less filesystem, so we can't use the inode
 * number to reference the metadata on the media.  Instead, we use the
 * inode number to hold the object ID, which in turn will tell us where
 * the data is held.  We also save the parent object ID, and with these
 * two, we can locate the metadata.
 *
 * This does mean that we rely on an objects parent remaining the same at
 * all times - we cannot cope with a cross-directory rename (yet).
 */
struct inode *
adfs_iget(struct super_block *sb, struct object_info *obj)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (!inode)
		goto out;

	inode->i_version = ++event;
	inode->i_uid	 = sb->u.adfs_sb.s_uid;
	inode->i_gid	 = sb->u.adfs_sb.s_gid;
	inode->i_ino	 = obj->file_id;
	inode->i_size	 = obj->size;
	inode->i_nlink	 = 2;
	inode->i_blksize = PAGE_SIZE;
	inode->i_blocks	 = (inode->i_size + sb->s_blocksize - 1) >>
			    sb->s_blocksize_bits;

	/*
	 * we need to save the parent directory ID so that
	 * write_inode can update the directory information
	 * for this file.  This will need special handling
	 * for cross-directory renames.
	 */
	inode->u.adfs_i.parent_id = obj->parent_id;
	inode->u.adfs_i.loadaddr  = obj->loadaddr;
	inode->u.adfs_i.execaddr  = obj->execaddr;
	inode->u.adfs_i.attr      = obj->attr;
	inode->u.adfs_i.stamped	  = ((obj->loadaddr & 0xfff00000) == 0xfff00000);

	inode->i_mode	 = adfs_atts2mode(sb, inode);
	inode->i_mtime	 =
	inode->i_atime	 =
	inode->i_ctime	 = adfs_adfs2unix_time(inode);

	if (S_ISDIR(inode->i_mode)) {
		inode->i_op	= &adfs_dir_inode_operations;
		inode->i_fop	= &adfs_dir_operations;
	} else if (S_ISREG(inode->i_mode)) {
		inode->i_op	= &adfs_file_inode_operations;
		inode->i_fop	= &adfs_file_operations;
		inode->i_mapping->a_ops = &adfs_aops;
		inode->u.adfs_i.mmu_private = inode->i_size;
	}

	insert_inode_hash(inode);

out:
	return inode;
}

/*
 * Validate and convert a changed access mode/time to their ADFS equivalents.
 * adfs_write_inode will actually write the information back to the directory
 * later.
 */
int
adfs_notify_change(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	unsigned int ia_valid = attr->ia_valid;
	int error;

	error = inode_change_ok(inode, attr);

	/*
	 * we can't change the UID or GID of any file -
	 * we have a global UID/GID in the superblock
	 */
	if ((ia_valid & ATTR_UID && attr->ia_uid != sb->u.adfs_sb.s_uid) ||
	    (ia_valid & ATTR_GID && attr->ia_gid != sb->u.adfs_sb.s_gid))
		error = -EPERM;

	if (error)
		goto out;

	if (ia_valid & ATTR_SIZE)
		error = vmtruncate(inode, attr->ia_size);

	if (error)
		goto out;

	if (ia_valid & ATTR_MTIME) {
		inode->i_mtime = attr->ia_mtime;
		adfs_unix2adfs_time(inode, attr->ia_mtime);
	}
	/*
	 * FIXME: should we make these == to i_mtime since we don't
	 * have the ability to represent them in our filesystem?
	 */
	if (ia_valid & ATTR_ATIME)
		inode->i_atime = attr->ia_atime;
	if (ia_valid & ATTR_CTIME)
		inode->i_ctime = attr->ia_ctime;
	if (ia_valid & ATTR_MODE) {
		inode->u.adfs_i.attr = adfs_mode2atts(sb, inode);
		inode->i_mode = adfs_atts2mode(sb, inode);
	}

	/*
	 * FIXME: should we be marking this inode dirty even if
	 * we don't have any metadata to write back?
	 */
	if (ia_valid & (ATTR_SIZE | ATTR_MTIME | ATTR_MODE))
		mark_inode_dirty(inode);
out:
	return error;
}

/*
 * write an existing inode back to the directory, and therefore the disk.
 * The adfs-specific inode data has already been updated by
 * adfs_notify_change()
 */
void adfs_write_inode(struct inode *inode, int unused)
{
	struct super_block *sb = inode->i_sb;
	struct object_info obj;

	lock_kernel();
	obj.file_id	= inode->i_ino;
	obj.name_len	= 0;
	obj.parent_id	= inode->u.adfs_i.parent_id;
	obj.loadaddr	= inode->u.adfs_i.loadaddr;
	obj.execaddr	= inode->u.adfs_i.execaddr;
	obj.attr	= inode->u.adfs_i.attr;
	obj.size	= inode->i_size;

	adfs_dir_update(sb, &obj);
	unlock_kernel();
}
MODULE_LICENSE("GPL");
