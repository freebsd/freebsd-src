/*
 *  linux/fs/ext3/namei.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *  Directory entry file type support and forward compatibility hooks
 *  	for B-tree directories by Theodore Ts'o (tytso@mit.edu), 1998
 */

#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/sched.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/quotaops.h>


/*
 * define how far ahead to read directories while searching them.
 */
#define NAMEI_RA_CHUNKS  2
#define NAMEI_RA_BLOCKS  4
#define NAMEI_RA_SIZE        (NAMEI_RA_CHUNKS * NAMEI_RA_BLOCKS)
#define NAMEI_RA_INDEX(c,b)  (((c) * NAMEI_RA_BLOCKS) + (b))

/*
 * NOTE! unlike strncmp, ext3_match returns 1 for success, 0 for failure.
 *
 * `len <= EXT3_NAME_LEN' is guaranteed by caller.
 * `de != NULL' is guaranteed by caller.
 */
static inline int ext3_match (int len, const char * const name,
			      struct ext3_dir_entry_2 * de)
{
	if (len != de->name_len)
		return 0;
	if (!de->inode)
		return 0;
	return !memcmp(name, de->name, len);
}

/*
 * Returns 0 if not found, -1 on failure, and 1 on success
 */
static int inline search_dirblock(struct buffer_head * bh,
				  struct inode *dir,
				  struct dentry *dentry,
				  unsigned long offset,
				  struct ext3_dir_entry_2 ** res_dir)
{
	struct ext3_dir_entry_2 * de;
	char * dlimit;
	int de_len;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;

	de = (struct ext3_dir_entry_2 *) bh->b_data;
	dlimit = bh->b_data + dir->i_sb->s_blocksize;
	while ((char *) de < dlimit) {
		/* this code is executed quadratically often */
		/* do minimal checking `by hand' */

		if ((char *) de + namelen <= dlimit &&
		    ext3_match (namelen, name, de)) {
			/* found a match - just to be sure, do a full check */
			if (!ext3_check_dir_entry("ext3_find_entry",
						  dir, de, bh, offset))
				return -1;
			*res_dir = de;
			return 1;
		}
		/* prevent looping on a bad block */
		de_len = le16_to_cpu(de->rec_len);
		if (de_len <= 0)
			return -1;
		offset += de_len;
		de = (struct ext3_dir_entry_2 *) ((char *) de + de_len);
	}
	return 0;
}

/*
 *	ext3_find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself (as a parameter - res_dir). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 *
 * The returned buffer_head has ->b_count elevated.  The caller is expected
 * to brelse() it when appropriate.
 */
static struct buffer_head * ext3_find_entry (struct dentry *dentry,
					struct ext3_dir_entry_2 ** res_dir)
{
	struct super_block * sb;
	struct buffer_head * bh_use[NAMEI_RA_SIZE];
	struct buffer_head * bh, *ret = NULL;
	unsigned long start, block, b;
	int ra_max = 0;		/* Number of bh's in the readahead
				   buffer, bh_use[] */
	int ra_ptr = 0;		/* Current index into readahead
				   buffer */
	int num = 0;
	int nblocks, i, err;
	struct inode *dir = dentry->d_parent->d_inode;

	*res_dir = NULL;
	sb = dir->i_sb;

	nblocks = dir->i_size >> EXT3_BLOCK_SIZE_BITS(sb);
	start = dir->u.ext3_i.i_dir_start_lookup;
	if (start >= nblocks)
		start = 0;
	block = start;
restart:
	do {
		/*
		 * We deal with the read-ahead logic here.
		 */
		if (ra_ptr >= ra_max) {
			/* Refill the readahead buffer */
			ra_ptr = 0;
			b = block;
			for (ra_max = 0; ra_max < NAMEI_RA_SIZE; ra_max++) {
				/*
				 * Terminate if we reach the end of the
				 * directory and must wrap, or if our
				 * search has finished at this block.
				 */
				if (b >= nblocks || (num && block == start)) {
					bh_use[ra_max] = NULL;
					break;
				}
				num++;
				bh = ext3_getblk(NULL, dir, b++, 0, &err);
				bh_use[ra_max] = bh;
				if (bh)
					ll_rw_block(READ, 1, &bh);
			}
		}
		if ((bh = bh_use[ra_ptr++]) == NULL)
			goto next;
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh)) {
			/* read error, skip block & hope for the best */
			brelse(bh);
			goto next;
		}
		i = search_dirblock(bh, dir, dentry,
			    block << EXT3_BLOCK_SIZE_BITS(sb), res_dir);
		if (i == 1) {
			dir->u.ext3_i.i_dir_start_lookup = block;
			ret = bh;
			goto cleanup_and_exit;
		} else {
			brelse(bh);
			if (i < 0)
				goto cleanup_and_exit;
		}
	next:
		if (++block >= nblocks)
			block = 0;
	} while (block != start);

	/*
	 * If the directory has grown while we were searching, then
	 * search the last part of the directory before giving up.
	 */
	block = nblocks;
	nblocks = dir->i_size >> EXT3_BLOCK_SIZE_BITS(sb);
	if (block < nblocks) {
		start = 0;
		goto restart;
	}
		
