/*
 *  linux/fs/hfsplus/inode.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Inode handling routines
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/buffer_head.h>
#endif

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

static int hfsplus_readpage(struct file *file, struct page *page)
{
	//printk("readpage: %lu\n", page->index);
	return block_read_full_page(page, hfsplus_get_block);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int hfsplus_writepage(struct page *page, struct writeback_control *wbc)
{
	//printk("writepage: %lu\n", page->index);
	return block_write_full_page(page, hfsplus_get_block, wbc);
}
#else
static int hfsplus_writepage(struct page *page)
{
	//printk("writepage: %lu\n", page->index);
	return block_write_full_page(page, hfsplus_get_block);
}
#endif

static int hfsplus_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return cont_prepare_write(page, from, to, hfsplus_get_block,
		&HFSPLUS_I(page->mapping->host).mmu_private);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static int hfsplus_bmap(struct address_space *mapping, long block)
#else
static sector_t hfsplus_bmap(struct address_space *mapping, sector_t block)
#endif
{
	return generic_block_bmap(mapping, block, hfsplus_get_block);
}

int hfsplus_releasepage(struct page *page, int mask)
{
	struct inode *inode = page->mapping->host;
	struct super_block *sb = inode->i_sb;
	struct hfsplus_btree *tree;
	hfsplus_bnode *node;
	u32 nidx;
	int i, res = 1;

	switch (inode->i_ino) {
	case HFSPLUS_EXT_CNID:
		tree = HFSPLUS_SB(sb).ext_tree;
		break;
	case HFSPLUS_CAT_CNID:
		tree = HFSPLUS_SB(sb).cat_tree;
		break;
	case HFSPLUS_ATTR_CNID:
		tree = HFSPLUS_SB(sb).attr_tree;
		break;
	default:
		BUG();
		return 0;
	}
	if (tree->node_size >= PAGE_CACHE_SIZE) {
		nidx = page->index >> (tree->node_size_shift - PAGE_CACHE_SHIFT);
		spin_lock(&tree->hash_lock);
		node = __hfsplus_find_bnode(tree, nidx);
		if (!node)
			;
		else if (atomic_read(&node->refcnt))
			res = 0;
		else for (i = 0; i < tree->pages_per_bnode; i++) {
			if (PageActive(node->page[i])) {
				res = 0;
				break;
			}
		}
		if (res && node) {
			__hfsplus_bnode_remove(node);
			hfsplus_bnode_free(node);
		}
		spin_unlock(&tree->hash_lock);
	} else {
		nidx = page->index >> (PAGE_CACHE_SHIFT - tree->node_size_shift);
		i = 1 << (PAGE_CACHE_SHIFT - tree->node_size_shift);
		spin_lock(&tree->hash_lock);
		do {
			node = __hfsplus_find_bnode(tree, nidx++);
			if (!node)
				continue;
			if (atomic_read(&node->refcnt)) {
				res = 0;
				break;
			}
			__hfsplus_bnode_remove(node);
			hfsplus_bnode_free(node);
		} while (--i);
		spin_unlock(&tree->hash_lock);
	}
	//printk("releasepage: %lu,%x = %d\n", page->index, mask, res);
	return res;
}

struct address_space_operations hfsplus_btree_aops = {
	.readpage	= hfsplus_readpage,
	.writepage	= hfsplus_writepage,
	.sync_page	= block_sync_page,
	.prepare_write	= hfsplus_prepare_write,
	.commit_write	= generic_commit_write,
	.bmap		= hfsplus_bmap,
	.releasepage	= hfsplus_releasepage,
};

struct address_space_operations hfsplus_aops = {
	.readpage	= hfsplus_readpage,
	.writepage	= hfsplus_writepage,
	.sync_page	= block_sync_page,
	.prepare_write	= hfsplus_prepare_write,
	.commit_write	= generic_commit_write,
	.bmap		= hfsplus_bmap,
};

static struct dentry *hfsplus_file_lookup(struct inode *dir, struct dentry *dentry)
{
	struct hfsplus_find_data fd;
	struct super_block *sb = dir->i_sb;
	struct inode *inode = NULL;
	int err;

	if (HFSPLUS_IS_RSRC(dir) || strcmp(dentry->d_name.name, "rsrc"))
		goto out;

	inode = HFSPLUS_I(dir).rsrc_inode;
	if (inode)
		goto out;

	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	inode->i_ino = dir->i_ino;
	HFSPLUS_I(inode).flags = HFSPLUS_FLG_RSRC;

	hfsplus_find_init(HFSPLUS_SB(sb).cat_tree, &fd);
	err = hfsplus_find_cat(sb, dir->i_ino, &fd);
	if (!err)
		err = hfsplus_cat_read_inode(inode, &fd);
	hfsplus_find_exit(&fd);
	if (err) {
		iput(inode);
		return ERR_PTR(err);
	}
	HFSPLUS_I(inode).rsrc_inode = dir;
	HFSPLUS_I(dir).rsrc_inode = inode;
	igrab(dir);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	list_add(&inode->i_hash, &HFSPLUS_SB(sb).rsrc_inodes);
#else
	hlist_add_head(&inode->i_hash, &HFSPLUS_SB(sb).rsrc_inodes);
#endif
	mark_inode_dirty(inode);
	{
	void hfsplus_inode_check(struct super_block *sb);
	atomic_inc(&HFSPLUS_SB(sb).inode_cnt);
	hfsplus_inode_check(sb);
	}
out:
	d_add(dentry, inode);
	return NULL;
}

static void hfsplus_get_perms(struct inode *inode, hfsplus_perm *perms, int dir)
{
	struct super_block *sb = inode->i_sb;
	int mode;

	mode = be32_to_cpu(perms->mode) & 0xffff;

	inode->i_uid = be32_to_cpu(perms->owner);
	if (!inode->i_uid && !mode)
		inode->i_uid = HFSPLUS_SB(sb).uid;

	inode->i_gid = be32_to_cpu(perms->group);
	if (!inode->i_gid && !mode)
		inode->i_gid = HFSPLUS_SB(sb).gid;

	if (dir) {
		mode = mode ? (mode & S_IALLUGO) :
			(S_IRWXUGO & ~(HFSPLUS_SB(sb).umask));
		mode |= S_IFDIR;
	} else if (!mode)
		mode = S_IFREG | ((S_IRUGO|S_IWUGO) &
			~(HFSPLUS_SB(sb).umask));
	inode->i_mode = mode;
}

static void hfsplus_set_perms(struct inode *inode, hfsplus_perm *perms)
{
	perms->mode = cpu_to_be32(inode->i_mode);
	perms->owner = cpu_to_be32(inode->i_uid);
	perms->group = cpu_to_be32(inode->i_gid);
	perms->dev = cpu_to_be32(HFSPLUS_I(inode).dev);
}

static int hfsplus_file_open(struct inode *inode, struct file *file)
{
	if (HFSPLUS_IS_RSRC(inode))
		inode = HFSPLUS_I(inode).rsrc_inode;
	if (atomic_read(&file->f_count) != 1)
		return 0;
	atomic_inc(&HFSPLUS_I(inode).opencnt);
	return 0;
}

static int hfsplus_file_release(struct inode *inode, struct file *file)
{
	struct super_block *sb = inode->i_sb;

	if (HFSPLUS_IS_RSRC(inode))
		inode = HFSPLUS_I(inode).rsrc_inode;
	if (atomic_read(&file->f_count) != 0)
		return 0;
	if (atomic_dec_and_test(&HFSPLUS_I(inode).opencnt)) {
		down(&inode->i_sem);
		if (inode->i_flags & S_DEAD) {
			hfsplus_delete_cat(inode->i_ino, HFSPLUS_SB(sb).hidden_dir, NULL);
			hfsplus_delete_inode(inode);
		}
		up(&inode->i_sem);
	}
	return 0;
}

extern struct inode_operations hfsplus_dir_inode_operations;
extern struct file_operations hfsplus_dir_operations;

struct inode_operations hfsplus_file_inode_operations = {
	.lookup		= hfsplus_file_lookup,
	.truncate	= hfsplus_truncate,
};

struct file_operations hfsplus_file_operations = {
	.llseek 	= generic_file_llseek,
	.read		= generic_file_read,
	//.write	= hfsplus_file_write,
	.write		= generic_file_write,
	.mmap		= generic_file_mmap,
	.fsync		= file_fsync,
	.open		= hfsplus_file_open,
	.release	= hfsplus_file_release,
};

struct inode *hfsplus_new_inode(struct super_block *sb, int mode)
{
	struct inode *inode = new_inode(sb);
	if (!inode)
		return NULL;

	{
	void hfsplus_inode_check(struct super_block *sb);
	atomic_inc(&HFSPLUS_SB(sb).inode_cnt);
	hfsplus_inode_check(sb);
	}
	inode->i_ino = HFSPLUS_SB(sb).next_cnid++;
	inode->i_mode = mode;
	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_nlink = 1;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	if (S_ISDIR(inode->i_mode)) {
		inode->i_size = 2;
		HFSPLUS_SB(sb).folder_count++;
		inode->i_op = &hfsplus_dir_inode_operations;
		inode->i_fop = &hfsplus_dir_operations;
		INIT_LIST_HEAD(&HFSPLUS_I(inode).open_dir_list);
	} else if (S_ISREG(inode->i_mode)) {
		HFSPLUS_SB(sb).file_count++;
		inode->i_op = &hfsplus_file_inode_operations;
		inode->i_fop = &hfsplus_file_operations;
		inode->i_mapping->a_ops = &hfsplus_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		HFSPLUS_SB(sb).file_count++;
		inode->i_op = &page_symlink_inode_operations;
		inode->i_mapping->a_ops = &hfsplus_aops;
	} else
		HFSPLUS_SB(sb).file_count++;
	insert_inode_hash(inode);
	mark_inode_dirty(inode);
	sb->s_dirt = 1;

	return inode;
}

void hfsplus_delete_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;

	if (S_ISDIR(inode->i_mode)) {
		HFSPLUS_SB(sb).folder_count--;
		sb->s_dirt = 1;
		return;
	}
	HFSPLUS_SB(sb).file_count--;
	if (S_ISREG(inode->i_mode)) {
		if (!inode->i_nlink) {
			inode->i_size = 0;
			hfsplus_truncate(inode);
		}
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_size = 0;
		hfsplus_truncate(inode);
	}
	sb->s_dirt = 1;
}

void hfsplus_inode_read_fork(struct inode *inode, hfsplus_fork_raw *fork)
{
	u32 count;
	int i;

	memcpy(&HFSPLUS_I(inode).extents, &fork->extents,
	       sizeof(hfsplus_extent_rec));
	for (count = 0, i = 0; i < 8; i++)
		count += be32_to_cpu(fork->extents[i].block_count);
	HFSPLUS_I(inode).extent_blocks = count;
	HFSPLUS_I(inode).total_blocks = HFSPLUS_I(inode).alloc_blocks =
		be32_to_cpu(fork->total_blocks);
	inode->i_size = HFSPLUS_I(inode).mmu_private = be64_to_cpu(fork->total_size);
	inode->i_blocks = be32_to_cpu(fork->total_blocks);
}

void hfsplus_inode_write_fork(struct inode *inode, hfsplus_fork_raw *fork)
{
	memcpy(&fork->extents, &HFSPLUS_I(inode).extents,
	       sizeof(hfsplus_extent_rec));
	fork->total_size = cpu_to_be64(inode->i_size);
	fork->total_blocks = cpu_to_be32(HFSPLUS_I(inode).alloc_blocks);
}

int hfsplus_cat_read_inode(struct inode *inode, struct hfsplus_find_data *fd)
{
	hfsplus_cat_entry entry;
	int res = 0;
	u16 type;

	type = hfsplus_bnode_read_u16(fd->bnode, fd->entryoffset);

	HFSPLUS_I(inode).dev = 0;
	inode->i_blksize = PAGE_SIZE; /* Doesn't seem to be useful... */
	if (type == HFSPLUS_FOLDER) {
		hfsplus_cat_folder *folder = &entry.folder;

		if (fd->entrylength < sizeof(hfsplus_cat_folder))
			/* panic? */;
		hfsplus_bnode_readbytes(fd->bnode, &entry, fd->entryoffset,
					sizeof(hfsplus_cat_folder));
		memset(&HFSPLUS_I(inode).extents, 0,
		       sizeof(hfsplus_extent_rec));
		hfsplus_get_perms(inode, &folder->permissions, 1);
		inode->i_nlink = 1;
		inode->i_size = 2 + be32_to_cpu(folder->valence);
		inode->i_atime = hfsp_mt2ut(folder->access_date);
		inode->i_mtime = hfsp_mt2ut(folder->content_mod_date);
		inode->i_ctime = inode->i_mtime;
		inode->i_blocks = 0;
		inode->i_op = &hfsplus_dir_inode_operations;
		inode->i_fop = &hfsplus_dir_operations;
		INIT_LIST_HEAD(&HFSPLUS_I(inode).open_dir_list);
	} else if (type == HFSPLUS_FILE) {
		hfsplus_cat_file *file = &entry.file;

		if (fd->entrylength < sizeof(hfsplus_cat_file))
			/* panic? */;
		hfsplus_bnode_readbytes(fd->bnode, &entry, fd->entryoffset,
					sizeof(hfsplus_cat_file));

		hfsplus_inode_read_fork(inode, HFSPLUS_IS_DATA(inode) ?
					&file->data_fork : &file->rsrc_fork);
		hfsplus_get_perms(inode, &file->permissions, 0);
		inode->i_nlink = 1;
		if (S_ISREG(inode->i_mode)) {
			if (file->permissions.dev)
				inode->i_nlink = be32_to_cpu(file->permissions.dev);
			inode->i_op = &hfsplus_file_inode_operations;
			inode->i_fop = &hfsplus_file_operations;
			inode->i_mapping->a_ops = &hfsplus_aops;
		} else if (S_ISLNK(inode->i_mode)) {
			inode->i_op = &page_symlink_inode_operations;
			inode->i_mapping->a_ops = &hfsplus_aops;
		} else {
			init_special_inode(inode, inode->i_mode,
					   be32_to_cpu(file->permissions.dev));
		}
		inode->i_atime = hfsp_mt2ut(file->access_date);
		inode->i_mtime = hfsp_mt2ut(file->content_mod_date);
		inode->i_ctime = inode->i_mtime;
	} else {
		printk("HFS+-fs: bad catalog entry used to create inode\n");
		res = -EIO;
	}
	return res;
}

