/*
 * proc/fs/generic.c --- generic routines for the proc-fs
 *
 * This file contains generic proc-fs routines for handling
 * directories and files.
 * 
 * Copyright (C) 1991, 1992 Linus Torvalds.
 * Copyright (C) 1997 Theodore Ts'o
 */

#include <asm/uaccess.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#define __NO_VERSION__
#include <linux/module.h>
#include <asm/bitops.h>

static ssize_t proc_file_read(struct file * file, char * buf,
			      size_t nbytes, loff_t *ppos);
static ssize_t proc_file_write(struct file * file, const char * buffer,
			       size_t count, loff_t *ppos);
static loff_t proc_file_lseek(struct file *, loff_t, int);

int proc_match(int len, const char *name,struct proc_dir_entry * de)
{
	if (!de || !de->low_ino)
		return 0;
	if (de->namelen != len)
		return 0;
	return !memcmp(name, de->name, len);
}

static struct file_operations proc_file_operations = {
	llseek:		proc_file_lseek,
	read:		proc_file_read,
	write:		proc_file_write,
};

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

/* buffer size is one page but our output routines use some slack for overruns */
#define PROC_BLOCK_SIZE	(PAGE_SIZE - 1024)

static ssize_t
proc_file_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	struct inode * inode = file->f_dentry->d_inode;
	char 	*page;
	ssize_t	retval=0;
	int	eof=0;
	ssize_t	n, count;
	char	*start;
	struct proc_dir_entry * dp;

	dp = (struct proc_dir_entry *) inode->u.generic_ip;
	if (!(page = (char*) __get_free_page(GFP_KERNEL)))
		return -ENOMEM;

	while ((nbytes > 0) && !eof)
	{
		count = MIN(PROC_BLOCK_SIZE, nbytes);

		start = NULL;
		if (dp->get_info) {
			/*
			 * Handle backwards compatibility with the old net
			 * routines.
			 */
			n = dp->get_info(page, &start, *ppos, count);
			if (n < count)
				eof = 1;
		} else if (dp->read_proc) {
			n = dp->read_proc(page, &start, *ppos,
					  count, &eof, dp->data);
		} else
			break;

		if (!start) {
			/*
			 * For proc files that are less than 4k
			 */
			start = page + *ppos;
			n -= *ppos;
			if (n <= 0)
				break;
			if (n > count)
				n = count;
		}
		if (n == 0)
			break;	/* End of file */
		if (n < 0) {
			if (retval == 0)
				retval = n;
			break;
		}
		
		/* This is a hack to allow mangling of file pos independent
 		 * of actual bytes read.  Simply place the data at page,
 		 * return the bytes, and set `start' to the desired offset
 		 * as an unsigned int. - Paul.Russell@rustcorp.com.au
		 */
 		n -= copy_to_user(buf, start < page ? page : start, n);
		if (n == 0) {
			if (retval == 0)
				retval = -EFAULT;
			break;
		}

		*ppos += start < page ? (long)start : n; /* Move down the file */
		nbytes -= n;
		buf += n;
		retval += n;
	}
	free_page((unsigned long) page);
	return retval;
}

static ssize_t
proc_file_write(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry * dp;
	
	dp = (struct proc_dir_entry *) inode->u.generic_ip;

	if (!dp->write_proc)
		return -EIO;

	/* FIXME: does this routine need ppos?  probably... */
	return dp->write_proc(file, buffer, count, dp->data);
}


static loff_t
proc_file_lseek(struct file * file, loff_t offset, int origin)
{
	long long retval;

	switch (origin) {
		case 2:
			offset += file->f_dentry->d_inode->i_size;
			break;
		case 1:
			offset += file->f_pos;
	}
	retval = -EINVAL;
	if (offset>=0 && (unsigned long long)offset<=file->f_dentry->d_inode->i_sb->s_maxbytes) {
		if (offset != file->f_pos) {
			file->f_pos = offset;
			file->f_reada = 0;
		}
		retval = offset;
	}
	/* RED-PEN user can fake an error here by setting offset to >=-4095 && <0  */
	return retval;
}

