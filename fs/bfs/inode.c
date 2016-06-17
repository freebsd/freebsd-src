/*
 *	fs/bfs/inode.c
 *	BFS superblock and inode operations.
 *	Copyright (C) 1999,2000 Tigran Aivazian <tigran@veritas.com>
 *	From fs/minix, Copyright (C) 1991, 1992 Linus Torvalds.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/bfs_fs.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

#include "bfs_defs.h"

MODULE_AUTHOR("Tigran A. Aivazian <tigran@veritas.com>");
MODULE_DESCRIPTION("SCO UnixWare BFS filesystem for Linux");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;

#undef DEBUG

#ifdef DEBUG
#define dprintf(x...)	printf(x)
#else
#define dprintf(x...)
#endif

void dump_imap(const char *prefix, struct super_block * s);

static void bfs_read_inode(struct inode * inode)
{
	unsigned long ino = inode->i_ino;
	kdev_t dev = inode->i_dev;
	struct bfs_inode * di;
	struct buffer_head * bh;
	int block, off;

	if (ino < BFS_ROOT_INO || ino > inode->i_sb->su_lasti) {
		printf("Bad inode number %s:%08lx\n", bdevname(dev), ino);
		make_bad_inode(inode);
		return;
	}

	block = (ino - BFS_ROOT_INO)/BFS_INODES_PER_BLOCK + 1;
	bh = sb_bread(inode->i_sb, block);
	if (!bh) {
		printf("Unable to read inode %s:%08lx\n", bdevname(dev), ino);
		make_bad_inode(inode);
		return;
	}

	off = (ino - BFS_ROOT_INO) % BFS_INODES_PER_BLOCK;
	di = (struct bfs_inode *)bh->b_data + off;

	inode->i_mode = 0x0000FFFF & di->i_mode;
	if (di->i_vtype == BFS_VDIR) {
		inode->i_mode |= S_IFDIR;
		inode->i_op = &bfs_dir_inops;
		inode->i_fop = &bfs_dir_operations;
	} else if (di->i_vtype == BFS_VREG) {
		inode->i_mode |= S_IFREG;
		inode->i_op = &bfs_file_inops;
		inode->i_fop = &bfs_file_operations;
		inode->i_mapping->a_ops = &bfs_aops;
	}

	inode->i_uid = di->i_uid;
	inode->i_gid = di->i_gid;
	inode->i_nlink = di->i_nlink;
	inode->i_size = BFS_FILESIZE(di);
	inode->i_blocks = BFS_FILEBLOCKS(di);
	inode->i_blksize = PAGE_SIZE;
	inode->i_atime = di->i_atime;
	inode->i_mtime = di->i_mtime;
	inode->i_ctime = di->i_ctime;
	inode->iu_dsk_ino = di->i_ino; /* can be 0 so we store a copy */
	inode->iu_sblock = di->i_sblock;
	inode->iu_eblock = di->i_eblock;

	brelse(bh);
}

static void bfs_write_inode(struct inode * inode, int unused)
{
	unsigned long ino = inode->i_ino;
	kdev_t dev = inode->i_dev;
	struct bfs_inode * di;
	struct buffer_head * bh;
	int block, off;

	if (ino < BFS_ROOT_INO || ino > inode->i_sb->su_lasti) {
		printf("Bad inode number %s:%08lx\n", bdevname(dev), ino);
		return;
	}

	lock_kernel();
	block = (ino - BFS_ROOT_INO)/BFS_INODES_PER_BLOCK + 1;
	bh = sb_bread(inode->i_sb, block);
	if (!bh) {
		printf("Unable to read inode %s:%08lx\n", bdevname(dev), ino);
		unlock_kernel();
		return;
	}

	off = (ino - BFS_ROOT_INO)%BFS_INODES_PER_BLOCK;
	di = (struct bfs_inode *)bh->b_data + off;

	if (inode->i_ino == BFS_ROOT_INO)
		di->i_vtype = BFS_VDIR;
	else
		di->i_vtype = BFS_VREG;

	di->i_ino = inode->i_ino;
	di->i_mode = inode->i_mode;
	di->i_uid = inode->i_uid;
	di->i_gid = inode->i_gid;
	di->i_nlink = inode->i_nlink;
	di->i_atime = inode->i_atime;
	di->i_mtime = inode->i_mtime;
	di->i_ctime = inode->i_ctime;
	di->i_sblock = inode->iu_sblock;
	di->i_eblock = inode->iu_eblock;
	di->i_eoffset = di->i_sblock * BFS_BSIZE + inode->i_size - 1;

	mark_buffer_dirty(bh);
	brelse(bh);
	unlock_kernel();
}