void hfsplus_cat_write_inode(struct inode *inode)
{
	struct hfsplus_find_data fd;
	hfsplus_cat_entry entry;

	if (HFSPLUS_IS_RSRC(inode)) {
		mark_inode_dirty(HFSPLUS_I(inode).rsrc_inode);
		return;
	}

	if (!inode->i_nlink)
		return;

	if (hfsplus_find_init(HFSPLUS_SB(inode->i_sb).cat_tree, &fd))
		/* panic? */
		return;

	if (hfsplus_find_cat(inode->i_sb, inode->i_ino, &fd))
		/* panic? */
		goto out;

	if (S_ISDIR(inode->i_mode)) {
		hfsplus_cat_folder *folder = &entry.folder;

		if (fd.entrylength < sizeof(hfsplus_cat_folder))
			/* panic? */;
		hfsplus_bnode_readbytes(fd.bnode, &entry, fd.entryoffset,
					sizeof(hfsplus_cat_folder));
		/* simple node checks? */
		hfsplus_set_perms(inode, &folder->permissions);
		folder->access_date = hfsp_ut2mt(inode->i_atime);
		folder->content_mod_date = hfsp_ut2mt(inode->i_mtime);
		folder->attribute_mod_date = hfsp_ut2mt(inode->i_ctime);
		folder->valence = cpu_to_be32(inode->i_size - 2);
		hfsplus_bnode_writebytes(fd.bnode, &entry, fd.entryoffset,
					 sizeof(hfsplus_cat_folder));
	} else {
		hfsplus_cat_file *file = &entry.file;

		if (fd.entrylength < sizeof(hfsplus_cat_file))
			/* panic? */;
		hfsplus_bnode_readbytes(fd.bnode, &entry, fd.entryoffset,
					sizeof(hfsplus_cat_file));
		hfsplus_inode_write_fork(inode, &file->data_fork);
		if (HFSPLUS_I(inode).rsrc_inode)
			hfsplus_inode_write_fork(HFSPLUS_I(inode).rsrc_inode, &file->rsrc_fork);
		if (S_ISREG(inode->i_mode))
			HFSPLUS_I(inode).dev = inode->i_nlink;
		if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
			HFSPLUS_I(inode).dev = kdev_t_to_nr(inode->i_rdev);
		hfsplus_set_perms(inode, &file->permissions);
		file->access_date = hfsp_ut2mt(inode->i_atime);
		file->content_mod_date = hfsp_ut2mt(inode->i_mtime);
		file->attribute_mod_date = hfsp_ut2mt(inode->i_ctime);
		hfsplus_bnode_writebytes(fd.bnode, &entry, fd.entryoffset,
					 sizeof(hfsplus_cat_file));
	}
out:
	hfsplus_find_exit(&fd);
}