/*
 * This function parses a name such as "tty/driver/serial", and
 * returns the struct proc_dir_entry for "/proc/tty/driver", and
 * returns "serial" in residual.
 */
static int xlate_proc_name(const char *name,
			   struct proc_dir_entry **ret, const char **residual)
{
	const char     		*cp = name, *next;
	struct proc_dir_entry	*de;
	int			len;

	de = &proc_root;
	while (1) {
		next = strchr(cp, '/');
		if (!next)
			break;

		len = next - cp;
		for (de = de->subdir; de ; de = de->next) {
			if (proc_match(len, cp, de))
				break;
		}
		if (!de)
			return -ENOENT;
		cp += len + 1;
	}
	*residual = cp;
	*ret = de;
	return 0;
}

static unsigned long proc_alloc_map[(PROC_NDYNAMIC + BITS_PER_LONG - 1) / BITS_PER_LONG];

spinlock_t proc_alloc_map_lock = SPIN_LOCK_UNLOCKED;

static int make_inode_number(void)
{
	int i;
	spin_lock(&proc_alloc_map_lock);
	i = find_first_zero_bit(proc_alloc_map, PROC_NDYNAMIC);
	if (i < 0 || i >= PROC_NDYNAMIC) {
		i = -1;
		goto out;
	}
	set_bit(i, proc_alloc_map);
	i += PROC_DYNAMIC_FIRST;
out:
	spin_unlock(&proc_alloc_map_lock);
	return i;
}

static int proc_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	char *s=((struct proc_dir_entry *)dentry->d_inode->u.generic_ip)->data;
	return vfs_readlink(dentry, buffer, buflen, s);
}

static int proc_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *s=((struct proc_dir_entry *)dentry->d_inode->u.generic_ip)->data;
	return vfs_follow_link(nd, s);
}

static struct inode_operations proc_link_inode_operations = {
	readlink:	proc_readlink,
	follow_link:	proc_follow_link,
};

/*
 * As some entries in /proc are volatile, we want to 
 * get rid of unused dentries.  This could be made 
 * smarter: we could keep a "volatile" flag in the 
 * inode to indicate which ones to keep.
 */
static int proc_delete_dentry(struct dentry * dentry)
{
	return 1;
}

static struct dentry_operations proc_dentry_operations =
{
	d_delete:	proc_delete_dentry,
};

/*
 * Don't create negative dentries here, return -ENOENT by hand
 * instead.
 */
struct dentry *proc_lookup(struct inode * dir, struct dentry *dentry)
{
	struct inode *inode;
	struct proc_dir_entry * de;
	int error;

	error = -ENOENT;
	inode = NULL;
	de = (struct proc_dir_entry *) dir->u.generic_ip;
	if (de) {
		for (de = de->subdir; de ; de = de->next) {
			if (!de || !de->low_ino)
				continue;
			if (de->namelen != dentry->d_name.len)
				continue;
			if (!memcmp(dentry->d_name.name, de->name, de->namelen)) {
				int ino = de->low_ino;
				error = -EINVAL;
				inode = proc_get_inode(dir->i_sb, ino, de);
				break;
			}
		}
	}

	if (inode) {
		dentry->d_op = &proc_dentry_operations;
		d_add(dentry, inode);
		return NULL;
	}
	return ERR_PTR(error);
}

/*
 * This returns non-zero if at EOF, so that the /proc
 * root directory can use this and check if it should
 * continue with the <pid> entries..
 *
 * Note that the VFS-layer doesn't care about the return
 * value of the readdir() call, as long as it's non-negative
 * for success..
 */