cleanup_and_exit:
	/* Clean up the read-ahead blocks */
	for (; ra_ptr < ra_max; ra_ptr++)
		brelse (bh_use[ra_ptr]);
	return ret;
}

static struct dentry *ext3_lookup(struct inode * dir, struct dentry *dentry)
{
	struct inode * inode;
	struct ext3_dir_entry_2 * de;
	struct buffer_head * bh;

	if (dentry->d_name.len > EXT3_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	bh = ext3_find_entry(dentry, &de);
	inode = NULL;
	if (bh) {
		unsigned long ino = le32_to_cpu(de->inode);
		brelse (bh);
		inode = iget(dir->i_sb, ino);

		if (!inode)
			return ERR_PTR(-EACCES);
	}
	d_add(dentry, inode);
	return NULL;
}

#define S_SHIFT 12
static unsigned char ext3_type_by_mode[S_IFMT >> S_SHIFT] = {
	[S_IFREG >> S_SHIFT]	EXT3_FT_REG_FILE,
	[S_IFDIR >> S_SHIFT]	EXT3_FT_DIR,
	[S_IFCHR >> S_SHIFT]	EXT3_FT_CHRDEV,
	[S_IFBLK >> S_SHIFT]	EXT3_FT_BLKDEV,
	[S_IFIFO >> S_SHIFT]	EXT3_FT_FIFO,
	[S_IFSOCK >> S_SHIFT]	EXT3_FT_SOCK,
	[S_IFLNK >> S_SHIFT]	EXT3_FT_SYMLINK,
};

static inline void ext3_set_de_type(struct super_block *sb,
				struct ext3_dir_entry_2 *de,
				umode_t mode) {
	if (EXT3_HAS_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_FILETYPE))
		de->file_type = ext3_type_by_mode[(mode & S_IFMT)>>S_SHIFT];
}

/*
 *	ext3_add_entry()
 *
 * adds a file entry to the specified directory, using the same
 * semantics as ext3_find_entry(). It returns NULL if it failed.
 *
 * NOTE!! The inode part of 'de' is left at 0 - which means you
 * may not sleep between calling this and putting something into
 * the entry, as someone else might have used it while you slept.
 */

/*
 * AKPM: the journalling code here looks wrong on the error paths
 */
static int ext3_add_entry (handle_t *handle, struct dentry *dentry,
	struct inode *inode)
{
	struct inode *dir = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	unsigned long offset;
	unsigned short rec_len;
	struct buffer_head * bh;
	struct ext3_dir_entry_2 * de, * de1;
	struct super_block * sb;
	int	retval;

	sb = dir->i_sb;

