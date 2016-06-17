/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: super.c,v 1.48.2.3 2002/10/11 09:04:44 dwmw2 Exp $
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/jffs2.h>
#include <linux/pagemap.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include "nodelist.h"

#ifndef MTD_BLOCK_MAJOR
#define MTD_BLOCK_MAJOR 31
#endif

extern void jffs2_read_inode (struct inode *);
void jffs2_put_super (struct super_block *);
void jffs2_write_super (struct super_block *);
static int jffs2_statfs (struct super_block *, struct statfs *);
int jffs2_remount_fs (struct super_block *, int *, char *);
extern void jffs2_clear_inode (struct inode *);

static struct super_operations jffs2_super_operations =
{
	read_inode:	jffs2_read_inode,
//	delete_inode:	jffs2_delete_inode,
	put_super:	jffs2_put_super,
	write_super:	jffs2_write_super,
	statfs:		jffs2_statfs,
	remount_fs:	jffs2_remount_fs,
	clear_inode:	jffs2_clear_inode
};

static int jffs2_statfs(struct super_block *sb, struct statfs *buf)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);
	unsigned long avail;

	buf->f_type = JFFS2_SUPER_MAGIC;
	buf->f_bsize = 1 << PAGE_SHIFT;
	buf->f_blocks = c->flash_size >> PAGE_SHIFT;
	buf->f_files = 0;
	buf->f_ffree = 0;
	buf->f_namelen = JFFS2_MAX_NAME_LEN;

	spin_lock_bh(&c->erase_completion_lock);

	avail = c->dirty_size + c->free_size;
	if (avail > c->sector_size * JFFS2_RESERVED_BLOCKS_WRITE)
		avail -= c->sector_size * JFFS2_RESERVED_BLOCKS_WRITE;
	else
		avail = 0;

	buf->f_bavail = buf->f_bfree = avail >> PAGE_SHIFT;

#if CONFIG_JFFS2_FS_DEBUG > 0
	printk(KERN_DEBUG "STATFS:\n");
	printk(KERN_DEBUG "flash_size: %08x\n", c->flash_size);
	printk(KERN_DEBUG "used_size: %08x\n", c->used_size);
	printk(KERN_DEBUG "dirty_size: %08x\n", c->dirty_size);
	printk(KERN_DEBUG "free_size: %08x\n", c->free_size);
	printk(KERN_DEBUG "erasing_size: %08x\n", c->erasing_size);
	printk(KERN_DEBUG "bad_size: %08x\n", c->bad_size);
	printk(KERN_DEBUG "sector_size: %08x\n", c->sector_size);

	if (c->nextblock) {
		printk(KERN_DEBUG "nextblock: 0x%08x\n", c->nextblock->offset);
	} else {
		printk(KERN_DEBUG "nextblock: NULL\n");
	}
	if (c->gcblock) {
		printk(KERN_DEBUG "gcblock: 0x%08x\n", c->gcblock->offset);
	} else {
		printk(KERN_DEBUG "gcblock: NULL\n");
	}
	if (list_empty(&c->clean_list)) {
		printk(KERN_DEBUG "clean_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->clean_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "clean_list: %08x\n", jeb->offset);
		}
	}
	if (list_empty(&c->dirty_list)) {
		printk(KERN_DEBUG "dirty_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->dirty_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "dirty_list: %08x\n", jeb->offset);
		}
	}
	if (list_empty(&c->erasing_list)) {
		printk(KERN_DEBUG "erasing_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->erasing_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "erasing_list: %08x\n", jeb->offset);
		}
	}
	if (list_empty(&c->erase_pending_list)) {
		printk(KERN_DEBUG "erase_pending_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->erase_pending_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "erase_pending_list: %08x\n", jeb->offset);
		}
	}
	if (list_empty(&c->free_list)) {
		printk(KERN_DEBUG "free_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->free_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "free_list: %08x\n", jeb->offset);
		}
	}
	if (list_empty(&c->bad_list)) {
		printk(KERN_DEBUG "bad_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->bad_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "bad_list: %08x\n", jeb->offset);
		}
	}
	if (list_empty(&c->bad_used_list)) {
		printk(KERN_DEBUG "bad_used_list: empty\n");
	} else {
		struct list_head *this;

		list_for_each(this, &c->bad_used_list) {
			struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
			printk(KERN_DEBUG "bad_used_list: %08x\n", jeb->offset);
		}
	}
