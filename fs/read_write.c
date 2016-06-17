/*
 *  linux/fs/read_write.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Minor pieces Copyright (C) 2002 Red Hat Inc, All Rights Reserved
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/slab.h> 
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/smp_lock.h>
#include <linux/dnotify.h>

#include <asm/uaccess.h>

struct file_operations generic_ro_fops = {
	llseek:		generic_file_llseek,
	read:		generic_file_read,
	mmap:		generic_file_mmap,
};

ssize_t generic_read_dir(struct file *filp, char *buf, size_t siz, loff_t *ppos)
{
	return -EISDIR;
}

loff_t generic_file_llseek(struct file *file, loff_t offset, int origin)
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
	if (offset>=0 && offset<=file->f_dentry->d_inode->i_sb->s_maxbytes) {
		if (offset != file->f_pos) {
			file->f_pos = offset;
			file->f_reada = 0;
			file->f_version = ++event;
		}
		retval = offset;
	}
	return retval;
}

loff_t no_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

loff_t default_llseek(struct file *file, loff_t offset, int origin)
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
	if (offset >= 0) {
		if (offset != file->f_pos) {
			file->f_pos = offset;
			file->f_reada = 0;
			file->f_version = ++event;
		}
		retval = offset;
	}
	return retval;
}

static inline loff_t llseek(struct file *file, loff_t offset, int origin)
{
	loff_t (*fn)(struct file *, loff_t, int);
	loff_t retval;

	fn = default_llseek;
	if (file->f_op && file->f_op->llseek)
		fn = file->f_op->llseek;
	lock_kernel();
	retval = fn(file, offset, origin);
	unlock_kernel();
	return retval;
}

asmlinkage off_t sys_lseek(unsigned int fd, off_t offset, unsigned int origin)
{
	off_t retval;
	struct file * file;

	retval = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad;
	retval = -EINVAL;
	if (origin <= 2) {
		loff_t res = llseek(file, offset, origin);
		retval = res;
		if (res != (loff_t)retval)
			retval = -EOVERFLOW;	/* LFS: should only happen on 32 bit platforms */
	}
	fput(file);
bad:
	return retval;
}

#if !defined(__alpha__)
asmlinkage long sys_llseek(unsigned int fd, unsigned long offset_high,
			   unsigned long offset_low, loff_t * result,
			   unsigned int origin)
{
	int retval;
	struct file * file;
	loff_t offset;

	retval = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad;
	retval = -EINVAL;
	if (origin > 2)
		goto out_putf;

	offset = llseek(file, ((loff_t) offset_high << 32) | offset_low,
			origin);

	retval = (int)offset;
	if (offset >= 0) {
		retval = -EFAULT;
		if (!copy_to_user(result, &offset, sizeof(offset)))
			retval = 0;
	}
out_putf:
	fput(file);
bad:
	return retval;
}
#endif

asmlinkage ssize_t sys_read(unsigned int fd, char * buf, size_t count)
{
	ssize_t ret;
	struct file * file;

	ret = -EBADF;
	file = fget(fd);
	if (file) {
		if (file->f_mode & FMODE_READ) {
			ret = locks_verify_area(FLOCK_VERIFY_READ, file->f_dentry->d_inode,
						file, file->f_pos, count);
			if (!ret) {
				ssize_t (*read)(struct file *, char *, size_t, loff_t *);
				ret = -EINVAL;
				if (file->f_op && (read = file->f_op->read) != NULL)
					ret = read(file, buf, count, &file->f_pos);
			}
		}
		if (ret > 0)
			dnotify_parent(file->f_dentry, DN_ACCESS);
		fput(file);
	}
	return ret;
}

asmlinkage ssize_t sys_write(unsigned int fd, const char * buf, size_t count)
{
	ssize_t ret;
	struct file * file;

	ret = -EBADF;
	file = fget(fd);
	if (file) {
		if (file->f_mode & FMODE_WRITE) {
			struct inode *inode = file->f_dentry->d_inode;
			ret = locks_verify_area(FLOCK_VERIFY_WRITE, inode, file,
				file->f_pos, count);
			if (!ret) {
				ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
				ret = -EINVAL;
				if (file->f_op && (write = file->f_op->write) != NULL)
					ret = write(file, buf, count, &file->f_pos);
			}
		}
		if (ret > 0)
			dnotify_parent(file->f_dentry, DN_MODIFY);
		fput(file);
	}
	return ret;
}