	if (!namelen)
		return -EINVAL;
	bh = ext3_bread (handle, dir, 0, 0, &retval);
	if (!bh)
		return retval;
	rec_len = EXT3_DIR_REC_LEN(namelen);
	offset = 0;
	de = (struct ext3_dir_entry_2 *) bh->b_data;
	while (1) {
		if ((char *)de >= sb->s_blocksize + bh->b_data) {
			brelse (bh);
			bh = NULL;
			bh = ext3_bread (handle, dir,
				offset >> EXT3_BLOCK_SIZE_BITS(sb), 1, &retval);
			if (!bh)
				return retval;
			if (dir->i_size <= offset) {
				if (dir->i_size == 0) {
					brelse(bh);
					return -ENOENT;
				}

				ext3_debug ("creating next block\n");

				BUFFER_TRACE(bh, "get_write_access");
				ext3_journal_get_write_access(handle, bh);
				de = (struct ext3_dir_entry_2 *) bh->b_data;
				de->inode = 0;
				de->rec_len = le16_to_cpu(sb->s_blocksize);
				dir->u.ext3_i.i_disksize =
					dir->i_size = offset + sb->s_blocksize;
				dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
				ext3_mark_inode_dirty(handle, dir);
			} else {

				ext3_debug ("skipping to next block\n");

				de = (struct ext3_dir_entry_2 *) bh->b_data;
			}
		}
		if (!ext3_check_dir_entry ("ext3_add_entry", dir, de, bh,
					   offset)) {
			brelse (bh);
			return -ENOENT;
		}
		if (ext3_match (namelen, name, de)) {
				brelse (bh);
				return -EEXIST;
		}
		if ((le32_to_cpu(de->inode) == 0 &&
				le16_to_cpu(de->rec_len) >= rec_len) ||
		    (le16_to_cpu(de->rec_len) >=
				EXT3_DIR_REC_LEN(de->name_len) + rec_len)) {
			BUFFER_TRACE(bh, "get_write_access");
			ext3_journal_get_write_access(handle, bh);
			/* By now the buffer is marked for journaling */
			offset += le16_to_cpu(de->rec_len);
			if (le32_to_cpu(de->inode)) {
				de1 = (struct ext3_dir_entry_2 *) ((char *) de +
					EXT3_DIR_REC_LEN(de->name_len));
				de1->rec_len =
					cpu_to_le16(le16_to_cpu(de->rec_len) -
					EXT3_DIR_REC_LEN(de->name_len));
				de->rec_len = cpu_to_le16(
						EXT3_DIR_REC_LEN(de->name_len));
				de = de1;
			}
			de->file_type = EXT3_FT_UNKNOWN;
			if (inode) {
				de->inode = cpu_to_le32(inode->i_ino);
				ext3_set_de_type(dir->i_sb, de, inode->i_mode);
			} else
				de->inode = 0;
			de->name_len = namelen;
			memcpy (de->name, name, namelen);
			/*
			 * XXX shouldn't update any times until successful
			 * completion of syscall, but too many callers depend
			 * on this.
			 *
			 * XXX similarly, too many callers depend on
			 * ext3_new_inode() setting the times, but error
			 * recovery deletes the inode, so the worst that can
			 * happen is that the times are slightly out of date
			 * and/or different from the directory change time.
			 */
			dir->i_mtime = dir->i_ctime = CURRENT_TIME;
			dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
			dir->i_version = ++event;
			ext3_mark_inode_dirty(handle, dir);
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			ext3_journal_dirty_metadata(handle, bh);
			brelse(bh);
			return 0;
		}
		offset += le16_to_cpu(de->rec_len);
		de = (struct ext3_dir_entry_2 *)
			((char *) de + le16_to_cpu(de->rec_len));
	}
	brelse (bh);
	return -ENOSPC;
}

/*
 * ext3_delete_entry deletes a directory entry by merging it with the
 * previous entry
 */
static int ext3_delete_entry (handle_t *handle, 
			      struct inode * dir,
			      struct ext3_dir_entry_2 * de_del,
			      struct buffer_head * bh)
{
	struct ext3_dir_entry_2 * de, * pde;
	int i;

	i = 0;
	pde = NULL;
	de = (struct ext3_dir_entry_2 *) bh->b_data;
	while (i < bh->b_size) {
		if (!ext3_check_dir_entry("ext3_delete_entry", dir, de, bh, i))
			return -EIO;
		if (de == de_del)  {
			BUFFER_TRACE(bh, "get_write_access");
			ext3_journal_get_write_access(handle, bh);
			if (pde)
				pde->rec_len =
					cpu_to_le16(le16_to_cpu(pde->rec_len) +
						    le16_to_cpu(de->rec_len));
			else
				de->inode = 0;
			dir->i_version = ++event;
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			ext3_journal_dirty_metadata(handle, bh);
			return 0;
		}
		i += le16_to_cpu(de->rec_len);
		pde = de;
		de = (struct ext3_dir_entry_2 *)
			((char *) de + le16_to_cpu(de->rec_len));
	}
	return -ENOENT;
}

/*
 * ext3_mark_inode_dirty is somewhat expensive, so unlike ext2 we
 * do not perform it in these functions.  We perform it at the call site,
 * if it is needed.
 */
static inline void ext3_inc_count(handle_t *handle, struct inode *inode)
{
	inode->i_nlink++;
}

static inline void ext3_dec_count(handle_t *handle, struct inode *inode)
{
	inode->i_nlink--;
}

static int ext3_add_nondir(handle_t *handle,
		struct dentry *dentry, struct inode *inode)
{
	int err = ext3_add_entry(handle, dentry, inode);
	if (!err) {
		err = ext3_mark_inode_dirty(handle, inode);
		if (err == 0) {
			d_instantiate(dentry, inode);
			return 0;
		}
	}
	ext3_dec_count(handle, inode);
	iput(inode);
	return err;
}

/*
 * By the time this is called, we already have created
 * the directory cache entry for the new file, but it
 * is so far negative - it has no inode.
 *
 * If the create succeeds, we fill in the inode information
 * with d_instantiate(). 
 */