#endif /* CONFIG_JFFS2_FS_DEBUG */

	spin_unlock_bh(&c->erase_completion_lock);


	return 0;
}

static struct super_block *jffs2_read_super(struct super_block *sb, void *data, int silent)
{
	struct jffs2_sb_info *c;
	struct inode *root_i;
	int i;

	D1(printk(KERN_DEBUG "jffs2: read_super for device %s\n", kdevname(sb->s_dev)));

	if (MAJOR(sb->s_dev) != MTD_BLOCK_MAJOR) {
		if (!silent)
			printk(KERN_DEBUG "jffs2: attempt to mount non-MTD device %s\n", kdevname(sb->s_dev));
		return NULL;
	}

	c = JFFS2_SB_INFO(sb);
	memset(c, 0, sizeof(*c));
	
	c->mtd = get_mtd_device(NULL, MINOR(sb->s_dev));
	if (!c->mtd) {
		D1(printk(KERN_DEBUG "jffs2: MTD device #%u doesn't appear to exist\n", MINOR(sb->s_dev)));
		return NULL;
	}
	c->sector_size = c->mtd->erasesize;
	c->free_size = c->flash_size = c->mtd->size;
	c->nr_blocks = c->mtd->size / c->mtd->erasesize;
	c->blocks = kmalloc(sizeof(struct jffs2_eraseblock) * c->nr_blocks, GFP_KERNEL);
	if (!c->blocks)
		goto out_mtd;
	for (i=0; i<c->nr_blocks; i++) {
		INIT_LIST_HEAD(&c->blocks[i].list);
		c->blocks[i].offset = i * c->sector_size;
		c->blocks[i].free_size = c->sector_size;
		c->blocks[i].dirty_size = 0;
		c->blocks[i].used_size = 0;
		c->blocks[i].first_node = NULL;
		c->blocks[i].last_node = NULL;
	}
		
	spin_lock_init(&c->nodelist_lock);
	init_MUTEX(&c->alloc_sem);
	init_waitqueue_head(&c->erase_wait);
	spin_lock_init(&c->erase_completion_lock);
	spin_lock_init(&c->inocache_lock);

	INIT_LIST_HEAD(&c->clean_list);
	INIT_LIST_HEAD(&c->dirty_list);
	INIT_LIST_HEAD(&c->erasing_list);
	INIT_LIST_HEAD(&c->erase_pending_list);
	INIT_LIST_HEAD(&c->erase_complete_list);
	INIT_LIST_HEAD(&c->free_list);
	INIT_LIST_HEAD(&c->bad_list);
	INIT_LIST_HEAD(&c->bad_used_list);
	c->highest_ino = 1;

	if (jffs2_build_filesystem(c)) {
		D1(printk(KERN_DEBUG "build_fs failed\n"));
		goto out_nodes;
	}

	sb->s_op = &jffs2_super_operations;

	D1(printk(KERN_DEBUG "jffs2_read_super(): Getting root inode\n"));
	root_i = iget(sb, 1);
	if (is_bad_inode(root_i)) {
		D1(printk(KERN_WARNING "get root inode failed\n"));
		goto out_nodes;
	}

	D1(printk(KERN_DEBUG "jffs2_read_super(): d_alloc_root()\n"));
	sb->s_root = d_alloc_root(root_i);
	if (!sb->s_root)
		goto out_root_i;

#if LINUX_VERSION_CODE >= 0x20403
	sb->s_maxbytes = 0xFFFFFFFF;
#endif
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = JFFS2_SUPER_MAGIC;
	if (!(sb->s_flags & MS_RDONLY))
		jffs2_start_garbage_collect_thread(c);
	return sb;

