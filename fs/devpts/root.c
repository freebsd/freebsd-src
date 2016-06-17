/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/fs/devpts/root.c
 *
 *  Copyright 1998 H. Peter Anvin -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/param.h>
#include <linux/string.h>
#include "devpts_i.h"

static int devpts_root_readdir(struct file *,void *,filldir_t);
static struct dentry *devpts_root_lookup(struct inode *,struct dentry *);
static int devpts_revalidate(struct dentry *, int);

struct file_operations devpts_root_operations = {
	read:		generic_read_dir,
	readdir:	devpts_root_readdir,
};

struct inode_operations devpts_root_inode_operations = {
	lookup:		devpts_root_lookup,
};

static struct dentry_operations devpts_dentry_operations = {
	d_revalidate:	devpts_revalidate,
};

/*
 * The normal naming convention is simply /dev/pts/<number>; this conforms
 * to the System V naming convention
 */

#define genptsname(buf,num) sprintf(buf, "%d", num)

static int devpts_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode * inode = filp->f_dentry->d_inode;
	struct devpts_sb_info * sbi = SBI(filp->f_dentry->d_inode->i_sb);
	off_t nr;
	char numbuf[16];

	nr = filp->f_pos;

	switch(nr)
	{
	case 0:
		if (filldir(dirent, ".", 1, nr, inode->i_ino, DT_DIR) < 0)
			return 0;
		filp->f_pos = ++nr;
		/* fall through */
	case 1:
		if (filldir(dirent, "..", 2, nr, inode->i_ino, DT_DIR) < 0)
			return 0;
		filp->f_pos = ++nr;
		/* fall through */
	default:
		while ( nr - 2 < sbi->max_ptys ) {
			int ptynr = nr - 2;
			if ( sbi->inodes[ptynr] ) {
				genptsname(numbuf, ptynr);
				if ( filldir(dirent, numbuf, strlen(numbuf), nr, nr, DT_CHR) < 0 )
					return 0;
			}
			filp->f_pos = ++nr;
		}
		break;
	}

	return 0;
}

/*
 * Revalidate is called on every cache lookup.  We use it to check that
 * the pty really does still exist.  Never revalidate negative dentries;
 * for simplicity (fix later?)
 */
static int devpts_revalidate(struct dentry * dentry, int flags)
{
	struct devpts_sb_info *sbi;

	if ( !dentry->d_inode )
		return 0;

	sbi = SBI(dentry->d_inode->i_sb);

	return ( sbi->inodes[dentry->d_inode->i_ino - 2] == dentry->d_inode );
}

static struct dentry *devpts_root_lookup(struct inode * dir, struct dentry * dentry)
{
	struct devpts_sb_info *sbi = SBI(dir->i_sb);
	unsigned int entry;
	int i;
	const char *p;

	dentry->d_op    = &devpts_dentry_operations;

	if ( dentry->d_name.len == 1 && dentry->d_name.name[0] == '0' ) {
		entry = 0;
	} else if ( dentry->d_name.len < 1 ) {
		return NULL;
	} else {
		p = dentry->d_name.name;
		if ( *p < '1' || *p > '9' )
			return NULL;
		entry = *p++ - '0';

		for ( i = dentry->d_name.len-1 ; i ; i-- ) {
			unsigned int nentry = *p++ - '0';
			if ( nentry > 9 )
				return NULL;
			if ( entry >= ~0U/10 )
				return NULL;
			entry = nentry + entry * 10;
		}
	}

	if ( entry >= sbi->max_ptys )
		return NULL;

	if ( sbi->inodes[entry] )
		atomic_inc(&sbi->inodes[entry]->i_count);
	
	d_add(dentry, sbi->inodes[entry]);

	return NULL;
}
