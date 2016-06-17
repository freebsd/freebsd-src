/*
 *  linux/fs/hfsplus/super.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <linux/locks.h>
#else
#include <linux/buffer_head.h>
#include <linux/vfs.h>

static struct inode *hfsplus_alloc_inode(struct super_block *sb);
static void hfsplus_destroy_inode(struct inode *inode);
#endif

#include "hfsplus_fs.h"

void hfsplus_inode_check(struct super_block *sb)
{
#if 0
	u32 cnt = atomic_read(&HFSPLUS_SB(sb).inode_cnt);
	u32 last_cnt = HFSPLUS_SB(sb).last_inode_cnt;

	if (cnt <= (last_cnt / 2) ||
	    cnt >= (last_cnt * 2)) {
		HFSPLUS_SB(sb).last_inode_cnt = cnt;
		printk("inode_check: %u,%u,%u\n", cnt, last_cnt,
			HFSPLUS_SB(sb).cat_tree ? HFSPLUS_SB(sb).cat_tree->node_hash_cnt : 0);
	}
#endif
}

static void hfsplus_read_inode(struct inode *inode)
{
	struct hfsplus_find_data fd;
	struct hfsplus_vh *vhdr;
	int err;

	atomic_inc(&HFSPLUS_SB(inode->i_sb).inode_cnt);
	hfsplus_inode_check(inode->i_sb);
	if (inode->i_ino >= HFSPLUS_FIRSTUSER_CNID) {
	read_inode:
		HFSPLUS_I(inode).flags = 0;
		hfsplus_find_init(HFSPLUS_SB(inode->i_sb).cat_tree, &fd);
		err = hfsplus_find_cat(inode->i_sb, inode->i_ino, &fd);
		if (!err)
			err = hfsplus_cat_read_inode(inode, &fd);
		hfsplus_find_exit(&fd);
		if (err)
			goto bad_inode;
		return;
	}
	vhdr = HFSPLUS_SB(inode->i_sb).s_vhdr;
	switch(inode->i_ino) {
	case HFSPLUS_ROOT_CNID:
		goto read_inode;
	case HFSPLUS_EXT_CNID:
		hfsplus_inode_read_fork(inode, &vhdr->ext_file);
		inode->i_mapping->a_ops = &hfsplus_btree_aops;
		break;
	case HFSPLUS_CAT_CNID:
		hfsplus_inode_read_fork(inode, &vhdr->cat_file);
		inode->i_mapping->a_ops = &hfsplus_btree_aops;
		break;
	case HFSPLUS_ALLOC_CNID:
		hfsplus_inode_read_fork(inode, &vhdr->alloc_file);
		break;
	case HFSPLUS_START_CNID:
		hfsplus_inode_read_fork(inode, &vhdr->start_file);
		break;
	case HFSPLUS_ATTR_CNID:
		hfsplus_inode_read_fork(inode, &vhdr->attr_file);
		inode->i_mapping->a_ops = &hfsplus_btree_aops;
		break;
	default:
		goto bad_inode;
	}
	
	return;

 bad_inode:
	make_bad_inode(inode);
}

void hfsplus_write_inode(struct inode *inode, int unused)
{
	struct hfsplus_vh *vhdr;

	dprint(DBG_INODE, "hfsplus_write_inode: %lu\n", inode->i_ino);
	if (inode->i_ino >= HFSPLUS_FIRSTUSER_CNID) {
		hfsplus_cat_write_inode(inode);
		return;
	}
	vhdr = HFSPLUS_SB(inode->i_sb).s_vhdr;
	switch (inode->i_ino) {
	case HFSPLUS_ROOT_CNID:
		hfsplus_cat_write_inode(inode);
		break;
	case HFSPLUS_EXT_CNID:
		if (vhdr->ext_file.total_size != cpu_to_be64(inode->i_size)) {
			HFSPLUS_SB(inode->i_sb).flags |= HFSPLUS_SB_WRITEBACKUP;
			inode->i_sb->s_dirt = 1;
		}
		hfsplus_inode_write_fork(inode, &vhdr->ext_file);
		hfsplus_write_btree(HFSPLUS_SB(inode->i_sb).ext_tree);
		break;
	case HFSPLUS_CAT_CNID:
		if (vhdr->cat_file.total_size != cpu_to_be64(inode->i_size)) {
			HFSPLUS_SB(inode->i_sb).flags |= HFSPLUS_SB_WRITEBACKUP;
			inode->i_sb->s_dirt = 1;
		}
		hfsplus_inode_write_fork(inode, &vhdr->cat_file);
		hfsplus_write_btree(HFSPLUS_SB(inode->i_sb).cat_tree);
		break;
	case HFSPLUS_ALLOC_CNID:
		if (vhdr->alloc_file.total_size != cpu_to_be64(inode->i_size)) {
			HFSPLUS_SB(inode->i_sb).flags |= HFSPLUS_SB_WRITEBACKUP;
			inode->i_sb->s_dirt = 1;
		}
		hfsplus_inode_write_fork(inode, &vhdr->alloc_file);
		break;
	case HFSPLUS_START_CNID:
		if (vhdr->start_file.total_size != cpu_to_be64(inode->i_size)) {
			HFSPLUS_SB(inode->i_sb).flags |= HFSPLUS_SB_WRITEBACKUP;
			inode->i_sb->s_dirt = 1;
		}
		hfsplus_inode_write_fork(inode, &vhdr->start_file);
		break;
	case HFSPLUS_ATTR_CNID:
		if (vhdr->attr_file.total_size != cpu_to_be64(inode->i_size)) {
			HFSPLUS_SB(inode->i_sb).flags |= HFSPLUS_SB_WRITEBACKUP;
			inode->i_sb->s_dirt = 1;
		}
		hfsplus_inode_write_fork(inode, &vhdr->attr_file);
		hfsplus_write_btree(HFSPLUS_SB(inode->i_sb).attr_tree);
		break;
	}
}

static void hfsplus_clear_inode(struct inode *inode)
{
	atomic_dec(&HFSPLUS_SB(inode->i_sb).inode_cnt);
	if (HFSPLUS_IS_RSRC(inode)) {
		HFSPLUS_I(HFSPLUS_I(inode).rsrc_inode).rsrc_inode = NULL;
		iput(HFSPLUS_I(inode).rsrc_inode);
	}
	hfsplus_inode_check(inode->i_sb);
}

static void hfsplus_write_super(struct super_block *sb)
{
	struct hfsplus_vh *vhdr = HFSPLUS_SB(sb).s_vhdr;

	dprint(DBG_SUPER, "hfsplus_write_super\n");
	sb->s_dirt = 0;
	if (sb->s_flags & MS_RDONLY)
		/* warn? */
		return;

	vhdr->free_blocks = cpu_to_be32(HFSPLUS_SB(sb).free_blocks);
	vhdr->next_alloc = cpu_to_be32(HFSPLUS_SB(sb).next_alloc);
	vhdr->next_cnid = cpu_to_be32(HFSPLUS_SB(sb).next_cnid);
	vhdr->folder_count = cpu_to_be32(HFSPLUS_SB(sb).folder_count);
	vhdr->file_count = cpu_to_be32(HFSPLUS_SB(sb).file_count);

	mark_buffer_dirty(HFSPLUS_SB(sb).s_vhbh);
	if (HFSPLUS_SB(sb).flags & HFSPLUS_SB_WRITEBACKUP) {
		if (HFSPLUS_SB(sb).sect_count) {
			struct buffer_head *bh;
			u32 block, offset;

			block = HFSPLUS_SB(sb).blockoffset;
			block += (HFSPLUS_SB(sb).sect_count - 2) >> (sb->s_blocksize_bits - 9);
			offset = ((HFSPLUS_SB(sb).sect_count - 2) << 9) & (sb->s_blocksize - 1);
			//printk("backup: %u,%u,%u,%u\n", HFSPLUS_SB(sb).blockoffset,
			//	HFSPLUS_SB(sb).sect_count, block, offset);
			bh = sb_bread(sb, block);
			vhdr = (struct hfsplus_vh *)(bh->b_data + offset);
			if (be16_to_cpu(vhdr->signature) == HFSPLUS_VOLHEAD_SIG) {
				memcpy(vhdr, HFSPLUS_SB(sb).s_vhdr, sizeof(*vhdr));
				mark_buffer_dirty(bh);
				brelse(bh);
			} else
				printk("backup not found!\n");
		}
		HFSPLUS_SB(sb).flags &= ~HFSPLUS_SB_WRITEBACKUP;
	}
}