static void bfs_delete_inode(struct inode * inode)
{
	unsigned long ino = inode->i_ino;
	kdev_t dev = inode->i_dev;
	struct bfs_inode * di;
	struct buffer_head * bh;
	int block, off;
	struct super_block * s = inode->i_sb;

	dprintf("ino=%08lx\n", inode->i_ino);

	if (inode->i_ino < BFS_ROOT_INO || inode->i_ino > inode->i_sb->su_lasti) {
		printf("invalid ino=%08lx\n", inode->i_ino);
		return;
	}
	
	inode->i_size = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	lock_kernel();
	mark_inode_dirty(inode);
	block = (ino - BFS_ROOT_INO)/BFS_INODES_PER_BLOCK + 1;
	bh = sb_bread(s, block);
	if (!bh) {
		printf("Unable to read inode %s:%08lx\n", bdevname(dev), ino);
		unlock_kernel();
		return;
	}
	off = (ino - BFS_ROOT_INO)%BFS_INODES_PER_BLOCK;
	di = (struct bfs_inode *)bh->b_data + off;
	if (di->i_ino) {
		s->su_freeb += BFS_FILEBLOCKS(di);
		s->su_freei++;
		clear_bit(di->i_ino, s->su_imap);
		dump_imap("delete_inode", s);
	}
	di->i_ino = 0;
	di->i_sblock = 0;
	mark_buffer_dirty(bh);
	brelse(bh);

	/* if this was the last file, make the previous 
	   block "last files last block" even if there is no real file there,
	   saves us 1 gap */
	if (s->su_lf_eblk == inode->iu_eblock) {
		s->su_lf_eblk = inode->iu_sblock - 1;
		mark_buffer_dirty(s->su_sbh);
	}
	unlock_kernel();
	clear_inode(inode);
}

static void bfs_put_super(struct super_block *s)
{
	brelse(s->su_sbh);
	kfree(s->su_imap);
}

static int bfs_statfs(struct super_block *s, struct statfs *buf)
{
	buf->f_type = BFS_MAGIC;
	buf->f_bsize = s->s_blocksize;
	buf->f_blocks = s->su_blocks;
	buf->f_bfree = buf->f_bavail = s->su_freeb;
	buf->f_files = s->su_lasti + 1 - BFS_ROOT_INO;
	buf->f_ffree = s->su_freei;
	buf->f_fsid.val[0] = kdev_t_to_nr(s->s_dev);
	buf->f_namelen = BFS_NAMELEN;
	return 0;
}

static void bfs_write_super(struct super_block *s)
{
	if (!(s->s_flags & MS_RDONLY))
		mark_buffer_dirty(s->su_sbh);
	s->s_dirt = 0;
}

static struct super_operations bfs_sops = {
	read_inode:	bfs_read_inode,
	write_inode:	bfs_write_inode,
	delete_inode:	bfs_delete_inode,
	put_super:	bfs_put_super,
	write_super:	bfs_write_super,
	statfs:		bfs_statfs,
};

