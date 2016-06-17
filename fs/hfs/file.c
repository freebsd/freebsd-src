/*
 * linux/fs/hfs/file.c
 *
 * Copyright (C) 1995, 1996  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the file-related functions which are independent of
 * which scheme is being used to represent forks.
 *
 * Based on the minix file system code, (C) 1991, 1992 by Linus Torvalds
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs.h"
#include <linux/hfs_fs_sb.h>
#include <linux/hfs_fs_i.h>
#include <linux/hfs_fs.h>

/*================ Forward declarations ================*/

static hfs_rwret_t hfs_file_read(struct file *, char *, hfs_rwarg_t,
				 loff_t *);
static hfs_rwret_t hfs_file_write(struct file *, const char *, hfs_rwarg_t,
				  loff_t *);
static void hfs_file_truncate(struct inode *);

/*================ Global variables ================*/

struct file_operations hfs_file_operations = {
	llseek:		generic_file_llseek,
	read:		hfs_file_read,
	write:		hfs_file_write,
	mmap:		generic_file_mmap,
	fsync:		file_fsync,
};

struct inode_operations hfs_file_inode_operations = {
	truncate:	hfs_file_truncate,
	setattr:	hfs_notify_change,
};

/*================ Variable-like macros ================*/

/* maximum number of blocks to try to read in at once */
#define NBUF 32

/*================ File-local functions ================*/

/*
 * hfs_getblk()
 *
 * Given an hfs_fork and a block number return the buffer_head for
 * that block from the fork.  If 'create' is non-zero then allocate
 * the necessary block(s) to the fork.
 */
struct buffer_head *hfs_getblk(struct hfs_fork *fork, int block, int create)
{
	int tmp;
	struct super_block *sb = fork->entry->mdb->sys_mdb;

	tmp = hfs_extent_map(fork, block, create);

	if (create) {
		/* If writing the block, then we have exclusive access
		   to the file until we return, so it can't have moved.
		*/
		if (tmp) {
			hfs_cat_mark_dirty(fork->entry);
			return sb_getblk(sb, tmp);
		}
		return NULL;
	} else {
		/* If reading the block, then retry since the
		   location on disk could have changed while
		   we waited on the I/O in getblk to complete.
		*/
		do {
			struct buffer_head *bh = sb_getblk(sb, tmp);
			int tmp2 = hfs_extent_map(fork, block, 0);

			if (tmp2 == tmp) {
				return bh;
			} else {
				/* The block moved or no longer exists. */
				brelse(bh);
				tmp = tmp2;
			}
		} while (tmp != 0);

		/* The block no longer exists. */
		return NULL;
	}
}

/*
 * hfs_get_block
 *
 * This is the hfs_get_block() field in the inode_operations structure for
 * "regular" (non-header) files.  The purpose is to translate an inode
 * and a block number within the corresponding file into a physical
 * block number.  This function just calls hfs_extent_map() to do the
 * real work and then stuffs the appropriate info into the buffer_head.
 */
int hfs_get_block(struct inode *inode, long iblock, struct buffer_head *bh_result, int create)
{
	unsigned long phys;

	phys = hfs_extent_map(HFS_I(inode)->fork, iblock, create);
	if (phys) {
		bh_result->b_dev = inode->i_dev;
		bh_result->b_blocknr = phys;
		bh_result->b_state |= (1UL << BH_Mapped);
		if (create)
			bh_result->b_state |= (1UL << BH_New);
		return 0;
	}

	if (!create)
		return 0;

	/* we tried to add stuff, but we couldn't. send back an out-of-space
	 * error. */
	return -ENOSPC;
}


/*
 * hfs_file_read()
 *
 * This is the read field in the inode_operations structure for
 * "regular" (non-header) files.  The purpose is to transfer up to
 * 'count' bytes from the file corresponding to 'inode', beginning at
 * 'filp->offset' bytes into the file.	The data is transferred to
 * user-space at the address 'buf'.  Returns the number of bytes
 * successfully transferred.  This function checks the arguments, does
 * some setup and then calls hfs_do_read() to do the actual transfer.  */