static void hfsplus_put_super(struct super_block *sb)
{
	dprint(DBG_SUPER, "hfsplus_put_super\n");
	if (!(sb->s_flags & MS_RDONLY)) {
		struct hfsplus_vh *vhdr = HFSPLUS_SB(sb).s_vhdr;

		vhdr->modify_date = hfsp_now2mt();
		vhdr->attributes |= cpu_to_be32(HFSPLUS_VOL_UNMNT);
		vhdr->attributes &= cpu_to_be32(~HFSPLUS_VOL_INCNSTNT);
		mark_buffer_dirty(HFSPLUS_SB(sb).s_vhbh);
		ll_rw_block(WRITE, 1, &HFSPLUS_SB(sb).s_vhbh);
		wait_on_buffer(HFSPLUS_SB(sb).s_vhbh);
	}

	hfsplus_close_btree(HFSPLUS_SB(sb).cat_tree);
	hfsplus_close_btree(HFSPLUS_SB(sb).ext_tree);
	iput(HFSPLUS_SB(sb).alloc_file);
	iput(HFSPLUS_SB(sb).hidden_dir);
	brelse(HFSPLUS_SB(sb).s_vhbh);
}

static int hfsplus_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = HFSPLUS_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = be32_to_cpu(HFSPLUS_SB(sb).s_vhdr->total_blocks);
	buf->f_bfree = HFSPLUS_SB(sb).free_blocks;
	buf->f_bavail = buf->f_bfree;
	buf->f_files = 0xFFFFFFFF;
	buf->f_ffree = 0xFFFFFFFF - HFSPLUS_SB(sb).next_cnid;
	buf->f_namelen = HFSPLUS_MAX_STRLEN;

	return 0;
}

