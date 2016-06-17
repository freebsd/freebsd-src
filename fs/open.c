/*
 *  linux/fs/open.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/string.h>
#include <linux/mm.h>
#include <linux/utime.h>
#include <linux/file.h>
#include <linux/smp_lock.h>
#include <linux/quotaops.h>
#include <linux/dnotify.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/iobuf.h>

#include <asm/uaccess.h>

#define special_file(m) (S_ISCHR(m)||S_ISBLK(m)||S_ISFIFO(m)||S_ISSOCK(m))

int vfs_statfs(struct super_block *sb, struct statfs *buf)
{
	int retval = -ENODEV;

	if (sb) {
		retval = -ENOSYS;
		if (sb->s_op && sb->s_op->statfs) {
			memset(buf, 0, sizeof(struct statfs));
			lock_kernel();
			retval = sb->s_op->statfs(sb, buf);
			unlock_kernel();
		}
	}
	return retval;
}


asmlinkage long sys_statfs(const char * path, struct statfs * buf)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(path, &nd);
	if (!error) {
		struct statfs tmp;
		error = vfs_statfs(nd.dentry->d_inode->i_sb, &tmp);
		if (!error && copy_to_user(buf, &tmp, sizeof(struct statfs)))
			error = -EFAULT;
		path_release(&nd);
	}
	return error;
}

asmlinkage long sys_fstatfs(unsigned int fd, struct statfs * buf)
{
	struct file * file;
	struct statfs tmp;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;
	error = vfs_statfs(file->f_dentry->d_inode->i_sb, &tmp);
	if (!error && copy_to_user(buf, &tmp, sizeof(struct statfs)))
		error = -EFAULT;
	fput(file);
out:
	return error;
}

/*
 * Install a file pointer in the fd array.  
 *
 * The VFS is full of places where we drop the files lock between
 * setting the open_fds bitmap and installing the file in the file
 * array.  At any such point, we are vulnerable to a dup2() race
 * installing a file in the array before us.  We need to detect this and
 * fput() the struct file we are about to overwrite in this case.
 *
 * It should never happen - if we allow dup2() do it, _really_ bad things
 * will follow.
 */

void fd_install(unsigned int fd, struct file * file)
{
	struct files_struct *files = current->files;
	
	write_lock(&files->file_lock);
	if (files->fd[fd])
		BUG();
	files->fd[fd] = file;
	write_unlock(&files->file_lock);
}

int do_truncate(struct dentry *dentry, loff_t length)
{
	struct inode *inode = dentry->d_inode;
	int error;
	struct iattr newattrs;

	/* Not pretty: "inode->i_size" shouldn't really be signed. But it is. */
	if (length < 0)
		return -EINVAL;

	down_write(&inode->i_alloc_sem);
	down(&inode->i_sem);
	newattrs.ia_size = length;
	newattrs.ia_valid = ATTR_SIZE | ATTR_CTIME;
	error = notify_change(dentry, &newattrs);
	up(&inode->i_sem);
	up_write(&inode->i_alloc_sem);
	return error;
}

static inline long do_sys_truncate(const char * path, loff_t length)
{
	struct nameidata nd;
	struct inode * inode;
	int error;

	error = -EINVAL;
	if (length < 0)	/* sorry, but loff_t says... */
		goto out;

	error = user_path_walk(path, &nd);
	if (error)
		goto out;
	inode = nd.dentry->d_inode;

	/* For directories it's -EISDIR, for other non-regulars - -EINVAL */
	error = -EISDIR;
	if (S_ISDIR(inode->i_mode))
		goto dput_and_out;

	error = -EINVAL;
	if (!S_ISREG(inode->i_mode))
		goto dput_and_out;

	error = permission(inode,MAY_WRITE);
	if (error)
		goto dput_and_out;

	error = -EROFS;
	if (IS_RDONLY(inode))
		goto dput_and_out;

	error = -EPERM;
	if (IS_IMMUTABLE(inode) || IS_APPEND(inode))
		goto dput_and_out;

	/*
	 * Make sure that there are no leases.
	 */
	error = get_lease(inode, FMODE_WRITE);
	if (error)
		goto dput_and_out;

	error = get_write_access(inode);
	if (error)
		goto dput_and_out;

	error = locks_verify_truncate(inode, NULL, length);
	if (!error) {
		DQUOT_INIT(inode);
		error = do_truncate(nd.dentry, length);
	}
	put_write_access(inode);

dput_and_out:
	path_release(&nd);
out:
	return error;
}