static hfs_rwret_t hfs_file_read(struct file * filp, char * buf, 
				 hfs_rwarg_t count, loff_t *ppos)
{
        struct inode *inode = filp->f_dentry->d_inode;
	hfs_s32 read, left, pos, size;

	if (!S_ISREG(inode->i_mode)) {
		hfs_warn("hfs_file_read: mode = %07o\n",inode->i_mode);
		return -EINVAL;
	}
	pos = *ppos;
	if (pos >= HFS_FORK_MAX) {
		return 0;
	}
	size = inode->i_size;
	if (pos > size) {
		left = 0;
	} else {
		left = size - pos;
	}
	if (left > count) {
		left = count;
	}
	if (left <= 0) {
		return 0;
	}
	if ((read = hfs_do_read(inode, HFS_I(inode)->fork, pos,
				buf, left, filp->f_reada != 0)) > 0) {
	        *ppos += read;
		filp->f_reada = 1;
	}

	return read;
}

/*
 * hfs_file_write()
 *
 * This is the write() entry in the file_operations structure for
 * "regular" files.  The purpose is to transfer up to 'count' bytes
 * to the file corresponding to 'inode' beginning at offset
 * 'file->f_pos' from user-space at the address 'buf'.  The return
 * value is the number of bytes actually transferred.
 */
static hfs_rwret_t hfs_file_write(struct file * filp, const char * buf,
				  hfs_rwarg_t count, loff_t *ppos)
{
        struct inode    *inode = filp->f_dentry->d_inode;
	struct hfs_fork *fork = HFS_I(inode)->fork;
	hfs_s32 written, pos;

	if (!S_ISREG(inode->i_mode)) {
		hfs_warn("hfs_file_write: mode = %07o\n", inode->i_mode);
		return -EINVAL;
	}

	pos = (filp->f_flags & O_APPEND) ? inode->i_size : *ppos;

	if (pos >= HFS_FORK_MAX) {
		return 0;
	}
	if (count > HFS_FORK_MAX) {
		count = HFS_FORK_MAX;
	}
	if ((written = hfs_do_write(inode, fork, pos, buf, count)) > 0)
	        pos += written;

	*ppos = pos;
	if (*ppos > inode->i_size) {
	        inode->i_size = *ppos;
		mark_inode_dirty(inode);
	}

	return written;
}

/*
 * hfs_file_truncate()
 *
 * This is the truncate() entry in the file_operations structure for
 * "regular" files.  The purpose is to change the length of the file
 * corresponding to the given inode.  Changes can either lengthen or
 * shorten the file.
 */
static void hfs_file_truncate(struct inode * inode)
{
	struct hfs_fork *fork = HFS_I(inode)->fork;

	fork->lsize = inode->i_size;
	hfs_extent_adj(fork);
	hfs_cat_mark_dirty(HFS_I(inode)->entry);

	inode->i_size = fork->lsize;
	inode->i_blocks = fork->psize;
	mark_inode_dirty(inode);
}

/*
 * xlate_to_user()
 *
 * Like copy_to_user() while translating CR->NL.
 */
static inline void xlate_to_user(char *buf, const char *data, int count)
{
	char ch;

	while (count--) {
		ch = *(data++);
		put_user((ch == '\r') ? '\n' : ch, buf++);
	}
}

/*
 * xlate_from_user()
 *
 * Like copy_from_user() while translating NL->CR;
 */
static inline int xlate_from_user(char *data, const char *buf, int count)
{
	int i;

	i = copy_from_user(data, buf, count);
	count -= i;
	while (count--) {
		if (*data == '\n') {
			*data = '\r';
		}
		++data;
	}
	return i;
}

/*================ Global functions ================*/

