/* $Id: capifs.c,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 * 
 * Copyright 2000 by Carsten Paeth <calle@calle.de>
 *
 * Heavily based on devpts filesystem from H. Peter Anvin
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/param.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

MODULE_DESCRIPTION("CAPI4Linux: /dev/capi/ filesystem");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");

static char *revision = "$Revision: 1.1.4.1 $";

struct capifs_ncci {
	struct inode *inode;
	char used;
	char type;
	unsigned int num;
	kdev_t kdev;
};

struct capifs_sb_info {
	u32 magic;
	struct super_block *next;
	struct super_block **back;
	int setuid;
	int setgid;
	uid_t   uid;
	gid_t   gid;
	umode_t mode;

	unsigned int max_ncci;
	struct capifs_ncci *nccis;
};

#define CAPIFS_SUPER_MAGIC (('C'<<8)|'N')
#define CAPIFS_SBI_MAGIC   (('C'<<24)|('A'<<16)|('P'<<8)|'I')

static inline struct capifs_sb_info *SBI(struct super_block *sb)
{
	return (struct capifs_sb_info *)(sb->u.generic_sbp);
}

/* ------------------------------------------------------------------ */

static int capifs_root_readdir(struct file *,void *,filldir_t);
static struct dentry *capifs_root_lookup(struct inode *,struct dentry *);
static int capifs_revalidate(struct dentry *, int);
static struct inode *capifs_new_inode(struct super_block *sb);

static struct file_operations capifs_root_operations = {
	read:		generic_read_dir,
	readdir:	capifs_root_readdir,
};

struct inode_operations capifs_root_inode_operations = {
	lookup: capifs_root_lookup,
};

static struct dentry_operations capifs_dentry_operations = {
	d_revalidate: capifs_revalidate,
};

/*
 * /dev/capi/%d
 * /dev/capi/r%d
 */

