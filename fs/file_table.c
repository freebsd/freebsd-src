/*
 *  linux/fs/file_table.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/smp_lock.h>
#include <linux/iobuf.h>

/* sysctl tunables... */
struct files_stat_struct files_stat = {0, 0, NR_FILE};

/* Here the new files go */
static LIST_HEAD(anon_list);
/* And here the free ones sit */
static LIST_HEAD(free_list);
/* public *and* exported. Not pretty! */
spinlock_t files_lock = SPIN_LOCK_UNLOCKED;

/* Find an unused file structure and return a pointer to it.
 * Returns NULL, if there are no more free file structures or
 * we run out of memory.
 *
 * SMP-safe.
 */
struct file * get_empty_filp(void)
{
	static int old_max = 0;
	struct file * f;

	file_list_lock();
	if (files_stat.nr_free_files > NR_RESERVED_FILES) {
	used_one:
		f = list_entry(free_list.next, struct file, f_list);
		list_del(&f->f_list);
		files_stat.nr_free_files--;
	new_one:
		memset(f, 0, sizeof(*f));
		atomic_set(&f->f_count,1);
		f->f_version = ++event;
		f->f_uid = current->fsuid;
		f->f_gid = current->fsgid;
		list_add(&f->f_list, &anon_list);
		file_list_unlock();
		return f;
	}
	/*
	 * Use a reserved one if we're the superuser
	 */
	if (files_stat.nr_free_files && !current->euid)
		goto used_one;
	/*
	 * Allocate a new one if we're below the limit.
	 */
	if (files_stat.nr_files < files_stat.max_files) {
		file_list_unlock();
		f = kmem_cache_alloc(filp_cachep, SLAB_KERNEL);
		file_list_lock();
		if (f) {
			files_stat.nr_files++;
			goto new_one;
		}
		/* Big problems... */
		printk(KERN_WARNING "VFS: filp allocation failed\n");

	} else if (files_stat.max_files > old_max) {
		printk(KERN_INFO "VFS: file-max limit %d reached\n", files_stat.max_files);
		old_max = files_stat.max_files;
	}
	file_list_unlock();
	return NULL;
}

/*
 * Clear and initialize a (private) struct file for the given dentry,
 * and call the open function (if any).  The caller must verify that
 * inode->i_fop is not NULL.
 */
int init_private_file(struct file *filp, struct dentry *dentry, int mode)
{
	memset(filp, 0, sizeof(*filp));
	filp->f_mode   = mode;
	atomic_set(&filp->f_count, 1);
	filp->f_dentry = dentry;
	filp->f_uid    = current->fsuid;
	filp->f_gid    = current->fsgid;
	filp->f_op     = dentry->d_inode->i_fop;
	if (filp->f_op->open)
		return filp->f_op->open(dentry->d_inode, filp);
	else
		return 0;
}

void fput(struct file * file)
{
	struct dentry * dentry = file->f_dentry;
	struct vfsmount * mnt = file->f_vfsmnt;
	struct inode * inode = dentry->d_inode;

	if (atomic_dec_and_test(&file->f_count)) {
		locks_remove_flock(file);

		if (file->f_iobuf)
			free_kiovec(1, &file->f_iobuf);

		if (file->f_op && file->f_op->release)
			file->f_op->release(inode, file);
		fops_put(file->f_op);
		if (file->f_mode & FMODE_WRITE)
			put_write_access(inode);
		file_list_lock();
		file->f_dentry = NULL;
		file->f_vfsmnt = NULL;
		list_del(&file->f_list);
		list_add(&file->f_list, &free_list);
		files_stat.nr_free_files++;
		file_list_unlock();
		dput(dentry);
		mntput(mnt);
	}
}

struct file * fget(unsigned int fd)
{
	struct file * file;
	struct files_struct *files = current->files;

	read_lock(&files->file_lock);
	file = fcheck(fd);
	if (file)
		get_file(file);
	read_unlock(&files->file_lock);
	return file;
}

/* Here. put_filp() is SMP-safe now. */

void put_filp(struct file *file)
{
	if(atomic_dec_and_test(&file->f_count)) {
		file_list_lock();
		list_del(&file->f_list);
		list_add(&file->f_list, &free_list);
		files_stat.nr_free_files++;
		file_list_unlock();
	}
}

void file_move(struct file *file, struct list_head *list)
{
	if (!list)
		return;
	file_list_lock();
	list_del(&file->f_list);
	list_add(&file->f_list, list);
	file_list_unlock();
}

int fs_may_remount_ro(struct super_block *sb)
{
	struct list_head *p;

	/* Check that no files are currently opened for writing. */
	file_list_lock();
	for (p = sb->s_files.next; p != &sb->s_files; p = p->next) {
		struct file *file = list_entry(p, struct file, f_list);
		struct inode *inode = file->f_dentry->d_inode;

		/* File with pending delete? */
		if (inode->i_nlink == 0)
			goto too_bad;

		/* Writable file? */
		if (S_ISREG(inode->i_mode) && (file->f_mode & FMODE_WRITE))
			goto too_bad;
	}
	file_list_unlock();
	return 1; /* Tis' cool bro. */
too_bad:
	file_list_unlock();
	return 0;
}

void __init files_init(unsigned long mempages)
{ 
	int n; 
	/* One file with associated inode and dcache is very roughly 1K. 
	 * Per default don't use more than 10% of our memory for files. 
	 */ 

	n = (mempages * (PAGE_SIZE / 1024)) / 10;
	files_stat.max_files = n; 
	if (files_stat.max_files < NR_FILE)
		files_stat.max_files = NR_FILE;
} 