asmlinkage long sys_truncate(const char * path, unsigned long length)
{
	/* on 32-bit boxen it will cut the range 2^31--2^32-1 off */
	return do_sys_truncate(path, (long)length);
}

static inline long do_sys_ftruncate(unsigned int fd, loff_t length, int small)
{
	struct inode * inode;
	struct dentry *dentry;
	struct file * file;
	int error;

	error = -EINVAL;
	if (length < 0)
		goto out;
	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	/* explicitly opened as large or we are on 64-bit box */
	if (file->f_flags & O_LARGEFILE)
		small = 0;

	dentry = file->f_dentry;
	inode = dentry->d_inode;
	error = -EINVAL;
	if (!S_ISREG(inode->i_mode) || !(file->f_mode & FMODE_WRITE))
		goto out_putf;

	error = -EINVAL;
	/* Cannot ftruncate over 2^31 bytes without large file support */
	if (small && length > MAX_NON_LFS)
		goto out_putf;

	error = -EPERM;
	if (IS_APPEND(inode))
		goto out_putf;

	error = locks_verify_truncate(inode, file, length);
	if (!error)
		error = do_truncate(dentry, length);
out_putf:
	fput(file);
out:
	return error;
}

asmlinkage long sys_ftruncate(unsigned int fd, unsigned long length)
{
	return do_sys_ftruncate(fd, length, 1);
}

/* LFS versions of truncate are only needed on 32 bit machines */
#if BITS_PER_LONG == 32
asmlinkage long sys_truncate64(const char * path, loff_t length)
{
	return do_sys_truncate(path, length);
}

asmlinkage long sys_ftruncate64(unsigned int fd, loff_t length)
{
	return do_sys_ftruncate(fd, length, 0);
}
#endif

#if !(defined(__alpha__) || defined(__ia64__))

/*
 * sys_utime() can be implemented in user-level using sys_utimes().
 * Is this for backwards compatibility?  If so, why not move it
 * into the appropriate arch directory (for those architectures that
 * need it).
 */

/* If times==NULL, set access and modification to current time,
 * must be owner or have write permission.
 * Else, update from *times, must be owner or super user.
 */
asmlinkage long sys_utime(char * filename, struct utimbuf * times)
{
	int error;
	struct nameidata nd;
	struct inode * inode;
	struct iattr newattrs;

	error = user_path_walk(filename, &nd);
	if (error)
		goto out;
	inode = nd.dentry->d_inode;

	error = -EROFS;
	if (IS_RDONLY(inode))
		goto dput_and_out;

	/* Don't worry, the checks are done in inode_change_ok() */
	newattrs.ia_valid = ATTR_CTIME | ATTR_MTIME | ATTR_ATIME;
	if (times) {
		error = -EPERM;
		if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
			goto dput_and_out;
		error = get_user(newattrs.ia_atime, &times->actime);
		if (!error) 
			error = get_user(newattrs.ia_mtime, &times->modtime);
		if (error)
			goto dput_and_out;

		newattrs.ia_valid |= ATTR_ATIME_SET | ATTR_MTIME_SET;
	} else {
		error = -EACCES;
		if (IS_IMMUTABLE(inode))
			goto dput_and_out;
		if (current->fsuid != inode->i_uid &&
		    (error = permission(inode,MAY_WRITE)) != 0)
			goto dput_and_out;
	}
	error = notify_change(nd.dentry, &newattrs);
dput_and_out:
	path_release(&nd);
out:
	return error;
}

#endif

/* If times==NULL, set access and modification to current time,
 * must be owner or have write permission.
 * Else, update from *times, must be owner or super user.
 */
