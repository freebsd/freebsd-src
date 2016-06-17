/*
 *  linux/fs/sysv/inode.c
 *
 *  minix/inode.c
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  xenix/inode.c
 *  Copyright (C) 1992  Doug Evans
 *
 *  coh/inode.c
 *  Copyright (C) 1993  Pascal Haible, Bruno Haible
 *
 *  sysv/inode.c
 *  Copyright (C) 1993  Paul B. Monday
 *
 *  sysv/inode.c
 *  Copyright (C) 1993  Bruno Haible
 *  Copyright (C) 1997, 1998  Krzysztof G. Baranowski
 *
 *  This file contains code for allocating/freeing inodes and for read/writing
 *  the superblock.
 */

#include <linux/fs.h>
#include <linux/sysv_fs.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/highuid.h>
#include <asm/byteorder.h>

/* This is only called on sync() and umount(), when s_dirt=1. */
static void sysv_write_super(struct super_block *sb)
{
	if (!(sb->s_flags & MS_RDONLY)) {
		/* If we are going to write out the super block,
		   then attach current time stamp.
		   But if the filesystem was marked clean, keep it clean. */
		unsigned long time = CURRENT_TIME;
		unsigned long old_time = fs32_to_cpu(sb, *sb->sv_sb_time);
		if (sb->sv_type == FSTYPE_SYSV4)
			if (*sb->sv_sb_state == cpu_to_fs32(sb, 0x7c269d38 - old_time))
				*sb->sv_sb_state = cpu_to_fs32(sb, 0x7c269d38 - time);
		*sb->sv_sb_time = cpu_to_fs32(sb, time);
		mark_buffer_dirty(sb->sv_bh2);
	}
	sb->s_dirt = 0;
}

static void sysv_put_super(struct super_block *sb)
{
	if (!(sb->s_flags & MS_RDONLY)) {
		/* XXX ext2 also updates the state here */
		mark_buffer_dirty(sb->sv_bh1);
		if (sb->sv_bh1 != sb->sv_bh2)
			mark_buffer_dirty(sb->sv_bh2);
	}

	brelse(sb->sv_bh1);
	if (sb->sv_bh1 != sb->sv_bh2)
		brelse(sb->sv_bh2);
}

static int sysv_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = sb->sv_ndatazones;
	buf->f_bavail = buf->f_bfree = sysv_count_free_blocks(sb);
	buf->f_files = sb->sv_ninodes;
	buf->f_ffree = sysv_count_free_inodes(sb);
	buf->f_namelen = SYSV_NAMELEN;
	return 0;
}

/* 
 * NXI <-> N0XI for PDP, XIN <-> XIN0 for le32, NIX <-> 0NIX for be32
 */
static inline void read3byte(struct super_block *sb,
	unsigned char * from, unsigned char * to)
{
	if (sb->sv_bytesex == BYTESEX_PDP) {
		to[0] = from[0];
		to[1] = 0;
		to[2] = from[1];
		to[3] = from[2];
	} else if (sb->sv_bytesex == BYTESEX_LE) {
		to[0] = from[0];
		to[1] = from[1];
		to[2] = from[2];
		to[3] = 0;
	} else {
		to[0] = 0;
		to[1] = from[0];
		to[2] = from[1];
		to[3] = from[2];
	}
}

static inline void write3byte(struct super_block *sb,
	unsigned char * from, unsigned char * to)
{
	if (sb->sv_bytesex == BYTESEX_PDP) {
		to[0] = from[0];
		to[1] = from[2];
		to[2] = from[3];
	} else if (sb->sv_bytesex == BYTESEX_LE) {
		to[0] = from[0];
		to[1] = from[1];
		to[2] = from[2];
	} else {
		to[0] = from[1];
		to[1] = from[2];
		to[2] = from[3];
	}
}

static struct inode_operations sysv_symlink_inode_operations = {
	readlink:	page_readlink,
	follow_link:	page_follow_link,
};

void sysv_set_inode(struct inode *inode, dev_t rdev)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &sysv_file_inode_operations;
		inode->i_fop = &sysv_file_operations;
		inode->i_mapping->a_ops = &sysv_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &sysv_dir_inode_operations;
		inode->i_fop = &sysv_dir_operations;
		inode->i_mapping->a_ops = &sysv_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		if (inode->i_blocks) {
			inode->i_op = &sysv_symlink_inode_operations;
			inode->i_mapping->a_ops = &sysv_aops;
		} else
			inode->i_op = &sysv_fast_symlink_inode_operations;
	} else
		init_special_inode(inode, inode->i_mode, rdev);
}