/*
 * hfs_do_read()
 *
 * This function transfers actual data from disk to user-space memory,
 * returning the number of bytes successfully transferred.  'fork' tells
 * which file on the disk to read from.  'pos' gives the offset into
 * the Linux file at which to begin the transfer.  Note that this will
 * differ from 'filp->offset' in the case of an AppleDouble header file
 * due to the block of metadata at the beginning of the file, which has
 * no corresponding place in the HFS file.  'count' tells how many
 * bytes to transfer.  'buf' gives an address in user-space to transfer
 * the data to.
 * 
 * This is based on Linus's minix_file_read().
 * It has been changed to take into account that HFS files have no holes.
 */
hfs_s32 hfs_do_read(struct inode *inode, struct hfs_fork * fork, hfs_u32 pos,
		    char * buf, hfs_u32 count, int reada)
{
	kdev_t dev = inode->i_dev;
	hfs_s32 size, chars, offset, block, blocks, read = 0;
	int bhrequest, uptodate;
	int convert = HFS_I(inode)->convert;
	struct buffer_head ** bhb, ** bhe;
	struct buffer_head * bhreq[NBUF];
	struct buffer_head * buflist[NBUF];

	/* split 'pos' in to block and (byte) offset components */
	block = pos >> HFS_SECTOR_SIZE_BITS;
	offset = pos & (HFS_SECTOR_SIZE-1);

	/* compute the logical size of the fork in blocks */
	size = (fork->lsize + (HFS_SECTOR_SIZE-1)) >> HFS_SECTOR_SIZE_BITS;

	/* compute the number of physical blocks to be transferred */
	blocks = (count+offset+HFS_SECTOR_SIZE-1) >> HFS_SECTOR_SIZE_BITS;

	bhb = bhe = buflist;
	if (reada) {
		if (blocks < read_ahead[MAJOR(dev)] / (HFS_SECTOR_SIZE>>9)) {
			blocks = read_ahead[MAJOR(dev)] / (HFS_SECTOR_SIZE>>9);
		}
		if (block + blocks > size) {
			blocks = size - block;
		}
	}

	/* We do this in a two stage process.  We first try and
	   request as many blocks as we can, then we wait for the
	   first one to complete, and then we try and wrap up as many
	   as are actually done.
	   
	   This routine is optimized to make maximum use of the
	   various buffers and caches. */

	do {
		bhrequest = 0;
		uptodate = 1;
		while (blocks) {
			--blocks;
			*bhb = hfs_getblk(fork, block++, 0);

			if (!(*bhb)) {
				/* Since there are no holes in HFS files
				   we must have encountered an error.
				   So, stop adding blocks to the queue. */
				blocks = 0;
				break;
			}

			if (!buffer_uptodate(*bhb)) {
				uptodate = 0;
				bhreq[bhrequest++] = *bhb;
			}

			if (++bhb == &buflist[NBUF]) {
				bhb = buflist;
			}

			/* If the block we have on hand is uptodate,
			   go ahead and complete processing. */
			if (uptodate) {
				break;
			}
			if (bhb == bhe) {
				break;
			}
		}

		/* If the only block in the queue is bad then quit */
		if (!(*bhe)) {
			break;
		}

		/* Now request them all */
		if (bhrequest) {
			ll_rw_block(READ, bhrequest, bhreq);
		}

		do {  /* Finish off all I/O that has actually completed */
			char *p;

			wait_on_buffer(*bhe);

			if (!buffer_uptodate(*bhe)) {
				/* read error? */
				brelse(*bhe);
				if (++bhe == &buflist[NBUF]) {
					bhe = buflist;
				}
				count = 0;
				break;
			}

			if (count < HFS_SECTOR_SIZE - offset) {
				chars = count;
			} else {
				chars = HFS_SECTOR_SIZE - offset;
			}
			p = (*bhe)->b_data + offset;
			if (convert) {
				xlate_to_user(buf, p, chars);
			} else {
				chars -= copy_to_user(buf, p, chars);
				if (!chars) {
					brelse(*bhe);
					count = 0;
					if (!read)
						read = -EFAULT;
					break;
				}
			}
			brelse(*bhe);
			count -= chars;
			buf += chars;
			read += chars;
			offset = 0;
			if (++bhe == &buflist[NBUF]) {
				bhe = buflist;
			}
		} while (count && (bhe != bhb) && !buffer_locked(*bhe));
	} while (count);

	/* Release the read-ahead blocks */
	while (bhe != bhb) {
		brelse(*bhe);
		if (++bhe == &buflist[NBUF]) {
			bhe = buflist;
		}
	}
	if (!read) {
		return -EIO;
	}
	return read;
}
 