asmlinkage long sys_utimes(char * filename, struct timeval * utimes)
{
	int error;
	struct nameidata nd;
	struct inode * inode;
	struct iattr newattrs;

	error = user_path_walk(filename, &nd);

	if (error)
		goto out;
	inode = nd.dentry->d_inode;

	error = -EROFS;
	if (IS_RDONLY(inode))
		goto dput_and_out;

	/* Don't worry, the checks are done in inode_change_ok() */
	newattrs.ia_valid = ATTR_CTIME | ATTR_MTIME | ATTR_ATIME;
	if (utimes) {
		struct timeval times[2];
		error = -EPERM;
		if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
			goto dput_and_out;
		error = -EFAULT;
		if (copy_from_user(&times, utimes, sizeof(times)))
			goto dput_and_out;
		newattrs.ia_atime = times[0].tv_sec;
		newattrs.ia_mtime = times[1].tv_sec;
		newattrs.ia_valid |= ATTR_ATIME_SET | ATTR_MTIME_SET;
	} else {
		error = -EACCES;
		if (IS_IMMUTABLE(inode))
			goto dput_and_out;

		if (current->fsuid != inode->i_uid &&
		    (error = permission(inode,MAY_WRITE)) != 0)
			goto dput_and_out;
	}
	error = notify_change(nd.dentry, &newattrs);
dput_and_out:
	path_release(&nd);
out:
	return error;
}

/*
 * access() needs to use the real uid/gid, not the effective uid/gid.
 * We do this by temporarily clearing all FS-related capabilities and
 * switching the fsuid/fsgid around to the real ones.
 */
asmlinkage long sys_access(const char * filename, int mode)
{
	struct nameidata nd;
	int old_fsuid, old_fsgid;
	kernel_cap_t old_cap;
	int res;

	if (mode & ~S_IRWXO)	/* where's F_OK, X_OK, W_OK, R_OK? */
		return -EINVAL;

	old_fsuid = current->fsuid;
	old_fsgid = current->fsgid;
	old_cap = current->cap_effective;

	current->fsuid = current->uid;
	current->fsgid = current->gid;

	/* Clear the capabilities if we switch to a non-root user */
	if (current->uid)
		cap_clear(current->cap_effective);
	else
		current->cap_effective = current->cap_permitted;

	res = user_path_walk(filename, &nd);
	if (!res) {
		res = permission(nd.dentry->d_inode, mode);
		/* SuS v2 requires we report a read only fs too */
		if(!res && (mode & S_IWOTH) && IS_RDONLY(nd.dentry->d_inode)
		   && !special_file(nd.dentry->d_inode->i_mode))
			res = -EROFS;
		path_release(&nd);
	}

	current->fsuid = old_fsuid;
	current->fsgid = old_fsgid;
	current->cap_effective = old_cap;

	return res;
}

asmlinkage long sys_chdir(const char * filename)
{
	int error;
	struct nameidata nd;

	error = __user_walk(filename,LOOKUP_POSITIVE|LOOKUP_FOLLOW|LOOKUP_DIRECTORY,&nd);
	if (error)
		goto out;

	error = permission(nd.dentry->d_inode,MAY_EXEC);
	if (error)
		goto dput_and_out;

	set_fs_pwd(current->fs, nd.mnt, nd.dentry);

dput_and_out:
	path_release(&nd);
out:
	return error;
}

asmlinkage long sys_fchdir(unsigned int fd)
{
	struct file *file;
	struct dentry *dentry;
	struct inode *inode;
	struct vfsmount *mnt;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	dentry = file->f_dentry;
	mnt = file->f_vfsmnt;
	inode = dentry->d_inode;

	error = -ENOTDIR;
	if (!S_ISDIR(inode->i_mode))
		goto out_putf;

	error = permission(inode, MAY_EXEC);
	if (!error)
		set_fs_pwd(current->fs, mnt, dentry);
out_putf:
	fput(file);
out:
	return error;
}

