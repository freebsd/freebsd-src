/*
 *  linux/fs/fcntl.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/dnotify.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/iobuf.h>
#include <linux/ptrace.h>

#include <asm/poll.h>
#include <asm/siginfo.h>
#include <asm/uaccess.h>

extern int sock_fcntl (struct file *, unsigned int cmd, unsigned long arg);
extern int fcntl_setlease(unsigned int fd, struct file *filp, long arg);
extern int fcntl_getlease(struct file *filp);

/* Expand files.  Return <0 on error; 0 nothing done; 1 files expanded,
 * we may have blocked. 
 *
 * Should be called with the files->file_lock spinlock held for write.
 */
static int expand_files(struct files_struct *files, int nr)
{
	int err, expand = 0;
#ifdef FDSET_DEBUG	
	printk (KERN_ERR __FUNCTION__ " %d: nr = %d\n", current->pid, nr);
#endif
	
	if (nr >= files->max_fdset) {
		expand = 1;
		if ((err = expand_fdset(files, nr)))
			goto out;
	}
	if (nr >= files->max_fds) {
		expand = 1;
		if ((err = expand_fd_array(files, nr)))
			goto out;
	}
	err = expand;
 out:
#ifdef FDSET_DEBUG	
	if (err)
		printk (KERN_ERR __FUNCTION__ " %d: return %d\n", current->pid, err);
#endif
	return err;
}

/*
 * locate_fd finds a free file descriptor in the open_fds fdset,
 * expanding the fd arrays if necessary.  The files write lock will be
 * held on exit to ensure that the fd can be entered atomically.
 */

static int locate_fd(struct files_struct *files, 
			    struct file *file, int orig_start)
{
	unsigned int newfd;
	int error;
	int start;

	write_lock(&files->file_lock);
	
	error = -EINVAL;
	if (orig_start >= current->rlim[RLIMIT_NOFILE].rlim_cur)
		goto out;

repeat:
	/*
	 * Someone might have closed fd's in the range
	 * orig_start..files->next_fd
	 */
	start = orig_start;
	if (start < files->next_fd)
		start = files->next_fd;

	newfd = start;
	if (start < files->max_fdset) {
		newfd = find_next_zero_bit(files->open_fds->fds_bits,
			files->max_fdset, start);
	}
	
	error = -EMFILE;
	if (newfd >= current->rlim[RLIMIT_NOFILE].rlim_cur)
		goto out;

	error = expand_files(files, newfd);
	if (error < 0)
		goto out;

	/*
	 * If we needed to expand the fs array we
	 * might have blocked - try again.
	 */
	if (error)
		goto repeat;

	if (start <= files->next_fd)
		files->next_fd = newfd + 1;
	
	error = newfd;
	
out:
	return error;
}

static inline void allocate_fd(struct files_struct *files, 
					struct file *file, int fd)
{
	FD_SET(fd, files->open_fds);
	FD_CLR(fd, files->close_on_exec);
	write_unlock(&files->file_lock);
	fd_install(fd, file);
}

static int dupfd(struct file *file, int start)
{
	struct files_struct * files = current->files;
	int ret;

	ret = locate_fd(files, file, start);
	if (ret < 0) 
		goto out_putf;
	allocate_fd(files, file, ret);
	return ret;

out_putf:
	write_unlock(&files->file_lock);
	fput(file);
	return ret;
}

asmlinkage long sys_dup2(unsigned int oldfd, unsigned int newfd)
{
	int err = -EBADF;
	struct file * file, *tofree;
	struct files_struct * files = current->files;

	write_lock(&files->file_lock);
	if (!(file = fcheck(oldfd)))
		goto out_unlock;
	err = newfd;
	if (newfd == oldfd)
		goto out_unlock;
	err = -EBADF;
	if (newfd >= current->rlim[RLIMIT_NOFILE].rlim_cur)
		goto out_unlock;
	get_file(file);			/* We are now finished with oldfd */

	err = expand_files(files, newfd);
	if (err < 0)
		goto out_fput;

	/* To avoid races with open() and dup(), we will mark the fd as
	 * in-use in the open-file bitmap throughout the entire dup2()
	 * process.  This is quite safe: do_close() uses the fd array
	 * entry, not the bitmap, to decide what work needs to be
	 * done.  --sct */
	/* Doesn't work. open() might be there first. --AV */

	/* Yes. It's a race. In user space. Nothing sane to do */
	err = -EBUSY;
	tofree = files->fd[newfd];
	if (!tofree && FD_ISSET(newfd, files->open_fds))
		goto out_fput;

	files->fd[newfd] = file;
	FD_SET(newfd, files->open_fds);
	FD_CLR(newfd, files->close_on_exec);
	write_unlock(&files->file_lock);

	if (tofree)
		filp_close(tofree, files);
	err = newfd;
out:
	return err;
out_unlock:
	write_unlock(&files->file_lock);
	goto out;

out_fput:
	write_unlock(&files->file_lock);
	fput(file);
	goto out;
}

asmlinkage long sys_dup(unsigned int fildes)
{
	int ret = -EBADF;
	struct file * file = fget(fildes);

	if (file)
		ret = dupfd(file, 0);
	return ret;
}