int proc_readdir(struct file * filp,
	void * dirent, filldir_t filldir)
{
	struct proc_dir_entry * de;
	unsigned int ino;
	int i;
	struct inode *inode = filp->f_dentry->d_inode;

	ino = inode->i_ino;
	de = (struct proc_dir_entry *) inode->u.generic_ip;
	if (!de)
		return -EINVAL;
	i = filp->f_pos;
	switch (i) {
		case 0:
			if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0)
				return 0;
			i++;
			filp->f_pos++;
			/* fall through */
		case 1:
			if (filldir(dirent, "..", 2, i,
				    filp->f_dentry->d_parent->d_inode->i_ino,
				    DT_DIR) < 0)
				return 0;
			i++;
			filp->f_pos++;
			/* fall through */
		default:
			de = de->subdir;
			i -= 2;
			for (;;) {
				if (!de)
					return 1;
				if (!i)
					break;
				de = de->next;
				i--;
			}

			do {
				if (filldir(dirent, de->name, de->namelen, filp->f_pos,
					    de->low_ino, de->mode >> 12) < 0)
					return 0;
				filp->f_pos++;
				de = de->next;
			} while (de);
	}
	return 1;
}

/*
 * These are the generic /proc directory operations. They
 * use the in-memory "struct proc_dir_entry" tree to parse
 * the /proc directory.
 */
static struct file_operations proc_dir_operations = {
	read:			generic_read_dir,
	readdir:		proc_readdir,
};

/*
 * proc directories can do almost nothing..
 */
static struct inode_operations proc_dir_inode_operations = {
	lookup:		proc_lookup,
};

static int proc_register(struct proc_dir_entry * dir, struct proc_dir_entry * dp)
{
	int	i;
	
	i = make_inode_number();
	if (i < 0)
		return -EAGAIN;
	dp->low_ino = i;
	dp->next = dir->subdir;
	dp->parent = dir;
	dir->subdir = dp;
	if (S_ISDIR(dp->mode)) {
		if (dp->proc_iops == NULL) {
			dp->proc_fops = &proc_dir_operations;
			dp->proc_iops = &proc_dir_inode_operations;
		}
		dir->nlink++;
	} else if (S_ISLNK(dp->mode)) {
		if (dp->proc_iops == NULL)
			dp->proc_iops = &proc_link_inode_operations;
	} else if (S_ISREG(dp->mode)) {
		if (dp->proc_fops == NULL)
			dp->proc_fops = &proc_file_operations;
	}
	return 0;
}

/*
 * Kill an inode that got unregistered..
 */
static void proc_kill_inodes(struct proc_dir_entry *de)
{
	struct list_head *p;
	struct super_block *sb = proc_mnt->mnt_sb;

	/*
	 * Actually it's a partial revoke().
	 */
	file_list_lock();
	for (p = sb->s_files.next; p != &sb->s_files; p = p->next) {
		struct file * filp = list_entry(p, struct file, f_list);
		struct dentry * dentry = filp->f_dentry;
		struct inode * inode;
		struct file_operations *fops;

		if (dentry->d_op != &proc_dentry_operations)
			continue;
		inode = dentry->d_inode;
		if (inode->u.generic_ip != de)
			continue;
		fops = filp->f_op;
		filp->f_op = NULL;
		fops_put(fops);
	}
	file_list_unlock();
}

static struct proc_dir_entry *proc_create(struct proc_dir_entry **parent,
					  const char *name,
					  mode_t mode,
					  nlink_t nlink)
{
	struct proc_dir_entry *ent = NULL;
	const char *fn = name;
	int len;

	/* make sure name is valid */
	if (!name || !strlen(name)) goto out;

	if (!(*parent) && xlate_proc_name(name, parent, &fn) != 0)
		goto out;
	len = strlen(fn);

	ent = kmalloc(sizeof(struct proc_dir_entry) + len + 1, GFP_KERNEL);
	if (!ent) goto out;

	memset(ent, 0, sizeof(struct proc_dir_entry));
	memcpy(((char *) ent) + sizeof(struct proc_dir_entry), fn, len + 1);
	ent->name = ((char *) ent) + sizeof(*ent);
	ent->namelen = len;
	ent->mode = mode;
	ent->nlink = nlink;
 out:
	return ent;
}