static int ext3_create (struct inode * dir, struct dentry * dentry, int mode)
{
	handle_t *handle; 
	struct inode * inode;
	int err;

	handle = ext3_journal_start(dir, EXT3_DATA_TRANS_BLOCKS + 3);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	inode = ext3_new_inode (handle, dir, mode);
	err = PTR_ERR(inode);
	if (!IS_ERR(inode)) {
		inode->i_op = &ext3_file_inode_operations;
		inode->i_fop = &ext3_file_operations;
		inode->i_mapping->a_ops = &ext3_aops;
		err = ext3_add_nondir(handle, dentry, inode);
	}
	ext3_journal_stop(handle, dir);
	return err;
}

static int ext3_mknod (struct inode * dir, struct dentry *dentry,
			int mode, int rdev)
{
	handle_t *handle;
	struct inode *inode;
	int err;

	handle = ext3_journal_start(dir, EXT3_DATA_TRANS_BLOCKS + 3);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	inode = ext3_new_inode (handle, dir, mode);
	err = PTR_ERR(inode);
	if (!IS_ERR(inode)) {
		init_special_inode(inode, mode, rdev);
		err = ext3_add_nondir(handle, dentry, inode);
	}
	ext3_journal_stop(handle, dir);
	return err;
}

static int ext3_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	handle_t *handle;
	struct inode * inode;
	struct buffer_head * dir_block;
	struct ext3_dir_entry_2 * de;
	int err;

	if (dir->i_nlink >= EXT3_LINK_MAX)
		return -EMLINK;

	handle = ext3_journal_start(dir, EXT3_DATA_TRANS_BLOCKS + 3);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	inode = ext3_new_inode (handle, dir, S_IFDIR);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_stop;

	inode->i_op = &ext3_dir_inode_operations;
	inode->i_fop = &ext3_dir_operations;
	inode->i_size = inode->u.ext3_i.i_disksize = inode->i_sb->s_blocksize;
	inode->i_blocks = 0;	
	dir_block = ext3_bread (handle, inode, 0, 1, &err);
	if (!dir_block) {
		inode->i_nlink--; /* is this nlink == 0? */
		ext3_mark_inode_dirty(handle, inode);
		iput (inode);
		goto out_stop;
	}
	BUFFER_TRACE(dir_block, "get_write_access");
	ext3_journal_get_write_access(handle, dir_block);
	de = (struct ext3_dir_entry_2 *) dir_block->b_data;
	de->inode = cpu_to_le32(inode->i_ino);
	de->name_len = 1;
	de->rec_len = cpu_to_le16(EXT3_DIR_REC_LEN(de->name_len));
	strcpy (de->name, ".");
	ext3_set_de_type(dir->i_sb, de, S_IFDIR);
	de = (struct ext3_dir_entry_2 *)
			((char *) de + le16_to_cpu(de->rec_len));
	de->inode = cpu_to_le32(dir->i_ino);
	de->rec_len = cpu_to_le16(inode->i_sb->s_blocksize-EXT3_DIR_REC_LEN(1));
	de->name_len = 2;
	strcpy (de->name, "..");
	ext3_set_de_type(dir->i_sb, de, S_IFDIR);
	inode->i_nlink = 2;
	BUFFER_TRACE(dir_block, "call ext3_journal_dirty_metadata");
	ext3_journal_dirty_metadata(handle, dir_block);
	brelse (dir_block);
	inode->i_mode = S_IFDIR | mode;
	if (dir->i_mode & S_ISGID)
		inode->i_mode |= S_ISGID;
	ext3_mark_inode_dirty(handle, inode);
	err = ext3_add_entry (handle, dentry, inode);
	if (err)
		goto out_no_entry;
	dir->i_nlink++;
	dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
	ext3_mark_inode_dirty(handle, dir);
	d_instantiate(dentry, inode);
out_stop:
	ext3_journal_stop(handle, dir);
	return err;

out_no_entry:
	inode->i_nlink = 0;
	ext3_mark_inode_dirty(handle, inode);
	iput (inode);
	goto out_stop;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
