/*
 *  linux/fs/readdir.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/file.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

int vfs_readdir(struct file *file, filldir_t filler, void *buf)
{
	struct inode *inode = file->f_dentry->d_inode;
	int res = -ENOTDIR;
	if (!file->f_op || !file->f_op->readdir)
		goto out;
	down(&inode->i_sem);
	down(&inode->i_zombie);
	res = -ENOENT;
	if (!IS_DEADDIR(inode)) {
		lock_kernel();
		res = file->f_op->readdir(file, buf, filler);
		unlock_kernel();
	}
	up(&inode->i_zombie);
	up(&inode->i_sem);
out:
	return res;
}

int dcache_dir_open(struct inode *inode, struct file *file)
{
	static struct qstr cursor_name = {len:1, name:"."};

	file->private_data = d_alloc(file->f_dentry, &cursor_name);

	return file->private_data ? 0 : -ENOMEM;
}

int dcache_dir_close(struct inode *inode, struct file *file)
{
	dput(file->private_data);
	return 0;
}

loff_t dcache_dir_lseek(struct file *file, loff_t offset, int origin)
{
	down(&file->f_dentry->d_inode->i_sem);
	switch (origin) {
		case 1:
			offset += file->f_pos;
		case 0:
			if (offset >= 0)
				break;
		default:
			up(&file->f_dentry->d_inode->i_sem);
			return -EINVAL;
	}
	if (offset != file->f_pos) {
		file->f_pos = offset;
		if (file->f_pos >= 2) {
			struct list_head *p;
			struct dentry *cursor = file->private_data;
			loff_t n = file->f_pos - 2;

			spin_lock(&dcache_lock);
			list_del(&cursor->d_child);
			p = file->f_dentry->d_subdirs.next;
			while (n && p != &file->f_dentry->d_subdirs) {
				struct dentry *next;
				next = list_entry(p, struct dentry, d_child);
				if (!list_empty(&next->d_hash) && next->d_inode)
					n--;
				p = p->next;
			}
			list_add_tail(&cursor->d_child, p);
			spin_unlock(&dcache_lock);
		}
	}
	up(&file->f_dentry->d_inode->i_sem);
	return offset;
}

int dcache_dir_fsync(struct file * file, struct dentry *dentry, int datasync)
{
	return 0;
}

/*
 * Directory is locked and all positive dentries in it are safe, since
 * for ramfs-type trees they can't go away without unlink() or rmdir(),
 * both impossible due to the lock on directory.
 */

int dcache_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_dentry;
	struct dentry *cursor = filp->private_data;
	struct list_head *p, *q = &cursor->d_child;
	ino_t ino;
	int i = filp->f_pos;

	switch (i) {
		case 0:
			ino = dentry->d_inode->i_ino;
			if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0)
				break;
			filp->f_pos++;
			i++;
			/* fallthrough */
		case 1:
			spin_lock(&dcache_lock);
			ino = dentry->d_parent->d_inode->i_ino;
			spin_unlock(&dcache_lock);
			if (filldir(dirent, "..", 2, i, ino, DT_DIR) < 0)
				break;
			filp->f_pos++;
			i++;
			/* fallthrough */
		default:
			spin_lock(&dcache_lock);
			if (filp->f_pos == 2) {
				list_del(q);
				list_add(q, &dentry->d_subdirs);
			}
			for (p=q->next; p != &dentry->d_subdirs; p=p->next) {
				struct dentry *next;
				next = list_entry(p, struct dentry, d_child);
				if (list_empty(&next->d_hash) || !next->d_inode)
					continue;

				spin_unlock(&dcache_lock);
				if (filldir(dirent, next->d_name.name, next->d_name.len, filp->f_pos, next->d_inode->i_ino, DT_UNKNOWN) < 0)
					return 0;
				spin_lock(&dcache_lock);
				/* next is still alive */
				list_del(q);
				list_add(q, p);
				p = q;
				filp->f_pos++;
			}
			spin_unlock(&dcache_lock);
	}
	UPDATE_ATIME(dentry->d_inode);
	return 0;
}

struct file_operations dcache_dir_ops = {
	open:		dcache_dir_open,
	release:	dcache_dir_close,
	llseek:		dcache_dir_lseek,
	read:		generic_read_dir,
	readdir:	dcache_readdir,
	fsync:		dcache_dir_fsync,
};