asmlinkage long sys_chroot(const char * filename)
{
	int error;
	struct nameidata nd;

	error = __user_walk(filename, LOOKUP_POSITIVE | LOOKUP_FOLLOW |
		      LOOKUP_DIRECTORY | LOOKUP_NOALT, &nd);
	if (error)
		goto out;

	error = permission(nd.dentry->d_inode,MAY_EXEC);
	if (error)
		goto dput_and_out;

	error = -EPERM;
	if (!capable(CAP_SYS_CHROOT))
		goto dput_and_out;

	set_fs_root(current->fs, nd.mnt, nd.dentry);
	set_fs_altroot();
	error = 0;
dput_and_out:
	path_release(&nd);
out:
	return error;
}

asmlinkage long sys_fchmod(unsigned int fd, mode_t mode)
{
	struct inode * inode;
	struct dentry * dentry;
	struct file * file;
	int err = -EBADF;
	struct iattr newattrs;

	file = fget(fd);
	if (!file)
		goto out;

	dentry = file->f_dentry;
	inode = dentry->d_inode;

	err = -EROFS;
	if (IS_RDONLY(inode))
		goto out_putf;
	err = -EPERM;
	if (IS_IMMUTABLE(inode) || IS_APPEND(inode))
		goto out_putf;
	if (mode == (mode_t) -1)
		mode = inode->i_mode;
	newattrs.ia_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	newattrs.ia_valid = ATTR_MODE | ATTR_CTIME;
	err = notify_change(dentry, &newattrs);

out_putf:
	fput(file);
out:
	return err;
}

asmlinkage long sys_chmod(const char * filename, mode_t mode)
{
	struct nameidata nd;
	struct inode * inode;
	int error;
	struct iattr newattrs;

	error = user_path_walk(filename, &nd);
	if (error)
		goto out;
	inode = nd.dentry->d_inode;

	error = -EROFS;
	if (IS_RDONLY(inode))
		goto dput_and_out;

	error = -EPERM;
	if (IS_IMMUTABLE(inode) || IS_APPEND(inode))
		goto dput_and_out;

	if (mode == (mode_t) -1)
		mode = inode->i_mode;
	newattrs.ia_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	newattrs.ia_valid = ATTR_MODE | ATTR_CTIME;
	error = notify_change(nd.dentry, &newattrs);

dput_and_out:
	path_release(&nd);
out:
	return error;
}

static int chown_common(struct dentry * dentry, uid_t user, gid_t group)
{
	struct inode * inode;
	int error;
	struct iattr newattrs;

	error = -ENOENT;
	if (!(inode = dentry->d_inode)) {
		printk(KERN_ERR "chown_common: NULL inode\n");
		goto out;
	}
	error = -EROFS;
	if (IS_RDONLY(inode))
		goto out;
	error = -EPERM;
	if (IS_IMMUTABLE(inode) || IS_APPEND(inode))
		goto out;
	if (user == (uid_t) -1)
		user = inode->i_uid;
	if (group == (gid_t) -1)
		group = inode->i_gid;
	newattrs.ia_mode = inode->i_mode;
	newattrs.ia_uid = user;
	newattrs.ia_gid = group;
	newattrs.ia_valid =  ATTR_UID | ATTR_GID | ATTR_CTIME;
	/*
	 * If the user or group of a non-directory has been changed by a
	 * non-root user, remove the setuid bit.
	 * 19981026	David C Niemi <niemi@tux.org>
	 *
	 * Changed this to apply to all users, including root, to avoid
	 * some races. This is the behavior we had in 2.0. The check for
	 * non-root was definitely wrong for 2.2 anyway, as it should
	 * have been using CAP_FSETID rather than fsuid -- 19990830 SD.
	 */
	if ((inode->i_mode & S_ISUID) == S_ISUID &&
		!S_ISDIR(inode->i_mode))
	{
		newattrs.ia_mode &= ~S_ISUID;
		newattrs.ia_valid |= ATTR_MODE;
	}
	/*
	 * Likewise, if the user or group of a non-directory has been changed
	 * by a non-root user, remove the setgid bit UNLESS there is no group
	 * execute bit (this would be a file marked for mandatory locking).
	 * 19981026	David C Niemi <niemi@tux.org>
	 *
	 * Removed the fsuid check (see the comment above) -- 19990830 SD.
	 */
	if (((inode->i_mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP)) 
		&& !S_ISDIR(inode->i_mode))
	{
		newattrs.ia_mode &= ~S_ISGID;
		newattrs.ia_valid |= ATTR_MODE;
	}
	error = notify_change(dentry, &newattrs);
out:
	return error;
}