/*
 * hfs_do_write()
 *
 * This function transfers actual data from user-space memory to disk,
 * returning the number of bytes successfully transferred.  'fork' tells
 * which file on the disk to write to.  'pos' gives the offset into
 * the Linux file at which to begin the transfer.  Note that this will
 * differ from 'filp->offset' in the case of an AppleDouble header file
 * due to the block of metadata at the beginning of the file, which has
 * no corresponding place in the HFS file.  'count' tells how many
 * bytes to transfer.  'buf' gives an address in user-space to transfer
 * the data from.
 * 
 * This is just a minor edit of Linus's minix_file_write().
 */
hfs_s32 hfs_do_write(struct inode *inode, struct hfs_fork * fork, hfs_u32 pos,
		     const char * buf, hfs_u32 count)
{
	hfs_s32 written, c;
	struct buffer_head * bh;
	char * p;
	int convert = HFS_I(inode)->convert;

	written = 0;
	while (written < count) {
		bh = hfs_getblk(fork, pos/HFS_SECTOR_SIZE, 1);
		if (!bh) {
			if (!written) {
				written = -ENOSPC;
			}
			break;
		}
		c = HFS_SECTOR_SIZE - (pos % HFS_SECTOR_SIZE);
		if (c > count - written) {
			c = count - written;
		}
		if (c != HFS_SECTOR_SIZE && !buffer_uptodate(bh)) {
			ll_rw_block(READ, 1, &bh);
			wait_on_buffer(bh);
			if (!buffer_uptodate(bh)) {
				brelse(bh);
				if (!written) {
					written = -EIO;
				}
				break;
			}
		}
		p = (pos % HFS_SECTOR_SIZE) + bh->b_data;
		c -= convert ? xlate_from_user(p, buf, c) :
			copy_from_user(p, buf, c);
		if (!c) {
			brelse(bh);
			if (!written)
				written = -EFAULT;
			break;
		}
		pos += c;
		written += c;
		buf += c;
		mark_buffer_uptodate(bh, 1);
		mark_buffer_dirty(bh);
		brelse(bh);
	}
	if (written > 0) {
		struct hfs_cat_entry *entry = fork->entry;

		inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		if (pos > fork->lsize) {
			fork->lsize = pos;
		}
		entry->modify_date = hfs_u_to_mtime(CURRENT_TIME);
		hfs_cat_mark_dirty(entry);
	}
	return written;
}

/*
 * hfs_file_fix_mode()
 *
 * Fixes up the permissions on a file after changing the write-inhibit bit.
 */
void hfs_file_fix_mode(struct hfs_cat_entry *entry)
{
	struct dentry **de = entry->sys_entry;
	int i;

	if (entry->u.file.flags & HFS_FIL_LOCK) {
		for (i = 0; i < 4; ++i) {
			if (de[i]) {
				de[i]->d_inode->i_mode &= ~S_IWUGO;
			}
		}
	} else {
		for (i = 0; i < 4; ++i) {
			if (de[i]) {
			        struct inode *inode = de[i]->d_inode;
				inode->i_mode |= S_IWUGO;
				inode->i_mode &= 
				  ~HFS_SB(inode->i_sb)->s_umask;
			}
		}
	}
}