static int capifs_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode * inode = filp->f_dentry->d_inode;
	struct capifs_sb_info * sbi = SBI(filp->f_dentry->d_inode->i_sb);
	off_t nr;
	char numbuf[32];

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
		while (nr < sbi->max_ncci) {
			int n = nr - 2;
			struct capifs_ncci *np = &sbi->nccis[n];
			if (np->inode && np->used) {
				char *p = numbuf;
				if (np->type) *p++ = np->type;
				sprintf(p, "%u", np->num);
				if ( filldir(dirent, numbuf, strlen(numbuf), nr, nr, DT_UNKNOWN) < 0 )
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
 * the ncci really does still exist.  Never revalidate negative dentries;
 * for simplicity (fix later?)
 */
static int capifs_revalidate(struct dentry * dentry, int flags)
{
	struct capifs_sb_info *sbi;

	if ( !dentry->d_inode )
		return 0;

	sbi = SBI(dentry->d_inode->i_sb);

	return ( sbi->nccis[dentry->d_inode->i_ino - 2].inode == dentry->d_inode );
}

static struct dentry *capifs_root_lookup(struct inode * dir, struct dentry * dentry)
{
	struct capifs_sb_info *sbi = SBI(dir->i_sb);
	struct capifs_ncci *np;
	unsigned int i;
	char numbuf[32];
	char *p, *tmp;
	unsigned int num;
	char type = 0;

	dentry->d_inode = NULL;	/* Assume failure */
	dentry->d_op    = &capifs_dentry_operations;

	if (dentry->d_name.len >= sizeof(numbuf) )
		return NULL;
	strncpy(numbuf, dentry->d_name.name, dentry->d_name.len);
	numbuf[dentry->d_name.len] = 0;
        p = numbuf;
	if (!isdigit(*p)) type = *p++;
	tmp = p;
	num = (unsigned int)simple_strtoul(p, &tmp, 10);
	if (tmp == p || *tmp)
		return NULL;

	for (i = 0, np = sbi->nccis ; i < sbi->max_ncci; i++, np++) {
		if (np->used && np->num == num && np->type == type)
			break;
	}

	if ( i >= sbi->max_ncci )
		return NULL;

	dentry->d_inode = np->inode;
	if ( dentry->d_inode )
		atomic_inc(&dentry->d_inode->i_count);
	
	d_add(dentry, dentry->d_inode);

	return NULL;
}

/* ------------------------------------------------------------------ */

static struct super_block *mounts = NULL;

static void capifs_put_super(struct super_block *sb)
{
	struct capifs_sb_info *sbi = SBI(sb);
	struct inode *inode;
	int i;

	for ( i = 0 ; i < sbi->max_ncci ; i++ ) {
		if ( (inode = sbi->nccis[i].inode) ) {
			if (atomic_read(&inode->i_count) != 1 )
				printk("capifs_put_super: badness: entry %d count %d\n",
				       i, (unsigned)atomic_read(&inode->i_count));
			inode->i_nlink--;
			iput(inode);
		}
	}

	*sbi->back = sbi->next;
	if ( sbi->next )
		SBI(sbi->next)->back = sbi->back;

	kfree(sbi->nccis);
	kfree(sbi);
}

static int capifs_statfs(struct super_block *sb, struct statfs *buf);

static struct super_operations capifs_sops = {
	put_super:	capifs_put_super,
	statfs:		capifs_statfs,
};

static int capifs_parse_options(char *options, struct capifs_sb_info *sbi)
{
	int setuid = 0;
	int setgid = 0;
	uid_t uid = 0;
	gid_t gid = 0;
	umode_t mode = 0600;
	unsigned int maxncci = 512;
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
		else if (!strcmp(this_char,"maxncci")) {
			if (!value || !*value)
				return 1;
			maxncci = simple_strtoul(value,&value,8);
			if (*value)
				return 1;
		}
		else
			return 1;
	}
	sbi->setuid   = setuid;
	sbi->setgid   = setgid;
	sbi->uid      = uid;
	sbi->gid      = gid;
	sbi->mode     = mode & ~S_IFMT;
	sbi->max_ncci = maxncci;

	return 0;
}

struct super_block *capifs_read_super(struct super_block *s, void *data,
				      int silent)
{
	struct inode * root_inode;
	struct dentry * root;
	struct capifs_sb_info *sbi;

	/* Super block already completed? */
	if (s->s_root)
		goto out;

	sbi = (struct capifs_sb_info *) kmalloc(sizeof(struct capifs_sb_info), GFP_KERNEL);
	if ( !sbi )
		goto fail;

	memset(sbi, 0, sizeof(struct capifs_sb_info));
	sbi->magic  = CAPIFS_SBI_MAGIC;

	if ( capifs_parse_options(data,sbi) ) {
		kfree(sbi);
		printk("capifs: called with bogus options\n");
		goto fail;
	}

	sbi->nccis = kmalloc(sizeof(struct capifs_ncci) * sbi->max_ncci, GFP_KERNEL);
	if ( !sbi->nccis ) {
		kfree(sbi);
		goto fail;
	}
	memset(sbi->nccis, 0, sizeof(struct capifs_ncci) * sbi->max_ncci);

	s->u.generic_sbp = (void *) sbi;
	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = CAPIFS_SUPER_MAGIC;
	s->s_op = &capifs_sops;
	s->s_root = NULL;

	/*
	 * Get the root inode and dentry, but defer checking for errors.
	 */
	root_inode = capifs_new_inode(s);
	if (root_inode) {
		root_inode->i_ino = 1;
		root_inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
		root_inode->i_op = &capifs_root_inode_operations;
		root_inode->i_fop = &capifs_root_operations;
		root_inode->i_nlink = 2;
	} 
	root = d_alloc_root(root_inode);

	/*
	 * Check whether somebody else completed the super block.
	 */
	if (s->s_root) {
		if (root) dput(root);
		else iput(root_inode);
		goto out;
	}

	if (!root) {
		printk("capifs: get root dentry failed\n");
		/*
	 	* iput() can block, so we clear the super block first.
	 	*/
		iput(root_inode);
		kfree(sbi->nccis);
		kfree(sbi);
		goto fail;
	}

	/*
	 * Check whether somebody else completed the super block.
	 */
	if (s->s_root)
		goto out;
	
	/*
	 * Success! Install the root dentry now to indicate completion.
	 */
	s->s_root = root;

	sbi->next = mounts;
	if ( sbi->next )
		SBI(sbi->next)->back = &(sbi->next);
	sbi->back = &mounts;
	mounts = s;

out:	/* Success ... somebody else completed the super block for us. */ 
	return s;
fail:
	return NULL;
}

static int capifs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = CAPIFS_SUPER_MAGIC;
	buf->f_bsize = 1024;
	buf->f_blocks = 0;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = 0;
	buf->f_ffree = 0;
	buf->f_namelen = NAME_MAX;
	return 0;
}

