/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/fs/devpts/inode.c
 *
 *  Copyright 1998 H. Peter Anvin -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include <linux/module.h>

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/tty.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "devpts_i.h"

static struct vfsmount *devpts_mnt;

static void devpts_put_super(struct super_block *sb)
{
	struct devpts_sb_info *sbi = SBI(sb);
	struct inode *inode;
	int i;

	for ( i = 0 ; i < sbi->max_ptys ; i++ ) {
		if ( (inode = sbi->inodes[i]) ) {
			if ( atomic_read(&inode->i_count) != 1 )
				printk("devpts_put_super: badness: entry %d count %d\n",
				       i, atomic_read(&inode->i_count));
			inode->i_nlink--;
			iput(inode);
		}
	}
	kfree(sbi->inodes);
	kfree(sbi);
}

static int devpts_statfs(struct super_block *sb, struct statfs *buf);
static int devpts_remount (struct super_block * sb, int * flags, char * data);

static struct super_operations devpts_sops = {
	put_super:	devpts_put_super,
	statfs:		devpts_statfs,
	remount_fs:	devpts_remount,
};

static int devpts_parse_options(char *options, struct devpts_sb_info *sbi)
{
	int setuid = 0;
	int setgid = 0;
	uid_t uid = 0;
	gid_t gid = 0;
	umode_t mode = 0600;
	char *this_char, *value;

	this_char = NULL;
	if ( options )
		this_char = strtok(options,",");
	for ( ; this_char; this_char = strtok(NULL,",")) {
		if ((value = strchr(this_char,'=')) != NULL)
			*value++ = 0;
		if (!strcmp(this_char,"uid")) {
			if (!value || !*value)
				return 1;
			uid = simple_strtoul(value,&value,0);
			if (*value)
				return 1;
			setuid = 1;
		}
		else if (!strcmp(this_char,"gid")) {
			if (!value || !*value)
				return 1;
			gid = simple_strtoul(value,&value,0);
			if (*value)
				return 1;
			setgid = 1;
		}
		else if (!strcmp(this_char,"mode")) {
			if (!value || !*value)
				return 1;
			mode = simple_strtoul(value,&value,8);
			if (*value)
				return 1;
		}
		else
			return 1;
	}
	sbi->setuid  = setuid;
	sbi->setgid  = setgid;
	sbi->uid     = uid;
	sbi->gid     = gid;
	sbi->mode    = mode & ~S_IFMT;

	return 0;
}

static int devpts_remount(struct super_block * sb, int * flags, char * data)
{
	struct devpts_sb_info *sbi = sb->u.generic_sbp;
	int res = devpts_parse_options(data,sbi);
	if (res) {
		printk("devpts: called with bogus options\n");
		return -EINVAL;
	}
	return 0;
}

struct super_block *devpts_read_super(struct super_block *s, void *data,
				      int silent)
{
	struct inode * inode;
	struct devpts_sb_info *sbi;

	sbi = (struct devpts_sb_info *) kmalloc(sizeof(struct devpts_sb_info), GFP_KERNEL);
	if ( !sbi )
		goto fail;

	sbi->magic  = DEVPTS_SBI_MAGIC;
	sbi->max_ptys = unix98_max_ptys;
	sbi->inodes = kmalloc(sizeof(struct inode *) * sbi->max_ptys, GFP_KERNEL);
	if ( !sbi->inodes )
		goto fail_free;
	memset(sbi->inodes, 0, sizeof(struct inode *) * sbi->max_ptys);

	if ( devpts_parse_options(data,sbi) && !silent) {
		printk("devpts: called with bogus options\n");
		goto fail_free;
	}

	inode = new_inode(s);
	if (!inode)
		goto fail_free;
	inode->i_ino = 1;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_blocks = 0;
	inode->i_blksize = 1024;
	inode->i_uid = inode->i_gid = 0;
	inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
	inode->i_op = &devpts_root_inode_operations;
	inode->i_fop = &devpts_root_operations;
	inode->i_nlink = 2;

	s->u.generic_sbp = (void *) sbi;
	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = DEVPTS_SUPER_MAGIC;
	s->s_op = &devpts_sops;
	s->s_root = d_alloc_root(inode);
	if (s->s_root)
		return s;
	
	printk("devpts: get root dentry failed\n");
	iput(inode);
fail_free:
	kfree(sbi);
fail:
	return NULL;
}

static int devpts_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = DEVPTS_SUPER_MAGIC;
	buf->f_bsize = 1024;
	buf->f_namelen = NAME_MAX;
	return 0;
}

static DECLARE_FSTYPE(devpts_fs_type, "devpts", devpts_read_super, FS_SINGLE);

void devpts_pty_new(int number, kdev_t device)
{
	struct super_block *sb = devpts_mnt->mnt_sb;
	struct devpts_sb_info *sbi = SBI(sb);
	struct inode *inode;
		
	if ( sbi->inodes[number] )
		return; /* Already registered, this does happen */
		
	inode = new_inode(sb);
	if (!inode)
		return;
	inode->i_ino = number+2;
	inode->i_blocks = 0;
	inode->i_blksize = 1024;
	inode->i_uid = sbi->setuid ? sbi->uid : current->fsuid;
	inode->i_gid = sbi->setgid ? sbi->gid : current->fsgid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	init_special_inode(inode, S_IFCHR|sbi->mode, kdev_t_to_nr(device));

	if ( sbi->inodes[number] ) {
		iput(inode);
		return;
	}
	sbi->inodes[number] = inode;
}

void devpts_pty_kill(int number)
{
	struct super_block *sb = devpts_mnt->mnt_sb;
	struct devpts_sb_info *sbi = SBI(sb);
	struct inode *inode = sbi->inodes[number];

	if ( inode ) {
		sbi->inodes[number] = NULL;
		inode->i_nlink--;
		iput(inode);
	}
}

static int __init init_devpts_fs(void)
{
	int err = register_filesystem(&devpts_fs_type);
	if (!err) {
		devpts_mnt = kern_mount(&devpts_fs_type);
		err = PTR_ERR(devpts_mnt);
		if (!IS_ERR(devpts_mnt))
			err = 0;
#ifdef MODULE
		if ( !err ) {
			devpts_upcall_new  = devpts_pty_new;
			devpts_upcall_kill = devpts_pty_kill;
		}
#endif
	}
	return err;
}

static void __exit exit_devpts_fs(void)
{
#ifdef MODULE
	devpts_upcall_new  = NULL;
	devpts_upcall_kill = NULL;
#endif
	unregister_filesystem(&devpts_fs_type);
	kern_umount(devpts_mnt);
}

module_init(init_devpts_fs)
module_exit(exit_devpts_fs)
MODULE_LICENSE("GPL");