asmlinkage long sys_chown(const char * filename, uid_t user, gid_t group)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(filename, &nd);
	if (!error) {
		error = chown_common(nd.dentry, user, group);
		path_release(&nd);
	}
	return error;
}

asmlinkage long sys_lchown(const char * filename, uid_t user, gid_t group)
{
	struct nameidata nd;
	int error;

	error = user_path_walk_link(filename, &nd);
	if (!error) {
		error = chown_common(nd.dentry, user, group);
		path_release(&nd);
	}
	return error;
}


asmlinkage long sys_fchown(unsigned int fd, uid_t user, gid_t group)
{
	struct file * file;
	int error = -EBADF;

	file = fget(fd);
	if (file) {
		error = chown_common(file->f_dentry, user, group);
		fput(file);
	}
	return error;
}

/*
 * Note that while the flag value (low two bits) for sys_open means:
 *	00 - read-only
 *	01 - write-only
 *	10 - read-write
 *	11 - special
 * it is changed into
 *	00 - no permissions needed
 *	01 - read-permission
 *	10 - write-permission
 *	11 - read-write
 * for the internal routines (ie open_namei()/follow_link() etc). 00 is
 * used by symlinks.
 */
struct file *filp_open(const char * filename, int flags, int mode)
{
	int namei_flags, error;
	struct nameidata nd;

	namei_flags = flags;
	if ((namei_flags+1) & O_ACCMODE)
		namei_flags++;
	if (namei_flags & O_TRUNC)
		namei_flags |= 2;

	error = open_namei(filename, namei_flags, mode, &nd);
	if (!error)
		return dentry_open(nd.dentry, nd.mnt, flags);

	return ERR_PTR(error);
}

struct file *dentry_open(struct dentry *dentry, struct vfsmount *mnt, int flags)
{
	struct file * f;
	struct inode *inode;
	static LIST_HEAD(kill_list);
	int error;

	error = -ENFILE;
	f = get_empty_filp();
	if (!f)
		goto cleanup_dentry;
	f->f_flags = flags;
	f->f_mode = (flags+1) & O_ACCMODE;
	inode = dentry->d_inode;
	if (f->f_mode & FMODE_WRITE) {
		error = get_write_access(inode);
		if (error)
			goto cleanup_file;
	}

	f->f_dentry = dentry;
	f->f_vfsmnt = mnt;
	f->f_pos = 0;
	f->f_reada = 0;
	f->f_op = fops_get(inode->i_fop);
	file_move(f, &inode->i_sb->s_files);

	/* preallocate kiobuf for O_DIRECT */
	f->f_iobuf = NULL;
	f->f_iobuf_lock = 0;
	if (f->f_flags & O_DIRECT) {
		error = alloc_kiovec(1, &f->f_iobuf);
		if (error)
			goto cleanup_all;
	}

	if (f->f_op && f->f_op->open) {
		error = f->f_op->open(inode,f);
		if (error)
			goto cleanup_all;
	}
	f->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);

	return f;

cleanup_all:
	if (f->f_iobuf)
		free_kiovec(1, &f->f_iobuf);
	fops_put(f->f_op);
	if (f->f_mode & FMODE_WRITE)
		put_write_access(inode);
	file_move(f, &kill_list); /* out of the way.. */
	f->f_dentry = NULL;
	f->f_vfsmnt = NULL;
cleanup_file:
	put_filp(f);
cleanup_dentry:
	dput(dentry);
	mntput(mnt);
	return ERR_PTR(error);
}

/*
 * Find an empty file descriptor entry, and mark it busy.
 */