 out_root_i:
	iput(root_i);
 out_nodes:
	jffs2_free_ino_caches(c);
	jffs2_free_raw_node_refs(c);
	kfree(c->blocks);
 out_mtd:
	put_mtd_device(c->mtd);
	return NULL;
}

void jffs2_put_super (struct super_block *sb)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);

	D2(printk(KERN_DEBUG "jffs2: jffs2_put_super()\n"));

	if (!(sb->s_flags & MS_RDONLY))
		jffs2_stop_garbage_collect_thread(c);
	jffs2_free_ino_caches(c);
	jffs2_free_raw_node_refs(c);
	kfree(c->blocks);
	if (c->mtd->sync)
		c->mtd->sync(c->mtd);
	put_mtd_device(c->mtd);
	
	D1(printk(KERN_DEBUG "jffs2_put_super returning\n"));
}

int jffs2_remount_fs (struct super_block *sb, int *flags, char *data)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);

	if (c->flags & JFFS2_SB_FLAG_RO && !(sb->s_flags & MS_RDONLY))
		return -EROFS;

	/* We stop if it was running, then restart if it needs to.
	   This also catches the case where it was stopped and this
	   is just a remount to restart it */
	if (!(sb->s_flags & MS_RDONLY))
		jffs2_stop_garbage_collect_thread(c);

	if (!(*flags & MS_RDONLY))
		jffs2_start_garbage_collect_thread(c);
	
	sb->s_flags = (sb->s_flags & ~MS_RDONLY)|(*flags & MS_RDONLY);

	return 0;
}

void jffs2_write_super (struct super_block *sb)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);
	sb->s_dirt = 0;

	if (sb->s_flags & MS_RDONLY)
		return;

	jffs2_garbage_collect_trigger(c);
	jffs2_erase_pending_blocks(c);
	jffs2_mark_erased_blocks(c);
}


static DECLARE_FSTYPE_DEV(jffs2_fs_type, "jffs2", jffs2_read_super);

static int __init init_jffs2_fs(void)
{
	int ret;

	printk(KERN_NOTICE "JFFS2 version 2.1. (C) 2001 Red Hat, Inc., designed by Axis Communications AB.\n");

#ifdef JFFS2_OUT_OF_KERNEL
	/* sanity checks. Could we do these at compile time? */
	if (sizeof(struct jffs2_sb_info) > sizeof (((struct super_block *)NULL)->u)) {
		printk(KERN_ERR "JFFS2 error: struct jffs2_sb_info (%d bytes) doesn't fit in the super_block union (%d bytes)\n", 
		       sizeof(struct jffs2_sb_info), sizeof (((struct super_block *)NULL)->u));
		return -EIO;
	}

	if (sizeof(struct jffs2_inode_info) > sizeof (((struct inode *)NULL)->u)) {
		printk(KERN_ERR "JFFS2 error: struct jffs2_inode_info (%d bytes) doesn't fit in the inode union (%d bytes)\n", 
		       sizeof(struct jffs2_inode_info), sizeof (((struct inode *)NULL)->u));
		return -EIO;
	}
#endif

	ret = jffs2_zlib_init();
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise zlib workspaces\n");
		goto out;
	}
	ret = jffs2_create_slab_caches();
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise slab caches\n");
		goto out_zlib;
	}
	ret = register_filesystem(&jffs2_fs_type);
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to register filesystem\n");
		goto out_slab;
	}
	return 0;

 out_slab:
	jffs2_destroy_slab_caches();
 out_zlib:
	jffs2_zlib_exit();
 out:
	return ret;
}

static void __exit exit_jffs2_fs(void)
{
	jffs2_destroy_slab_caches();
	jffs2_zlib_exit();
	unregister_filesystem(&jffs2_fs_type);
}

module_init(init_jffs2_fs);
module_exit(exit_jffs2_fs);

MODULE_DESCRIPTION("The Journalling Flash File System, v2");
MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL"); // Actually dual-licensed, but it doesn't matter for 
		       // the sake of this tag. It's Free Software.