#define SETFL_MASK (O_APPEND | O_NONBLOCK | O_NDELAY | FASYNC | O_DIRECT)

static int setfl(int fd, struct file * filp, unsigned long arg)
{
	struct inode * inode = filp->f_dentry->d_inode;
	int error;

	/*
	 * In the case of an append-only file, O_APPEND
	 * cannot be cleared
	 */
	if (!(arg & O_APPEND) && IS_APPEND(inode))
		return -EPERM;

	/* Did FASYNC state change? */
	if ((arg ^ filp->f_flags) & FASYNC) {
		if (filp->f_op && filp->f_op->fasync) {
			error = filp->f_op->fasync(fd, filp, (arg & FASYNC) != 0);
			if (error < 0)
				return error;
		}
	}

	if (arg & O_DIRECT) {
		/*
		 * alloc_kiovec() can sleep and we are only serialized by
		 * the big kernel lock here, so abuse the i_sem to serialize
		 * this case too. We of course wouldn't need to go deep down
		 * to the inode layer, we could stay at the file layer, but
		 * we don't want to pay for the memory of a semaphore in each
		 * file structure too and we use the inode semaphore that we just
		 * pay for anyways.
		 */
		error = 0;
		down(&inode->i_sem);
		if (!filp->f_iobuf)
			error = alloc_kiovec(1, &filp->f_iobuf);
		up(&inode->i_sem);
		if (error < 0)
			return error;
	}

	/* required for strict SunOS emulation */
	if (O_NONBLOCK != O_NDELAY)
	       if (arg & O_NDELAY)
		   arg |= O_NONBLOCK;

	filp->f_flags = (arg & SETFL_MASK) | (filp->f_flags & ~SETFL_MASK);
	return 0;
}

static long do_fcntl(unsigned int fd, unsigned int cmd,
		     unsigned long arg, struct file * filp)
{
	long err = -EINVAL;

	switch (cmd) {
		case F_DUPFD:
			if (arg < NR_OPEN) {
				get_file(filp);
				err = dupfd(filp, arg);
			}
			break;
		case F_GETFD:
			err = get_close_on_exec(fd);
			break;
		case F_SETFD:
			err = 0;
			set_close_on_exec(fd, arg&1);
			break;
		case F_GETFL:
			err = filp->f_flags;
			break;
		case F_SETFL:
			lock_kernel();
			err = setfl(fd, filp, arg);
			unlock_kernel();
			break;
		case F_GETLK:
			err = fcntl_getlk(fd, (struct flock *) arg);
			break;
		case F_SETLK:
		case F_SETLKW:
			err = fcntl_setlk(fd, cmd, (struct flock *) arg);
			break;
		case F_GETOWN:
			/*
			 * XXX If f_owner is a process group, the
			 * negative return value will get converted
			 * into an error.  Oops.  If we keep the
			 * current syscall conventions, the only way
			 * to fix this will be in libc.
			 */
			err = filp->f_owner.pid;
			force_successful_syscall_return();
			break;
		case F_SETOWN:
			lock_kernel();
			filp->f_owner.pid = arg;
			filp->f_owner.uid = current->uid;
			filp->f_owner.euid = current->euid;
			err = 0;
			if (S_ISSOCK (filp->f_dentry->d_inode->i_mode))
				err = sock_fcntl (filp, F_SETOWN, arg);
			unlock_kernel();
			break;
		case F_GETSIG:
			err = filp->f_owner.signum;
			break;
		case F_SETSIG:
			/* arg == 0 restores default behaviour. */
			if (arg < 0 || arg > _NSIG) {
				break;
			}
			err = 0;
			filp->f_owner.signum = arg;
			break;
		case F_GETLEASE:
			err = fcntl_getlease(filp);
			break;
		case F_SETLEASE:
			err = fcntl_setlease(fd, filp, arg);
			break;
		case F_NOTIFY:
			err = fcntl_dirnotify(fd, filp, arg);
			break;
		default:
			/* sockets need a few special fcntls. */
			err = -EINVAL;
			if (S_ISSOCK (filp->f_dentry->d_inode->i_mode))
				err = sock_fcntl (filp, cmd, arg);
			break;
	}

	return err;
}

asmlinkage long sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{	
	struct file * filp;
	long err = -EBADF;

	filp = fget(fd);
	if (!filp)
		goto out;

	err = do_fcntl(fd, cmd, arg, filp);

 	fput(filp);
out:
	return err;
}

#if BITS_PER_LONG == 32
asmlinkage long sys_fcntl64(unsigned int fd, unsigned int cmd, unsigned long arg)
{	
	struct file * filp;
	long err;

	err = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	switch (cmd) {
		case F_GETLK64:
			err = fcntl_getlk64(fd, (struct flock64 *) arg);
			break;
		case F_SETLK64:
			err = fcntl_setlk64(fd, cmd, (struct flock64 *) arg);
			break;
		case F_SETLKW64:
			err = fcntl_setlk64(fd, cmd, (struct flock64 *) arg);
			break;
		default:
			err = do_fcntl(fd, cmd, arg, filp);
			break;
	}
	fput(filp);
out:
	return err;
}
#endif

