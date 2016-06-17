/*
 *  linux/fs/hpfs/file.c
 *
 *  Mikulas Patocka (mikulas@artax.karlin.mff.cuni.cz), 1998-1999
 *
 *  file VFS functions
 */

#include <linux/string.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include "hpfs_fn.h"

#define BLOCKS(size) (((size) + 511) >> 9)

/* HUH? */
int hpfs_open(struct inode *i, struct file *f)
{
	lock_kernel();
	hpfs_lock_inode(i);
	hpfs_unlock_inode(i); /* make sure nobody is deleting the file */
	unlock_kernel();
	if (!i->i_nlink) return -ENOENT;
	return 0;
}

int hpfs_file_release(struct inode *inode, struct file *file)
{
	lock_kernel();
	hpfs_write_if_changed(inode);
	unlock_kernel();
	return 0;
}

int hpfs_file_fsync(struct file *file, struct dentry *dentry, int datasync)
{
	/*return file_fsync(file, dentry);*/
	return 0; /* Don't fsync :-) */
}

/*
 * generic_file_read often calls bmap with non-existing sector,
 * so we must ignore such errors.
 */

secno hpfs_bmap(struct inode *inode, unsigned file_secno)
{
	unsigned n, disk_secno;
	struct fnode *fnode;
	struct buffer_head *bh;
	if (BLOCKS(inode->u.hpfs_i.mmu_private) <= file_secno) return 0;
	n = file_secno - inode->i_hpfs_file_sec;
	if (n < inode->i_hpfs_n_secs) return inode->i_hpfs_disk_sec + n;
	if (!(fnode = hpfs_map_fnode(inode->i_sb, inode->i_ino, &bh))) return 0;
	disk_secno = hpfs_bplus_lookup(inode->i_sb, inode, &fnode->btree, file_secno, bh);
	if (disk_secno == -1) return 0;
	if (hpfs_chk_sectors(inode->i_sb, disk_secno, 1, "bmap")) return 0;
	return disk_secno;
}

void hpfs_truncate(struct inode *i)
{
	if (IS_IMMUTABLE(i)) return /*-EPERM*/;
	i->i_hpfs_n_secs = 0;
	i->i_blocks = 1 + ((i->i_size + 511) >> 9);
	i->u.hpfs_i.mmu_private = i->i_size;
	hpfs_truncate_btree(i->i_sb, i->i_ino, 1, ((i->i_size + 511) >> 9));
	hpfs_write_inode(i);
}

int hpfs_get_block(struct inode *inode, long iblock, struct buffer_head *bh_result, int create)
{
	secno s;
	s = hpfs_bmap(inode, iblock);
	if (s) {
		bh_result->b_dev = inode->i_dev;
		bh_result->b_blocknr = s;
		bh_result->b_state |= (1UL << BH_Mapped);
		return 0;
	}
	if (!create) return 0;
	if (iblock<<9 != inode->u.hpfs_i.mmu_private) {
		BUG();
		return -EIO;
	}
	if ((s = hpfs_add_sector_to_btree(inode->i_sb, inode->i_ino, 1, inode->i_blocks - 1)) == -1) {
		hpfs_truncate_btree(inode->i_sb, inode->i_ino, 1, inode->i_blocks - 1);
		return -ENOSPC;
	}
	inode->i_blocks++;
	inode->u.hpfs_i.mmu_private += 512;
	bh_result->b_dev = inode->i_dev;
	bh_result->b_blocknr = s;
	bh_result->b_state |= (1UL << BH_Mapped) | (1UL << BH_New);
	return 0;
}

static int hpfs_writepage(struct page *page)
{
	return block_write_full_page(page,hpfs_get_block);
}
static int hpfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page,hpfs_get_block);
}
static int hpfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return cont_prepare_write(page,from,to,hpfs_get_block,
		&page->mapping->host->u.hpfs_i.mmu_private);
}
static int _hpfs_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping,block,hpfs_get_block);
}
struct address_space_operations hpfs_aops = {
	readpage: hpfs_readpage,
	writepage: hpfs_writepage,
	sync_page: block_sync_page,
	prepare_write: hpfs_prepare_write,
	commit_write: generic_commit_write,
	bmap: _hpfs_bmap
};

ssize_t hpfs_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	ssize_t retval;

	retval = generic_file_write(file, buf, count, ppos);
	if (retval > 0) {
		struct inode *inode = file->f_dentry->d_inode;
		inode->i_mtime = CURRENT_TIME;
		inode->i_hpfs_dirty = 1;
	}
	return retval;
}