int hfsplus_remount(struct super_block *sb, int *flags, char *data)
{
	if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
		return 0;
	if (!(*flags & MS_RDONLY)) {
		struct hfsplus_vh *vhdr = HFSPLUS_SB(sb).s_vhdr;

		if ((vhdr->attributes & cpu_to_be32(HFSPLUS_VOL_INCNSTNT)) ||
		    !(vhdr->attributes & cpu_to_be32(HFSPLUS_VOL_UNMNT))) {
			printk("HFS+-fs warning: Filesystem was not cleanly unmounted, "
			       "running fsck.hfsplus is recommended.  leaving read-only.\n");
			sb->s_flags |= MS_RDONLY;
			*flags |= MS_RDONLY;
		} else if (vhdr->attributes & cpu_to_be32(HFSPLUS_VOL_SOFTLOCK)) {
			printk("HFS+-fs: Filesystem is marked locked, leaving read-only.\n");
			sb->s_flags |= MS_RDONLY;
			*flags |= MS_RDONLY;
		}
	}
	return 0;
}

static struct super_operations hfsplus_sops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	.alloc_inode	= hfsplus_alloc_inode,
	.destroy_inode	= hfsplus_destroy_inode,
#endif
	.read_inode	= hfsplus_read_inode,
	.write_inode	= hfsplus_write_inode,
	.clear_inode	= hfsplus_clear_inode,
	.put_super	= hfsplus_put_super,
	.write_super	= hfsplus_write_super,
	.statfs		= hfsplus_statfs,
	.remount_fs	= hfsplus_remount,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
struct super_block *hfsplus_read_super(struct super_block *sb, void *data,
				       int silent)