static int empty_dir (struct inode * inode)
{
	unsigned long offset;
	struct buffer_head * bh;
	struct ext3_dir_entry_2 * de, * de1;
	struct super_block * sb;
	int err;

	sb = inode->i_sb;
	if (inode->i_size < EXT3_DIR_REC_LEN(1) + EXT3_DIR_REC_LEN(2) ||
	    !(bh = ext3_bread (NULL, inode, 0, 0, &err))) {
	    	ext3_warning (inode->i_sb, "empty_dir",
			      "bad directory (dir #%lu) - no data block",
			      inode->i_ino);
		return 1;
	}
	de = (struct ext3_dir_entry_2 *) bh->b_data;
	de1 = (struct ext3_dir_entry_2 *)
			((char *) de + le16_to_cpu(de->rec_len));
	if (le32_to_cpu(de->inode) != inode->i_ino ||
			!le32_to_cpu(de1->inode) || 
			strcmp (".", de->name) ||
			strcmp ("..", de1->name)) {
	    	ext3_warning (inode->i_sb, "empty_dir",
			      "bad directory (dir #%lu) - no `.' or `..'",
			      inode->i_ino);
		brelse (bh);
		return 1;
	}
	offset = le16_to_cpu(de->rec_len) + le16_to_cpu(de1->rec_len);
	de = (struct ext3_dir_entry_2 *)
			((char *) de1 + le16_to_cpu(de1->rec_len));
	while (offset < inode->i_size ) {
		if (!bh ||
			(void *) de >= (void *) (bh->b_data+sb->s_blocksize)) {
			brelse (bh);
			bh = ext3_bread (NULL, inode,
				offset >> EXT3_BLOCK_SIZE_BITS(sb), 0, &err);
			if (!bh) {
#if 0
				ext3_error (sb, "empty_dir",
				"directory #%lu contains a hole at offset %lu",
					inode->i_ino, offset);
#endif
				offset += sb->s_blocksize;
				continue;
			}
			de = (struct ext3_dir_entry_2 *) bh->b_data;
		}
		if (!ext3_check_dir_entry ("empty_dir", inode, de, bh,
					   offset)) {
			brelse (bh);
			return 1;
		}
		if (le32_to_cpu(de->inode)) {
			brelse (bh);
			return 0;
		}
		offset += le16_to_cpu(de->rec_len);
		de = (struct ext3_dir_entry_2 *)
				((char *) de + le16_to_cpu(de->rec_len));
	}
	brelse (bh);
	return 1;
}

/* ext3_orphan_add() links an unlinked or truncated inode into a list of
 * such inodes, starting at the superblock, in case we crash before the
 * file is closed/deleted, or in case the inode truncate spans multiple
 * transactions and the last transaction is not recovered after a crash.
 *
 * At filesystem recovery time, we walk this list deleting unlinked
 * inodes and truncating linked inodes in ext3_orphan_cleanup().
 */
int ext3_orphan_add(handle_t *handle, struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct ext3_iloc iloc;
	int err = 0, rc;
	
	lock_super(sb);
	if (!list_empty(&inode->u.ext3_i.i_orphan))
		goto out_unlock;

	/* Orphan handling is only valid for files with data blocks
	 * being truncated, or files being unlinked. */

	/* @@@ FIXME: Observation from aviro:
	 * I think I can trigger J_ASSERT in ext3_orphan_add().  We block 
	 * here (on lock_super()), so race with ext3_link() which might bump
	 * ->i_nlink. For, say it, character device. Not a regular file,
	 * not a directory, not a symlink and ->i_nlink > 0.
	 */
	J_ASSERT ((S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
		S_ISLNK(inode->i_mode)) || inode->i_nlink == 0);

	BUFFER_TRACE(sb->u.ext3_sb.s_sbh, "get_write_access");
	err = ext3_journal_get_write_access(handle, sb->u.ext3_sb.s_sbh);
	if (err)
		goto out_unlock;
	
	err = ext3_reserve_inode_write(handle, inode, &iloc);
	if (err)
		goto out_unlock;

	/* Insert this inode at the head of the on-disk orphan list... */
	NEXT_ORPHAN(inode) = le32_to_cpu(EXT3_SB(sb)->s_es->s_last_orphan);
	EXT3_SB(sb)->s_es->s_last_orphan = cpu_to_le32(inode->i_ino);
	err = ext3_journal_dirty_metadata(handle, sb->u.ext3_sb.s_sbh);
	rc = ext3_mark_iloc_dirty(handle, inode, &iloc);
	if (!err)
		err = rc;

	/* Only add to the head of the in-memory list if all the
	 * previous operations succeeded.  If the orphan_add is going to
	 * fail (possibly taking the journal offline), we can't risk
	 * leaving the inode on the orphan list: stray orphan-list
	 * entries can cause panics at unmount time.
	 *
	 * This is safe: on error we're going to ignore the orphan list
	 * anyway on the next recovery. */
	if (!err)
		list_add(&inode->u.ext3_i.i_orphan, &EXT3_SB(sb)->s_orphan);

	jbd_debug(4, "superblock will point to %ld\n", inode->i_ino);
	jbd_debug(4, "orphan inode %ld will point to %d\n",
			inode->i_ino, NEXT_ORPHAN(inode));
out_unlock:
	unlock_super(sb);
	ext3_std_error(inode->i_sb, err);
	return err;
}