int get_unused_fd(void)
{
	struct files_struct * files = current->files;
	int fd, error;

  	error = -EMFILE;
	write_lock(&files->file_lock);

repeat:
 	fd = find_next_zero_bit(files->open_fds, 
				files->max_fdset, 
				files->next_fd);

	/*
	 * N.B. For clone tasks sharing a files structure, this test
	 * will limit the total number of files that can be opened.
	 */
	if (fd >= current->rlim[RLIMIT_NOFILE].rlim_cur)
		goto out;

	/* Do we need to expand the fdset array? */
	if (fd >= files->max_fdset) {
		error = expand_fdset(files, fd);
		if (!error) {
			error = -EMFILE;
			goto repeat;
		}
		goto out;
	}
	
	/* 
	 * Check whether we need to expand the fd array.
	 */
	if (fd >= files->max_fds) {
		error = expand_fd_array(files, fd);
		if (!error) {
			error = -EMFILE;
			goto repeat;
		}
		goto out;
	}

	FD_SET(fd, files->open_fds);
	FD_CLR(fd, files->close_on_exec);
	files->next_fd = fd + 1;
#if 1
	/* Sanity check */
	if (files->fd[fd] != NULL) {
		printk(KERN_WARNING "get_unused_fd: slot %d not NULL!\n", fd);
		files->fd[fd] = NULL;
	}
#endif
	error = fd;

out:
	write_unlock(&files->file_lock);
	return error;
}

asmlinkage long sys_open(const char * filename, int flags, int mode)
{
	char * tmp;
	int fd, error;

#if BITS_PER_LONG != 32
	flags |= O_LARGEFILE;
#endif
	tmp = getname(filename);
	fd = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {
		fd = get_unused_fd();
		if (fd >= 0) {
			struct file *f = filp_open(tmp, flags, mode);
			error = PTR_ERR(f);
			if (IS_ERR(f))
				goto out_error;
			fd_install(fd, f);
		}
out:
		putname(tmp);
	}
	return fd;

out_error:
	put_unused_fd(fd);
	fd = error;
	goto out;
}

#ifndef __alpha__

/*
 * For backward compatibility?  Maybe this should be moved
 * into arch/i386 instead?
 */
asmlinkage long sys_creat(const char * pathname, int mode)
{
	return sys_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

#endif

/*
 * "id" is the POSIX thread ID. We use the
 * files pointer for this..
 */
int filp_close(struct file *filp, fl_owner_t id)
{
	int retval;

	if (!file_count(filp)) {
		printk(KERN_ERR "VFS: Close: file count is 0\n");
		return 0;
	}
	retval = 0;
	if (filp->f_op && filp->f_op->flush) {
		lock_kernel();
		retval = filp->f_op->flush(filp);
		unlock_kernel();
	}
	dnotify_flush(filp, id);
	locks_remove_posix(filp, id);
	fput(filp);
	return retval;
}

/*
 * Careful here! We test whether the file pointer is NULL before
 * releasing the fd. This ensures that one clone task can't release
 * an fd while another clone is opening it.
 */
asmlinkage long sys_close(unsigned int fd)
{
	struct file * filp;
	struct files_struct *files = current->files;

	write_lock(&files->file_lock);
	if (fd >= files->max_fds)
		goto out_unlock;
	filp = files->fd[fd];
	if (!filp)
		goto out_unlock;
	files->fd[fd] = NULL;
	FD_CLR(fd, files->close_on_exec);
	__put_unused_fd(files, fd);
	write_unlock(&files->file_lock);
	return filp_close(filp, files);

out_unlock:
	write_unlock(&files->file_lock);
	return -EBADF;
}

/*
 * This routine simulates a hangup on the tty, to arrange that users
 * are given clean terminals at login time.
 */
asmlinkage long sys_vhangup(void)
{
	if (capable(CAP_SYS_TTY_CONFIG)) {
		tty_vhangup(current->tty);
		return 0;
	}
	return -EPERM;
}

/*
 * Called when an inode is about to be open.
 * We use this to disallow opening RW large files on 32bit systems if
 * the caller didn't specify O_LARGEFILE.  On 64bit systems we force
 * on this flag in sys_open.
 */
int generic_file_open(struct inode * inode, struct file * filp)
{
	if (!(filp->f_flags & O_LARGEFILE) && inode->i_size > MAX_NON_LFS)
		return -EFBIG;
	return 0;
}

EXPORT_SYMBOL(generic_file_open);