#else
static int hfsplus_fill_super(struct super_block *sb, void *data, int silent)
#endif
{
	struct hfsplus_vh *vhdr;
	struct hfsplus_sb_info *sbi;
	hfsplus_cat_entry entry;
	struct hfsplus_find_data fd;
	struct qstr str;
	int err = -EINVAL;

	sbi = kmalloc(sizeof(struct hfsplus_sb_info), GFP_KERNEL);
	if (!sbi) {
		err = -ENOMEM;
		goto out2;
	}
	memset(sbi, 0, sizeof(HFSPLUS_SB(sb)));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if (sizeof(struct inode) - offsetof(struct inode, u) < sizeof(struct hfsplus_inode_info)) {
		extern void hfsplus_inode_info_exceeded_space_in_inode_error(void);
		hfsplus_inode_info_exceeded_space_in_inode_error();
	}

	if (sizeof(struct super_block) - offsetof(struct super_block, u) < sizeof(struct hfsplus_sb_info)) {
		extern void hfsplus_sb_info_exceeded_space_in_super_block_error(void);
		hfsplus_sb_info_exceeded_space_in_super_block_error();
	}

	INIT_LIST_HEAD(&HFSPLUS_SB(sb).rsrc_inodes);
#else
	sb->s_fs_info = sbi;
	INIT_HLIST_HEAD(&sbi->rsrc_inodes);
#endif
	fill_defaults(sbi);
	if (!parse_options(data, sbi)) {
		if (!silent)
			printk("HFS+-fs: unable to parse mount options\n");
		err = -EINVAL;
		goto out2;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	fill_current(sbi, &HFSPLUS_SB(sb));
	kfree(sbi);
#endif

	/* Grab the volume header */
	if (hfsplus_read_wrapper(sb)) {
		if (!silent)
			printk("HFS+-fs: unable to find HFS+ superblock\n");
		err = -EINVAL;
		goto out2;
	}
	vhdr = HFSPLUS_SB(sb).s_vhdr;

	/* Copy parts of the volume header into the superblock */
	sb->s_magic = be16_to_cpu(vhdr->signature);
	if (be16_to_cpu(vhdr->version) != HFSPLUS_CURRENT_VERSION) {
		if (!silent)
			printk("HFS+-fs: wrong filesystem version\n");
		goto cleanup;
	}
	HFSPLUS_SB(sb).total_blocks = be32_to_cpu(vhdr->total_blocks);
	HFSPLUS_SB(sb).free_blocks = be32_to_cpu(vhdr->free_blocks);
	HFSPLUS_SB(sb).next_alloc = be32_to_cpu(vhdr->next_alloc);
	HFSPLUS_SB(sb).next_cnid = be32_to_cpu(vhdr->next_cnid);
	HFSPLUS_SB(sb).file_count = be32_to_cpu(vhdr->file_count);
	HFSPLUS_SB(sb).folder_count = be32_to_cpu(vhdr->folder_count);

	/* Set up operations so we can load metadata */
	sb->s_op = &hfsplus_sops;

	if ((vhdr->attributes & cpu_to_be32(HFSPLUS_VOL_INCNSTNT)) ||
	    !(vhdr->attributes & cpu_to_be32(HFSPLUS_VOL_UNMNT))) {
		if (!silent)
			printk("HFS+-fs warning: Filesystem was not cleanly unmounted, "
			       "running fsck.hfsplus is recommended.  mounting read-only.\n");
		sb->s_flags |= MS_RDONLY;
	} else if (vhdr->attributes & cpu_to_be32(HFSPLUS_VOL_SOFTLOCK)) {
		if (!silent)
			printk("HFS+-fs: Filesystem is marked locked, mounting read-only.\n");
		sb->s_flags |= MS_RDONLY;
	}

	/* Load metadata objects (B*Trees) */
	HFSPLUS_SB(sb).ext_tree = hfsplus_open_btree(sb, HFSPLUS_EXT_CNID);
	if (!HFSPLUS_SB(sb).ext_tree) {
		if (!silent)
			printk("HFS+-fs: failed to load extents file\n");
		goto cleanup;
	}
	HFSPLUS_SB(sb).cat_tree = hfsplus_open_btree(sb, HFSPLUS_CAT_CNID);
	if (!HFSPLUS_SB(sb).cat_tree) {
		if (!silent)
			printk("HFS+-fs: failed to load catalog file\n");
		goto cleanup;
	}

	HFSPLUS_SB(sb).alloc_file = iget(sb, HFSPLUS_ALLOC_CNID);
	if (!HFSPLUS_SB(sb).alloc_file) {
		if (!silent)
			printk("HFS+-fs: failed to load allocation file\n");
		goto cleanup;
	}

	/* Load the root directory */
	sb->s_root = d_alloc_root(iget(sb, HFSPLUS_ROOT_CNID));
	if (!sb->s_root) {
		if (!silent)
			printk("HFS+-fs: failed to load root directory\n");
		goto cleanup;
	}

	str.len = sizeof(HFSP_HIDDENDIR_NAME) - 1;
	str.name = HFSP_HIDDENDIR_NAME;
	hfsplus_find_init(HFSPLUS_SB(sb).cat_tree, &fd);
	hfsplus_fill_cat_key(fd.search_key, HFSPLUS_ROOT_CNID, &str);
	if (!hfsplus_btree_find_entry(&fd, &entry, sizeof(entry))) {
		hfsplus_find_exit(&fd);
		if (entry.type != cpu_to_be16(HFSPLUS_FOLDER))
			goto cleanup;
		HFSPLUS_SB(sb).hidden_dir = iget(sb, be32_to_cpu(entry.folder.id));
		if (!HFSPLUS_SB(sb).hidden_dir)
			goto cleanup;
	} else
		hfsplus_find_exit(&fd);

	if (sb->s_flags & MS_RDONLY)
		goto out;

	/* H+LX == hfsplusutils, H+Lx == this driver, H+lx is unused
	 * all three are registered with Apple for our use
	 */
	vhdr->last_mount_vers = cpu_to_be32(HFSP_MOUNT_VERSION);
	vhdr->modify_date = hfsp_now2mt();
	vhdr->write_count = cpu_to_be32(be32_to_cpu(vhdr->write_count) + 1);
	vhdr->attributes &= cpu_to_be32(~HFSPLUS_VOL_UNMNT);
	vhdr->attributes |= cpu_to_be32(HFSPLUS_VOL_INCNSTNT);
	mark_buffer_dirty(HFSPLUS_SB(sb).s_vhbh);
	ll_rw_block(WRITE, 1, &HFSPLUS_SB(sb).s_vhbh);
	wait_on_buffer(HFSPLUS_SB(sb).s_vhbh);

	if (!HFSPLUS_SB(sb).hidden_dir) {
		printk("HFS+: create hidden dir...\n");
		HFSPLUS_SB(sb).hidden_dir = hfsplus_new_inode(sb, S_IFDIR);
		hfsplus_create_cat(HFSPLUS_SB(sb).hidden_dir->i_ino, sb->s_root->d_inode,
				   &str, HFSPLUS_SB(sb).hidden_dir);
		mark_inode_dirty(HFSPLUS_SB(sb).hidden_dir);
	}
out:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	return sb;
#else
	return 0;
#endif

cleanup:
	hfsplus_put_super(sb);
out2:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	return NULL;
#else
	return err;
#endif
}

MODULE_AUTHOR("Brad Boyer");
MODULE_DESCRIPTION("Extended Macintosh Filesystem");
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)