/* Table to convert sigio signal codes into poll band bitmaps */

static long band_table[NSIGPOLL] = {
	POLLIN | POLLRDNORM,			/* POLL_IN */
	POLLOUT | POLLWRNORM | POLLWRBAND,	/* POLL_OUT */
	POLLIN | POLLRDNORM | POLLMSG,		/* POLL_MSG */
	POLLERR,				/* POLL_ERR */
	POLLPRI | POLLRDBAND,			/* POLL_PRI */
	POLLHUP | POLLERR			/* POLL_HUP */
};

static void send_sigio_to_task(struct task_struct *p,
			       struct fown_struct *fown, 
			       int fd,
			       int reason)
{
	if ((fown->euid != 0) &&
	    (fown->euid ^ p->suid) && (fown->euid ^ p->uid) &&
	    (fown->uid ^ p->suid) && (fown->uid ^ p->uid))
		return;
	switch (fown->signum) {
		siginfo_t si;
		default:
			/* Queue a rt signal with the appropriate fd as its
			   value.  We use SI_SIGIO as the source, not 
			   SI_KERNEL, since kernel signals always get 
			   delivered even if we can't queue.  Failure to
			   queue in this case _should_ be reported; we fall
			   back to SIGIO in that case. --sct */
			si.si_signo = fown->signum;
			si.si_errno = 0;
		        si.si_code  = reason;
			/* Make sure we are called with one of the POLL_*
			   reasons, otherwise we could leak kernel stack into
			   userspace.  */
			if ((reason & __SI_MASK) != __SI_POLL)
				BUG();
			if (reason - POLL_IN >= NSIGPOLL)
				si.si_band  = ~0L;
			else
				si.si_band = band_table[reason - POLL_IN];
			si.si_fd    = fd;
			if (!send_sig_info(fown->signum, &si, p))
				break;
		/* fall-through: fall back on the old plain SIGIO signal */
		case 0:
			send_sig(SIGIO, p, 1);
	}
}

void send_sigio(struct fown_struct *fown, int fd, int band)
{
	struct task_struct * p;
	int   pid	= fown->pid;
	
	read_lock(&tasklist_lock);
	if ( (pid > 0) && (p = find_task_by_pid(pid)) ) {
		send_sigio_to_task(p, fown, fd, band);
		goto out;
	}
	for_each_task(p) {
		int match = p->pid;
		if (pid < 0)
			match = -p->pgrp;
		if (pid != match)
			continue;
		send_sigio_to_task(p, fown, fd, band);
	}
out:
	read_unlock(&tasklist_lock);
}

static rwlock_t fasync_lock = RW_LOCK_UNLOCKED;
static kmem_cache_t *fasync_cache;

/*
 * fasync_helper() is used by some character device drivers (mainly mice)
 * to set up the fasync queue. It returns negative on error, 0 if it did
 * no changes and positive if it added/deleted the entry.
 */
int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp)
{
	struct fasync_struct *fa, **fp;
	struct fasync_struct *new = NULL;
	int result = 0;

	if (on) {
		new = kmem_cache_alloc(fasync_cache, SLAB_KERNEL);
		if (!new)
			return -ENOMEM;
	}
	write_lock_irq(&fasync_lock);
	for (fp = fapp; (fa = *fp) != NULL; fp = &fa->fa_next) {
		if (fa->fa_file == filp) {
			if(on) {
				fa->fa_fd = fd;
				kmem_cache_free(fasync_cache, new);
			} else {
				*fp = fa->fa_next;
				kmem_cache_free(fasync_cache, fa);
				result = 1;
			}
			goto out;
		}
	}

	if (on) {
		new->magic = FASYNC_MAGIC;
		new->fa_file = filp;
		new->fa_fd = fd;
		new->fa_next = *fapp;
		*fapp = new;
		result = 1;
	}
out:
	write_unlock_irq(&fasync_lock);
	return result;
}

void __kill_fasync(struct fasync_struct *fa, int sig, int band)
{
	while (fa) {
		struct fown_struct * fown;
		if (fa->magic != FASYNC_MAGIC) {
			printk(KERN_ERR "kill_fasync: bad magic number in "
			       "fasync_struct!\n");
			return;
		}
		fown = &fa->fa_file->f_owner;
		/* Don't send SIGURG to processes which have not set a
		   queued signum: SIGURG has its own default signalling
		   mechanism. */
		if (fown->pid && !(sig == SIGURG && fown->signum == 0))
			send_sigio(fown, fa->fa_fd, band);
		fa = fa->fa_next;
	}
}

void kill_fasync(struct fasync_struct **fp, int sig, int band)
{
	read_lock(&fasync_lock);
	__kill_fasync(*fp, sig, band);
	read_unlock(&fasync_lock);
}

static int __init fasync_init(void)
{
	fasync_cache = kmem_cache_create("fasync_cache",
		sizeof(struct fasync_struct), 0, 0, NULL, NULL);
	if (!fasync_cache)
		panic("cannot create fasync slab cache");
	return 0;
}

module_init(fasync_init)