struct proc_dir_entry *proc_symlink(const char *name,
		struct proc_dir_entry *parent, const char *dest)
{
	struct proc_dir_entry *ent;

	ent = proc_create(&parent,name,
			  (S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO),1);

	if (ent) {
		ent->data = kmalloc((ent->size=strlen(dest))+1, GFP_KERNEL);
		if (ent->data) {
			strcpy((char*)ent->data,dest);
			if (proc_register(parent, ent) < 0) {
				kfree(ent->data);
				kfree(ent);
				ent = NULL;
			}
		} else {
			kfree(ent);
			ent = NULL;
		}
	}
	return ent;
}

struct proc_dir_entry *proc_mknod(const char *name, mode_t mode,
		struct proc_dir_entry *parent, kdev_t rdev)
{
	struct proc_dir_entry *ent;

	ent = proc_create(&parent,name,mode,1);
	if (ent) {
		ent->rdev = rdev;
		if (proc_register(parent, ent) < 0) {
			kfree(ent);
			ent = NULL;
		}
	}
	return ent;
}

struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *parent)
{
	struct proc_dir_entry *ent;

	ent = proc_create(&parent,name,
			  (S_IFDIR | S_IRUGO | S_IXUGO),2);
	if (ent) {
		ent->proc_fops = &proc_dir_operations;
		ent->proc_iops = &proc_dir_inode_operations;

		if (proc_register(parent, ent) < 0) {
			kfree(ent);
			ent = NULL;
		}
	}
	return ent;
}

struct proc_dir_entry *create_proc_entry(const char *name, mode_t mode,
					 struct proc_dir_entry *parent)
{
	struct proc_dir_entry *ent;
	nlink_t nlink;

	if (S_ISDIR(mode)) {
		if ((mode & S_IALLUGO) == 0)
			mode |= S_IRUGO | S_IXUGO;
		nlink = 2;
	} else {
		if ((mode & S_IFMT) == 0)
			mode |= S_IFREG;
		if ((mode & S_IALLUGO) == 0)
			mode |= S_IRUGO;
		nlink = 1;
	}

	ent = proc_create(&parent,name,mode,nlink);
	if (ent) {
		if (S_ISDIR(mode)) {
			ent->proc_fops = &proc_dir_operations;
			ent->proc_iops = &proc_dir_inode_operations;
		}
		if (proc_register(parent, ent) < 0) {
			kfree(ent);
			ent = NULL;
		}
	}
	return ent;
}

void free_proc_entry(struct proc_dir_entry *de)
{
	int ino = de->low_ino;

	if (ino < PROC_DYNAMIC_FIRST ||
	    ino >= PROC_DYNAMIC_FIRST+PROC_NDYNAMIC)
		return;
	if (S_ISLNK(de->mode) && de->data)
		kfree(de->data);
	kfree(de);
}

/*
 * Remove a /proc entry and free it if it's not currently in use.
 * If it is in use, we set the 'deleted' flag.
 */
void remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
	struct proc_dir_entry **p;
	struct proc_dir_entry *de;
	const char *fn = name;
	int len;

	if (!parent && xlate_proc_name(name, &parent, &fn) != 0)
		goto out;
	len = strlen(fn);
	for (p = &parent->subdir; *p; p=&(*p)->next ) {
		if (!proc_match(len, fn, *p))
			continue;
		de = *p;
		*p = de->next;
		de->next = NULL;
		if (S_ISDIR(de->mode))
			parent->nlink--;
		clear_bit(de->low_ino - PROC_DYNAMIC_FIRST,
			  proc_alloc_map);
		proc_kill_inodes(de);
		de->nlink = 0;
		if (!atomic_read(&de->count))
			free_proc_entry(de);
		else {
			de->deleted = 1;
			printk("remove_proc_entry: %s/%s busy, count=%d\n",
				parent->name, de->name, atomic_read(&de->count));
		}
		break;
	}
out:
	return;
}