static DECLARE_FSTYPE_DEV(hfsplus_fs_type, "hfsplus", hfsplus_read_super);

static int __init init_hfsplus_fs(void)
{
	return register_filesystem(&hfsplus_fs_type);
}

static void __exit exit_hfsplus_fs(void)
{
	unregister_filesystem(&hfsplus_fs_type);
}

EXPORT_NO_SYMBOLS;

#else

static kmem_cache_t * hfsplus_inode_cachep;

static void init_once(void *p, kmem_cache_t *cachep, unsigned long flags)
{
	struct hfsplus_inode_info *i = (struct hfsplus_inode_info *)p;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) == SLAB_CTOR_CONSTRUCTOR) {
		inode_init_once(&i->vfs_inode);
	}
}

static struct inode *hfsplus_alloc_inode(struct super_block *sb)
{
	struct hfsplus_inode_info *i;
	i = (struct hfsplus_inode_info *)kmem_cache_alloc(hfsplus_inode_cachep, SLAB_KERNEL);
	return i ? &i->vfs_inode : NULL;
}

static void hfsplus_destroy_inode(struct inode *inode)
{
	kmem_cache_free(hfsplus_inode_cachep, &HFSPLUS_I(inode));
}

static struct super_block *hfsplus_get_sb(struct file_system_type *fs_type,
					  int flags, char *dev_name, void *data)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, hfsplus_fill_super);
}

static struct file_system_type hfsplus_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "hfsplus",
	.get_sb		= hfsplus_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_hfsplus_fs(void)
{
	int err;

	hfsplus_inode_cachep = kmem_cache_create("hfsplus_inode_cache",
		sizeof(struct hfsplus_inode_info), 0, SLAB_HWCACHE_ALIGN,
		init_once, NULL);
	if (!hfsplus_inode_cachep)
		return -ENOMEM;
	err = register_filesystem(&hfsplus_fs_type);
	if (err)
		kmem_cache_destroy(hfsplus_inode_cachep);
	return err;
}

static void __exit exit_hfsplus_fs(void)
{
	unregister_filesystem(&hfsplus_fs_type);
	if (kmem_cache_destroy(hfsplus_inode_cachep))
		printk(KERN_INFO "hfsplus_inode_cache: not all structures were freed\n");
}
#endif

module_init(init_hfsplus_fs)
module_exit(exit_hfsplus_fs)