void dump_imap(const char *prefix, struct super_block * s)
{
#if 0
	int i;
	char *tmpbuf = (char *)get_free_page(GFP_KERNEL);

	if (!tmpbuf)
		return;
	for (i=s->su_lasti; i>=0; i--) {
		if (i>PAGE_SIZE-100) break;
		if (test_bit(i, s->su_imap))
			strcat(tmpbuf, "1");
		else
			strcat(tmpbuf, "0");
	}
	printk(KERN_ERR "BFS-fs: %s: lasti=%08lx <%s>\n", prefix, s->su_lasti, tmpbuf);
	free_page((unsigned long)tmpbuf);
#endif
}

static struct super_block * bfs_read_super(struct super_block * s, 
	void * data, int silent)
{
	kdev_t dev;
	struct buffer_head * bh;
	struct bfs_super_block * bfs_sb;
	struct inode * inode;
	int i, imap_len;

	dev = s->s_dev;
	set_blocksize(dev, BFS_BSIZE);
	s->s_blocksize = BFS_BSIZE;
	s->s_blocksize_bits = BFS_BSIZE_BITS;

	bh = sb_bread(s, 0);
	if(!bh)
		goto out;
	bfs_sb = (struct bfs_super_block *)bh->b_data;
	if (bfs_sb->s_magic != BFS_MAGIC) {
		if (!silent)
			printf("No BFS filesystem on %s (magic=%08x)\n", 
				bdevname(dev), bfs_sb->s_magic);
		goto out;
	}
	if (BFS_UNCLEAN(bfs_sb, s) && !silent)
		printf("%s is unclean, continuing\n", bdevname(dev));

	s->s_magic = BFS_MAGIC;
	s->su_bfs_sb = bfs_sb;
	s->su_sbh = bh;
	s->su_lasti = (bfs_sb->s_start - BFS_BSIZE)/sizeof(struct bfs_inode) 
			+ BFS_ROOT_INO - 1;

	imap_len = s->su_lasti/8 + 1;
	s->su_imap = kmalloc(imap_len, GFP_KERNEL);
	if (!s->su_imap)
		goto out;
	memset(s->su_imap, 0, imap_len);
	for (i=0; i<BFS_ROOT_INO; i++) 
		set_bit(i, s->su_imap);

	s->s_op = &bfs_sops;
	inode = iget(s, BFS_ROOT_INO);
	if (!inode) {
		kfree(s->su_imap);
		goto out;
	}
	s->s_root = d_alloc_root(inode);
	if (!s->s_root) {
		iput(inode);
		kfree(s->su_imap);
		goto out;
	}

	s->su_blocks = (bfs_sb->s_end + 1)>>BFS_BSIZE_BITS; /* for statfs(2) */
	s->su_freeb = (bfs_sb->s_end + 1 - bfs_sb->s_start)>>BFS_BSIZE_BITS;
	s->su_freei = 0;
	s->su_lf_eblk = 0;
	s->su_lf_sblk = 0;
	s->su_lf_ioff = 0;
	for (i=BFS_ROOT_INO; i<=s->su_lasti; i++) {
		inode = iget(s,i);
		if (inode->iu_dsk_ino == 0)
			s->su_freei++;
		else {
			set_bit(i, s->su_imap);
			s->su_freeb -= inode->i_blocks;
			if (inode->iu_eblock > s->su_lf_eblk) {
				s->su_lf_eblk = inode->iu_eblock;
				s->su_lf_sblk = inode->iu_sblock;
				s->su_lf_ioff = BFS_INO2OFF(i);
			}
		}
		iput(inode);
	}
	if (!(s->s_flags & MS_RDONLY)) {
		mark_buffer_dirty(bh);
		s->s_dirt = 1;
	} 
	dump_imap("read_super", s);
	return s;

out:
	brelse(bh);
	return NULL;
}

static DECLARE_FSTYPE_DEV(bfs_fs_type, "bfs", bfs_read_super);

static int __init init_bfs_fs(void)
{
	return register_filesystem(&bfs_fs_type);
}

static void __exit exit_bfs_fs(void)
{
	unregister_filesystem(&bfs_fs_type);
}

module_init(init_bfs_fs)
module_exit(exit_bfs_fs)