/*
 * ext3_orphan_del() removes an unlinked or truncated inode from the list
 * of such inodes stored on disk, because it is finally being cleaned up.
 */
int ext3_orphan_del(handle_t *handle, struct inode *inode)
{
	struct list_head *prev;
	struct ext3_sb_info *sbi;
	unsigned long ino_next;
	struct ext3_iloc iloc;
	int err = 0;

	lock_super(inode->i_sb);
	if (list_empty(&inode->u.ext3_i.i_orphan)) {
		unlock_super(inode->i_sb);
		return 0;
	}

	ino_next = NEXT_ORPHAN(inode);
	prev = inode->u.ext3_i.i_orphan.prev;
	sbi = EXT3_SB(inode->i_sb);

	jbd_debug(4, "remove inode %lu from orphan list\n", inode->i_ino);

	list_del(&inode->u.ext3_i.i_orphan);
	INIT_LIST_HEAD(&inode->u.ext3_i.i_orphan);

	/* If we're on an error path, we may not have a valid
	 * transaction handle with which to update the orphan list on
	 * disk, but we still need to remove the inode from the linked
	 * list in memory. */
	if (!handle)
		goto out;

	err = ext3_reserve_inode_write(handle, inode, &iloc);
	if (err)
		goto out_err;

	if (prev == &sbi->s_orphan) {
		jbd_debug(4, "superblock will point to %lu\n", ino_next);
		BUFFER_TRACE(sbi->s_sbh, "get_write_access");
		err = ext3_journal_get_write_access(handle, sbi->s_sbh);
		if (err)
			goto out_brelse;
		sbi->s_es->s_last_orphan = cpu_to_le32(ino_next);
		err = ext3_journal_dirty_metadata(handle, sbi->s_sbh);
	} else {
		struct ext3_iloc iloc2;
		struct inode *i_prev =
			list_entry(prev, struct inode, u.ext3_i.i_orphan);

		jbd_debug(4, "orphan inode %lu will point to %lu\n",
			  i_prev->i_ino, ino_next);
		err = ext3_reserve_inode_write(handle, i_prev, &iloc2);
		if (err)
			goto out_brelse;
		NEXT_ORPHAN(i_prev) = ino_next;
		err = ext3_mark_iloc_dirty(handle, i_prev, &iloc2);
	}
	if (err)
		goto out_brelse;
	NEXT_ORPHAN(inode) = 0;
	err = ext3_mark_iloc_dirty(handle, inode, &iloc);
	if (err)
		goto out_brelse;

out_err:
	ext3_std_error(inode->i_sb, err);
out:
	unlock_super(inode->i_sb);
	return err;

out_brelse:
	brelse(iloc.bh);
	goto out_err;
}

static int ext3_rmdir (struct inode * dir, struct dentry *dentry)
{
	int retval;
	struct inode * inode;
	struct buffer_head * bh;
	struct ext3_dir_entry_2 * de;
	handle_t *handle;

	handle = ext3_journal_start(dir, EXT3_DELETE_TRANS_BLOCKS);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	retval = -ENOENT;
	bh = ext3_find_entry (dentry, &de);
	if (!bh)
		goto end_rmdir;

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	inode = dentry->d_inode;
	DQUOT_INIT(inode);

	retval = -EIO;
	if (le32_to_cpu(de->inode) != inode->i_ino)
		goto end_rmdir;

	retval = -ENOTEMPTY;
	if (!empty_dir (inode))
		goto end_rmdir;

	retval = ext3_delete_entry(handle, dir, de, bh);
	if (retval)
		goto end_rmdir;
	if (inode->i_nlink != 2)
		ext3_warning (inode->i_sb, "ext3_rmdir",
			      "empty directory has nlink!=2 (%d)",
			      inode->i_nlink);
	inode->i_version = ++event;
	inode->i_nlink = 0;
	/* There's no need to set i_disksize: the fact that i_nlink is
	 * zero will ensure that the right thing happens during any
	 * recovery. */
	inode->i_size = 0;
	ext3_orphan_add(handle, inode);
	dir->i_nlink--;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	ext3_mark_inode_dirty(handle, inode);
	dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
	ext3_mark_inode_dirty(handle, dir);

end_rmdir:
	ext3_journal_stop(handle, dir);
	brelse (bh);
	return retval;
}