static void sysv_read_inode(struct inode *inode)
{
	struct super_block * sb = inode->i_sb;
	struct buffer_head * bh;
	struct sysv_inode * raw_inode;
	unsigned int block, ino;
	dev_t rdev = 0;

	ino = inode->i_ino;
	if (!ino || ino > sb->sv_ninodes) {
		printk("Bad inode number on dev %s"
		       ": %d is out of range\n",
		       kdevname(inode->i_dev), ino);
		goto bad_inode;
	}
	raw_inode = sysv_raw_inode(sb, ino, &bh);
	if (!raw_inode) {
		printk("Major problem: unable to read inode from dev %s\n",
		       bdevname(inode->i_dev));
		goto bad_inode;
	}
	/* SystemV FS: kludge permissions if ino==SYSV_ROOT_INO ?? */
	inode->i_mode = fs16_to_cpu(sb, raw_inode->i_mode);
	inode->i_uid = (uid_t)fs16_to_cpu(sb, raw_inode->i_uid);
	inode->i_gid = (gid_t)fs16_to_cpu(sb, raw_inode->i_gid);
	inode->i_nlink = fs16_to_cpu(sb, raw_inode->i_nlink);
	inode->i_size = fs32_to_cpu(sb, raw_inode->i_size);
	inode->i_atime = fs32_to_cpu(sb, raw_inode->i_atime);
	inode->i_mtime = fs32_to_cpu(sb, raw_inode->i_mtime);
	inode->i_ctime = fs32_to_cpu(sb, raw_inode->i_ctime);
	inode->i_blocks = inode->i_blksize = 0;
	for (block = 0; block < 10+1+1+1; block++)
		read3byte(sb, &raw_inode->i_a.i_addb[3*block],
			(unsigned char*)&inode->u.sysv_i.i_data[block]);
	brelse(bh);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		rdev = (u16)fs32_to_cpu(sb, inode->u.sysv_i.i_data[0]);
	inode->u.sysv_i.i_dir_start_lookup = 0;
	sysv_set_inode(inode, rdev);
	return;

bad_inode:
	make_bad_inode(inode);
	return;
}

static struct buffer_head * sysv_update_inode(struct inode * inode)
{
	struct super_block * sb = inode->i_sb;
	struct buffer_head * bh;
	struct sysv_inode * raw_inode;
	unsigned int ino, block;

	ino = inode->i_ino;
	if (!ino || ino > sb->sv_ninodes) {
		printk("Bad inode number on dev %s: %d is out of range\n",
		       bdevname(inode->i_dev), ino);
		return 0;
	}
	raw_inode = sysv_raw_inode(sb, ino, &bh);
	if (!raw_inode) {
		printk("unable to read i-node block\n");
		return 0;
	}

	raw_inode->i_mode = cpu_to_fs16(sb, inode->i_mode);
	raw_inode->i_uid = cpu_to_fs16(sb, fs_high2lowuid(inode->i_uid));
	raw_inode->i_gid = cpu_to_fs16(sb, fs_high2lowgid(inode->i_gid));
	raw_inode->i_nlink = cpu_to_fs16(sb, inode->i_nlink);
	raw_inode->i_size = cpu_to_fs32(sb, inode->i_size);
	raw_inode->i_atime = cpu_to_fs32(sb, inode->i_atime);
	raw_inode->i_mtime = cpu_to_fs32(sb, inode->i_mtime);
	raw_inode->i_ctime = cpu_to_fs32(sb, inode->i_ctime);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		inode->u.sysv_i.i_data[0] = 
			cpu_to_fs32(sb, kdev_t_to_nr(inode->i_rdev));
	for (block = 0; block < 10+1+1+1; block++)
		write3byte(sb, (unsigned char*)&inode->u.sysv_i.i_data[block],
			&raw_inode->i_a.i_addb[3*block]);
	mark_buffer_dirty(bh);
	return bh;
}

void sysv_write_inode(struct inode * inode, int wait)
{
	struct buffer_head *bh;
	lock_kernel();
	bh = sysv_update_inode(inode);
	brelse(bh);
	unlock_kernel();
}

int sysv_sync_inode(struct inode * inode)
{
        int err = 0;
        struct buffer_head *bh;

        bh = sysv_update_inode(inode);
        if (bh && buffer_dirty(bh)) {
                ll_rw_block(WRITE, 1, &bh);
                wait_on_buffer(bh);
                if (buffer_req(bh) && !buffer_uptodate(bh)) {
                        printk ("IO error syncing sysv inode [%s:%08lx]\n",
                                bdevname(inode->i_dev), inode->i_ino);
                        err = -1;
                }
        }
        else if (!bh)
                err = -1;
        brelse (bh);
        return err;
}

static void sysv_delete_inode(struct inode *inode)
{
	lock_kernel();
	inode->i_size = 0;
	sysv_truncate(inode);
	sysv_free_inode(inode);
	unlock_kernel();
}

struct super_operations sysv_sops = {
	read_inode:	sysv_read_inode,
	write_inode:	sysv_write_inode,
	delete_inode:	sysv_delete_inode,
	put_super:	sysv_put_super,
	write_super:	sysv_write_super,
	statfs:		sysv_statfs,
};