static ssize_t do_readv_writev(int type, struct file *file,
			       const struct iovec * vector,
			       unsigned long count)
{
	typedef ssize_t (*io_fn_t)(struct file *, char *, size_t, loff_t *);
	typedef ssize_t (*iov_fn_t)(struct file *, const struct iovec *, unsigned long, loff_t *);

	size_t tot_len;
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov=iovstack;
	ssize_t ret, i;
	io_fn_t fn;
	iov_fn_t fnv;
	struct inode *inode;

	/*
	 * First get the "struct iovec" from user memory and
	 * verify all the pointers
	 */
	ret = 0;
	if (!count)
		goto out_nofree;
	ret = -EINVAL;
	if (count > UIO_MAXIOV)
		goto out_nofree;
	if (!file->f_op)
		goto out_nofree;
	if (count > UIO_FASTIOV) {
		ret = -ENOMEM;
		iov = kmalloc(count*sizeof(struct iovec), GFP_KERNEL);
		if (!iov)
			goto out_nofree;
	}
	ret = -EFAULT;
	if (copy_from_user(iov, vector, count*sizeof(*vector)))
		goto out;

	/*
	 * Single unix specification:
	 * We should -EINVAL if an element length is not >= 0 and fitting an ssize_t
	 * The total length is fitting an ssize_t
	 *
	 * Be careful here because iov_len is a size_t not an ssize_t
	 */
	 
	tot_len = 0;
	ret = -EINVAL;
	for (i = 0 ; i < count ; i++) {
		ssize_t len = (ssize_t) iov[i].iov_len;
		if (len < 0)	/* size_t not fitting an ssize_t .. */
			goto out;
		tot_len += len;
		/* We must do this work unsigned - signed overflow is
		   undefined and gcc 3.2 now uses that fact sometimes... 
		   
		   FIXME: put in a proper limits.h for each platform */
#if BITS_PER_LONG==64
		if (tot_len > 0x7FFFFFFFFFFFFFFFUL)
#else
		if (tot_len > 0x7FFFFFFFUL)
#endif		
			goto out;
	}

	inode = file->f_dentry->d_inode;
	/* VERIFY_WRITE actually means a read, as we write to user space */
	ret = locks_verify_area((type == VERIFY_WRITE
				 ? FLOCK_VERIFY_READ : FLOCK_VERIFY_WRITE),
				inode, file, file->f_pos, tot_len);
	if (ret) goto out;

	fnv = (type == VERIFY_WRITE ? file->f_op->readv : file->f_op->writev);
	if (fnv) {
		ret = fnv(file, iov, count, &file->f_pos);
		goto out;
	}

	/* VERIFY_WRITE actually means a read, as we write to user space */
	fn = (type == VERIFY_WRITE ? file->f_op->read :
	      (io_fn_t) file->f_op->write);

	ret = 0;
	vector = iov;
	while (count > 0) {
		void * base;
		size_t len;
		ssize_t nr;

		base = vector->iov_base;
		len = vector->iov_len;
		vector++;
		count--;

		nr = fn(file, base, len, &file->f_pos);

		if (nr < 0) {
			if (!ret) ret = nr;
			break;
		}
		ret += nr;
		if (nr != len)
			break;
	}

out:
	if (iov != iovstack)
		kfree(iov);
out_nofree:
	/* VERIFY_WRITE actually means a read, as we write to user space */
	if ((ret + (type == VERIFY_WRITE)) > 0)
		dnotify_parent(file->f_dentry,
			(type == VERIFY_WRITE) ? DN_ACCESS : DN_MODIFY);
	return ret;
}

asmlinkage ssize_t sys_readv(unsigned long fd, const struct iovec * vector,
			     unsigned long count)
{
	struct file * file;
	ssize_t ret;


	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (file->f_op && (file->f_mode & FMODE_READ) &&
	    (file->f_op->readv || file->f_op->read))
		ret = do_readv_writev(VERIFY_WRITE, file, vector, count);
	fput(file);

bad_file:
	return ret;
}

asmlinkage ssize_t sys_writev(unsigned long fd, const struct iovec * vector,
			      unsigned long count)
{
	struct file * file;
	ssize_t ret;


	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (file->f_op && (file->f_mode & FMODE_WRITE) &&
	    (file->f_op->writev || file->f_op->write))
		ret = do_readv_writev(VERIFY_READ, file, vector, count);
	fput(file);

bad_file:
	return ret;
}

/* From the Single Unix Spec: pread & pwrite act like lseek to pos + op +
   lseek back to original location.  They fail just like lseek does on
   non-seekable files.  */

asmlinkage ssize_t sys_pread(unsigned int fd, char * buf,
			     size_t count, loff_t pos)
{
	ssize_t ret;
	struct file * file;
	ssize_t (*read)(struct file *, char *, size_t, loff_t *);

	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (!(file->f_mode & FMODE_READ))
		goto out;
	ret = locks_verify_area(FLOCK_VERIFY_READ, file->f_dentry->d_inode,
				file, pos, count);
	if (ret)
		goto out;
	ret = -EINVAL;
	if (!file->f_op || !(read = file->f_op->read))
		goto out;
	if (pos < 0)
		goto out;
	ret = read(file, buf, count, &pos);
	if (ret > 0)
		dnotify_parent(file->f_dentry, DN_ACCESS);
out:
	fput(file);
bad_file:
	return ret;
}

asmlinkage ssize_t sys_pwrite(unsigned int fd, const char * buf,
			      size_t count, loff_t pos)
{
	ssize_t ret;
	struct file * file;
	ssize_t (*write)(struct file *, const char *, size_t, loff_t *);

	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (!(file->f_mode & FMODE_WRITE))
		goto out;
	ret = locks_verify_area(FLOCK_VERIFY_WRITE, file->f_dentry->d_inode,
				file, pos, count);
	if (ret)
		goto out;
	ret = -EINVAL;
	if (!file->f_op || !(write = file->f_op->write))
		goto out;
	if (pos < 0)
		goto out;

	ret = write(file, buf, count, &pos);
	if (ret > 0)
		dnotify_parent(file->f_dentry, DN_MODIFY);
out:
	fput(file);
bad_file:
	return ret;
}