static struct inode *capifs_new_inode(struct super_block *sb)
{
	struct inode *inode = new_inode(sb);
	if (inode) {
		inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
		inode->i_blocks = 0;
		inode->i_blksize = 1024;
		inode->i_uid = inode->i_gid = 0;
	}
	return inode;
}

static DECLARE_FSTYPE(capifs_fs_type, "capifs", capifs_read_super, 0);

void capifs_new_ncci(char type, unsigned int num, kdev_t device)
{
	struct super_block *sb;
	struct capifs_sb_info *sbi;
	struct capifs_ncci *np;
	ino_t ino;

	for ( sb = mounts ; sb ; sb = sbi->next ) {
		sbi = SBI(sb);

		for (ino = 0, np = sbi->nccis ; ino < sbi->max_ncci; ino++, np++) {
			if (np->used == 0) {
				np->used = 1;
				np->type = type;
				np->num = num;
				np->kdev = device;
				break;
			}
		}
		if ( ino >= sbi->max_ncci )
			continue;

		if ((np->inode = capifs_new_inode(sb)) != NULL) {
			struct inode *inode = np->inode;
			inode->i_uid = sbi->setuid ? sbi->uid : current->fsuid;
			inode->i_gid = sbi->setgid ? sbi->gid : current->fsgid;
			inode->i_nlink = 1;
			inode->i_ino = ino + 2;
			init_special_inode(inode, sbi->mode|S_IFCHR, np->kdev);
		}
	}
}

void capifs_free_ncci(char type, unsigned int num)
{
	struct super_block *sb;
	struct capifs_sb_info *sbi;
	struct inode *inode;
	struct capifs_ncci *np;
	ino_t ino;

	for ( sb = mounts ; sb ; sb = sbi->next ) {
		sbi = SBI(sb);

		for (ino = 0, np = sbi->nccis ; ino < sbi->max_ncci; ino++, np++) {
			if (!np->used || np->type != type || np->num != num)
				continue;
			if (np->inode) {
				inode = np->inode;
				np->inode = 0;
				np->used = 0;
				inode->i_nlink--;
				iput(inode);
				break;
			}
		}
	}
}

static int __init capifs_init(void)
{
	char rev[32];
	char *p;
	int err;

	MOD_INC_USE_COUNT;

	if ((p = strchr(revision, ':')) != 0 && p[1]) {
		strncpy(rev, p + 2, sizeof(rev));
		rev[sizeof(rev)-1] = 0;
		if ((p = strchr(rev, '$')) != 0 && p > rev)
		   *(p-1) = 0;
	} else
		strcpy(rev, "1.0");

	err = register_filesystem(&capifs_fs_type);
	if (err) {
		MOD_DEC_USE_COUNT;
		return err;
	}
        printk(KERN_NOTICE "capifs: Rev %s\n", rev);
	MOD_DEC_USE_COUNT;
	return 0;
}

static void __exit capifs_exit(void)
{
	unregister_filesystem(&capifs_fs_type);
}

EXPORT_SYMBOL(capifs_new_ncci);
EXPORT_SYMBOL(capifs_free_ncci);

module_init(capifs_init);
module_exit(capifs_exit);