/*
 * Traditional linux readdir() handling..
 *
 * "count=1" is a special case, meaning that the buffer is one
 * dirent-structure in size and that the code can't handle more
 * anyway. Thus the special "fillonedir()" function for that
 * case (the low-level handlers don't need to care about this).
 */
#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP(x) (((x)+sizeof(long)-1) & ~(sizeof(long)-1))

#ifndef __ia64__

struct old_linux_dirent {
	unsigned long	d_ino;
	unsigned long	d_offset;
	unsigned short	d_namlen;
	char		d_name[1];
};

struct readdir_callback {
	struct old_linux_dirent * dirent;
	int count;
};

static int fillonedir(void * __buf, const char * name, int namlen, loff_t offset,
		      ino_t ino, unsigned int d_type)
{
	struct readdir_callback * buf = (struct readdir_callback *) __buf;
	struct old_linux_dirent * dirent;

	if (buf->count)
		return -EINVAL;
	buf->count++;
	dirent = buf->dirent;
	put_user(ino, &dirent->d_ino);
	put_user(offset, &dirent->d_offset);
	put_user(namlen, &dirent->d_namlen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	return 0;
}

asmlinkage int old_readdir(unsigned int fd, void * dirent, unsigned int count)
{
	int error;
	struct file * file;
	struct readdir_callback buf;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.count = 0;
	buf.dirent = dirent;

	error = vfs_readdir(file, fillonedir, &buf);
	if (error >= 0)
		error = buf.count;

	fput(file);
out:
	return error;
}

#endif /* !__ia64__ */

/*
 * New, all-improved, singing, dancing, iBCS2-compliant getdents()
 * interface. 
 */
struct linux_dirent {
	unsigned long	d_ino;
	unsigned long	d_off;
	unsigned short	d_reclen;
	char		d_name[1];
};

struct getdents_callback {
	struct linux_dirent * current_dir;
	struct linux_dirent * previous;
	int count;
	int error;
};

static int filldir(void * __buf, const char * name, int namlen, loff_t offset,
		   ino_t ino, unsigned int d_type)
{
	struct linux_dirent * dirent;
	struct getdents_callback * buf = (struct getdents_callback *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent)
		put_user(offset, &dirent->d_off);
	dirent = buf->current_dir;
	buf->previous = dirent;
	put_user(ino, &dirent->d_ino);
	put_user(reclen, &dirent->d_reclen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	((char *) dirent) += reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
}

asmlinkage long sys_getdents(unsigned int fd, void * dirent, unsigned int count)
{
	struct file * file;
	struct linux_dirent * lastdirent;
	struct getdents_callback buf;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = (struct linux_dirent *) dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, filldir, &buf);
	if (error < 0)
		goto out_putf;
	error = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = count - buf.count;
	}

out_putf:
	fput(file);
out:
	return error;
}

/*
 * And even better one including d_type field and 64bit d_ino and d_off.
 */
struct linux_dirent64 {
	u64		d_ino;
	s64		d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[0];
};

#define ROUND_UP64(x) (((x)+sizeof(u64)-1) & ~(sizeof(u64)-1))

struct getdents_callback64 {
	struct linux_dirent64 * current_dir;
	struct linux_dirent64 * previous;
	int count;
	int error;
};

static int filldir64(void * __buf, const char * name, int namlen, loff_t offset,
		     ino_t ino, unsigned int d_type)
{
	struct linux_dirent64 * dirent, d;
	struct getdents_callback64 * buf = (struct getdents_callback64 *) __buf;
	int reclen = ROUND_UP64(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent) {
		d.d_off = offset;
		copy_to_user(&dirent->d_off, &d.d_off, sizeof(d.d_off));
	}
	dirent = buf->current_dir;
	buf->previous = dirent;
	memset(&d, 0, NAME_OFFSET(&d));
	d.d_ino = ino;
	d.d_reclen = reclen;
	d.d_type = d_type;
	copy_to_user(dirent, &d, NAME_OFFSET(&d));
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	((char *) dirent) += reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
}

asmlinkage long sys_getdents64(unsigned int fd, void * dirent, unsigned int count)
{
	struct file * file;
	struct linux_dirent64 * lastdirent;
	struct getdents_callback64 buf;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = (struct linux_dirent64 *) dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, filldir64, &buf);
	if (error < 0)
		goto out_putf;
	error = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		struct linux_dirent64 d;
		d.d_off = file->f_pos;
		copy_to_user(&lastdirent->d_off, &d.d_off, sizeof(d.d_off));
		error = count - buf.count;
	}

out_putf:
	fput(file);
out:
	return error;
}