static int ext3_unlink(struct inode * dir, struct dentry *dentry)
{
	int retval;
	struct inode * inode;
	struct buffer_head * bh;
	struct ext3_dir_entry_2 * de;
	handle_t *handle;

	handle = ext3_journal_start(dir, EXT3_DELETE_TRANS_BLOCKS);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	retval = -ENOENT;
	bh = ext3_find_entry (dentry, &de);
	if (!bh)
		goto end_unlink;

	inode = dentry->d_inode;
	DQUOT_INIT(inode);

	retval = -EIO;
	if (le32_to_cpu(de->inode) != inode->i_ino)
		goto end_unlink;
	
	if (!inode->i_nlink) {
		ext3_warning (inode->i_sb, "ext3_unlink",
			      "Deleting nonexistent file (%lu), %d",
			      inode->i_ino, inode->i_nlink);
		inode->i_nlink = 1;
	}
	retval = ext3_delete_entry(handle, dir, de, bh);
	if (retval)
		goto end_unlink;
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
	ext3_mark_inode_dirty(handle, dir);
	inode->i_nlink--;
	if (!inode->i_nlink)
		ext3_orphan_add(handle, inode);
	inode->i_ctime = dir->i_ctime;
	ext3_mark_inode_dirty(handle, inode);
	retval = 0;

end_unlink:
	ext3_journal_stop(handle, dir);
	brelse (bh);
	return retval;
}

static int ext3_symlink (struct inode * dir,
		struct dentry *dentry, const char * symname)
{
	handle_t *handle;
	struct inode * inode;
	int l, err;

	l = strlen(symname)+1;
	if (l > dir->i_sb->s_blocksize)
		return -ENAMETOOLONG;

	handle = ext3_journal_start(dir, EXT3_DATA_TRANS_BLOCKS + 5);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	inode = ext3_new_inode (handle, dir, S_IFLNK|S_IRWXUGO);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_stop;

	if (l > sizeof (inode->u.ext3_i.i_data)) {
		inode->i_op = &page_symlink_inode_operations;
		inode->i_mapping->a_ops = &ext3_aops;
		/*
		 * block_symlink() calls back into ext3_prepare/commit_write.
		 * We have a transaction open.  All is sweetness.  It also sets
		 * i_size in generic_commit_write().
		 */
		err = block_symlink(inode, symname, l);
		if (err)
			goto out_no_entry;
	} else {
		inode->i_op = &ext3_fast_symlink_inode_operations;
		memcpy((char*)&inode->u.ext3_i.i_data,symname,l);
		inode->i_size = l-1;
	}
	inode->u.ext3_i.i_disksize = inode->i_size;
	err = ext3_add_nondir(handle, dentry, inode);
out_stop:
	ext3_journal_stop(handle, dir);
	return err;

out_no_entry:
	ext3_dec_count(handle, inode);
	ext3_mark_inode_dirty(handle, inode);
	iput (inode);
	goto out_stop;
}

static int ext3_link (struct dentry * old_dentry,
		struct inode * dir, struct dentry *dentry)
{
	handle_t *handle;
	struct inode *inode = old_dentry->d_inode;
	int err;

	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	if (inode->i_nlink >= EXT3_LINK_MAX)
		return -EMLINK;

	handle = ext3_journal_start(dir, EXT3_DATA_TRANS_BLOCKS);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(dir))
		handle->h_sync = 1;

	inode->i_ctime = CURRENT_TIME;
	ext3_inc_count(handle, inode);
	atomic_inc(&inode->i_count);

	err = ext3_add_nondir(handle, dentry, inode);
	ext3_journal_stop(handle, dir);
	return err;
}

#define PARENT_INO(buffer) \
	((struct ext3_dir_entry_2 *) ((char *) buffer + \
	le16_to_cpu(((struct ext3_dir_entry_2 *) buffer)->rec_len)))->inode

/*
 * Anybody can rename anything with this: the permission checks are left to the
 * higher-level routines.
 */
static int ext3_rename (struct inode * old_dir, struct dentry *old_dentry,
			   struct inode * new_dir,struct dentry *new_dentry)
{
	handle_t *handle;
	struct inode * old_inode, * new_inode;
	struct buffer_head * old_bh, * new_bh, * dir_bh;
	struct ext3_dir_entry_2 * old_de, * new_de;
	int retval;

	old_bh = new_bh = dir_bh = NULL;

	handle = ext3_journal_start(old_dir, 2 * EXT3_DATA_TRANS_BLOCKS + 2);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (IS_SYNC(old_dir) || IS_SYNC(new_dir))
		handle->h_sync = 1;

	old_bh = ext3_find_entry (old_dentry, &old_de);
	/*
	 *  Check for inode number is _not_ due to possible IO errors.
	 *  We might rmdir the source, keep it as pwd of some process
	 *  and merrily kill the link to whatever was created under the
	 *  same name. Goodbye sticky bit ;-<
	 */
	old_inode = old_dentry->d_inode;
	retval = -ENOENT;
	if (!old_bh || le32_to_cpu(old_de->inode) != old_inode->i_ino)
		goto end_rename;

	new_inode = new_dentry->d_inode;
	new_bh = ext3_find_entry (new_dentry, &new_de);
	if (new_bh) {
		if (!new_inode) {
			brelse (new_bh);
			new_bh = NULL;
		} else {
			DQUOT_INIT(new_inode);
		}
	}
	if (S_ISDIR(old_inode->i_mode)) {
		if (new_inode) {
			retval = -ENOTEMPTY;
			if (!empty_dir (new_inode))
				goto end_rename;
		}
		retval = -EIO;
		dir_bh = ext3_bread (handle, old_inode, 0, 0, &retval);
		if (!dir_bh)
			goto end_rename;
		if (le32_to_cpu(PARENT_INO(dir_bh->b_data)) != old_dir->i_ino)
			goto end_rename;
		retval = -EMLINK;
		if (!new_inode && new_dir!=old_dir &&
				new_dir->i_nlink >= EXT3_LINK_MAX)
			goto end_rename;
	}
	if (!new_bh) {
		retval = ext3_add_entry (handle, new_dentry, old_inode);
		if (retval)
			goto end_rename;
	} else {
		BUFFER_TRACE(new_bh, "get write access");
		BUFFER_TRACE(new_bh, "get_write_access");
		ext3_journal_get_write_access(handle, new_bh);
		new_de->inode = le32_to_cpu(old_inode->i_ino);
		if (EXT3_HAS_INCOMPAT_FEATURE(new_dir->i_sb,
					      EXT3_FEATURE_INCOMPAT_FILETYPE))
			new_de->file_type = old_de->file_type;
		new_dir->i_version = ++event;
		BUFFER_TRACE(new_bh, "call ext3_journal_dirty_metadata");
		ext3_journal_dirty_metadata(handle, new_bh);
		brelse(new_bh);
		new_bh = NULL;
	}

	/*
	 * Like most other Unix systems, set the ctime for inodes on a
	 * rename.
	 */
	old_inode->i_ctime = CURRENT_TIME;
	ext3_mark_inode_dirty(handle, old_inode);

	/*
	 * ok, that's it
	 */
	ext3_delete_entry(handle, old_dir, old_de, old_bh);

	if (new_inode) {
		new_inode->i_nlink--;
		new_inode->i_ctime = CURRENT_TIME;
	}
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;
	old_dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
	if (dir_bh) {
		BUFFER_TRACE(dir_bh, "get_write_access");
		ext3_journal_get_write_access(handle, dir_bh);
		PARENT_INO(dir_bh->b_data) = le32_to_cpu(new_dir->i_ino);
		BUFFER_TRACE(dir_bh, "call ext3_journal_dirty_metadata");
		ext3_journal_dirty_metadata(handle, dir_bh);
		old_dir->i_nlink--;
		if (new_inode) {
			new_inode->i_nlink--;
		} else {
			new_dir->i_nlink++;
			new_dir->u.ext3_i.i_flags &= ~EXT3_INDEX_FL;
			ext3_mark_inode_dirty(handle, new_dir);
		}
	}
	ext3_mark_inode_dirty(handle, old_dir);
	if (new_inode) {
		ext3_mark_inode_dirty(handle, new_inode);
		if (!new_inode->i_nlink)
			ext3_orphan_add(handle, new_inode);
	}
	retval = 0;

end_rename:
	brelse (dir_bh);
	brelse (old_bh);
	brelse (new_bh);
	ext3_journal_stop(handle, old_dir);
	return retval;
}

/*
 * directories can handle most operations...
 */
struct inode_operations ext3_dir_inode_operations = {
	create:		ext3_create,		/* BKL held */
	lookup:		ext3_lookup,		/* BKL held */
	link:		ext3_link,		/* BKL held */
	unlink:		ext3_unlink,		/* BKL held */
	symlink:	ext3_symlink,		/* BKL held */
	mkdir:		ext3_mkdir,		/* BKL held */
	rmdir:		ext3_rmdir,		/* BKL held */
	mknod:		ext3_mknod,		/* BKL held */
	rename:		ext3_rename,		/* BKL held */
};
