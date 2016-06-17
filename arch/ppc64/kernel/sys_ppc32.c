/*
 * sys_ppc32.c: Conversion between 32bit and 64bit native syscalls.
 *
 * Copyright (C) 2001 IBM
 * Copyright (C) 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * environment.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <asm/ptrace.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h> 
#include <linux/mm.h> 
#include <linux/file.h> 
#include <linux/signal.h>
#include <linux/utime.h>
#include <linux/resource.h>
#include <linux/times.h>
#include <linux/utsname.h>
#include <linux/timex.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/nfs_fs.h>
#include <linux/smb_fs.h>
#include <linux/smb_mount.h>
#include <linux/ncp_fs.h>
#include <linux/quota.h>
#include <linux/quotacompat.h>
#include <linux/module.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/cache.h>
#include <linux/nfsd/xdr.h>
#include <linux/nfsd/syscall.h>
#include <linux/poll.h>
#include <linux/personality.h>
#include <linux/stat.h>
#include <linux/filter.h>
#include <linux/highmem.h>
#include <linux/highuid.h>
#include <linux/mman.h>
#include <linux/sysctl.h>

#include <asm/types.h>
#include <asm/ipc.h>
#include <asm/uaccess.h>

#include <asm/semaphore.h>

#include <net/scm.h>
#include <linux/elf.h>
#include <asm/ppcdebug.h>
#include <asm/time.h>
#include <asm/ppc32.h>

extern unsigned long wall_jiffies;
#define USEC_PER_SEC (1000000)


extern asmlinkage long sys_utime(char * filename, struct utimbuf * times);

struct utimbuf32 {
	__kernel_time_t32 actime, modtime;
};

asmlinkage long sys32_utime(char * filename, struct utimbuf32 *times)
{
	struct utimbuf t;
	mm_segment_t old_fs;
	int ret;
	char *filenam;
	
	PPCDBG(PPCDBG_SYS32NI, "sys32_utime - running - filename=%s, times=%p - pid=%ld, comm=%s \n", filename, times, current->pid, current->comm);

	if (!times)
		return sys_utime(filename, NULL);
	if (get_user(t.actime, &times->actime) || __get_user(t.modtime, &times->modtime))
		return -EFAULT;
	filenam = getname(filename);

	ret = PTR_ERR(filenam);
	if (!IS_ERR(filenam)) {
		old_fs = get_fs();
		set_fs (KERNEL_DS); 
		ret = sys_utime(filenam, &t);
		set_fs (old_fs);
		putname (filenam);
	}

	return ret;
}

asmlinkage int sys32_ustat(__kernel_dev_t32 dev, struct ustat32 * ubuf)
{

	struct super_block *s;
	struct ustat32 tmp;
	struct statfs sbuf;
	int err = -EINVAL;

	s = get_super(to_kdev_t(dev));
	if (s == NULL)
		goto out;
	err = vfs_statfs(s, &sbuf);
	drop_super(s);
	if (err)
		goto out;

	memset(&tmp,0,sizeof(struct ustat));
	tmp.f_tfree = sbuf.f_bfree;
	tmp.f_tinode = sbuf.f_ffree;

	err = copy_to_user(ubuf, &tmp, sizeof(struct ustat32)) ? -EFAULT : 0;

out:
	return err;

}

struct iovec32 { u32 iov_base; __kernel_size_t32 iov_len; };

typedef ssize_t (*IO_fn_t)(struct file *, char *, size_t, loff_t *);
typedef __kernel_ssize_t32 ssize_t32;

static long do_readv_writev32(int type, struct file *file,
			      const struct iovec32 *vector, u32 count)
{
	unsigned long tot_len;
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov=iovstack, *ivp;
	struct inode *inode;
	long retval, i;
	IO_fn_t fn;

	/* First get the "struct iovec" from user memory and
	 * verify all the pointers
	 */
	if (!count)
		return 0;
	if (count > UIO_MAXIOV)
		return -EINVAL;
	if(verify_area(VERIFY_READ, vector, sizeof(struct iovec32)*count))
		return -EFAULT;
	if (count > UIO_FASTIOV) {
		iov = kmalloc(count*sizeof(struct iovec), GFP_KERNEL);
		if (!iov)
			return -ENOMEM;
	}

	tot_len = 0;
	i = count;
	ivp = iov;
	while(i > 0) {
		u32 len;
		u32 buf;

		__get_user(len, &vector->iov_len);
		__get_user(buf, &vector->iov_base);
		tot_len += len;
		ivp->iov_base = (void *)A(buf);
		ivp->iov_len = (__kernel_size_t) len;
		vector++;
		ivp++;
		if ((len < 0) || (tot_len != (ssize_t32)tot_len)) {
			if (iov != iovstack)
				kfree(iov);
			return -EINVAL;
		}
		i--;
	}

	inode = file->f_dentry->d_inode;
	/* VERIFY_WRITE actually means a read, as we write to user space */
	retval = locks_verify_area((type == VERIFY_WRITE
				    ? FLOCK_VERIFY_READ : FLOCK_VERIFY_WRITE),
				   inode, file, file->f_pos, tot_len);
	if (retval) {
		if (iov != iovstack)
			kfree(iov);
		return retval;
	}

	/* Then do the actual IO.  Note that sockets need to be handled
	 * specially as they have atomicity guarantees and can handle
	 * iovec's natively
	 */
	if (inode->i_sock) {
		int err;
		err = sock_readv_writev(type, inode, file, iov, count, tot_len);
		if (iov != iovstack)
			kfree(iov);
		return err;
	}

	if (!file->f_op) {
		if (iov != iovstack)
			kfree(iov);
		return -EINVAL;
	}
	/* VERIFY_WRITE actually means a read, as we write to user space */
	fn = file->f_op->read;
	if (type == VERIFY_READ)
		fn = (IO_fn_t) file->f_op->write;		
	ivp = iov;
	while (count > 0) {
		void * base;
		int len, nr;

		base = ivp->iov_base;
		len = ivp->iov_len;
		ivp++;
		count--;
		nr = fn(file, base, len, &file->f_pos);
		if (nr < 0) {
			if (retval)
				break;
			retval = nr;
			break;
		}
		retval += nr;
		if (nr != len)
			break;
	}
	if (iov != iovstack)
		kfree(iov);
	return retval;
}

asmlinkage long sys32_readv(u32 fd, struct iovec32 *vector, u32 count)
{
	struct file *file;
	long ret = -EBADF;
	
	PPCDBG(PPCDBG_SYS32, "sys32_readv - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	file = fget(fd);
	if(!file)
		goto bad_file;

	if (file->f_op && (file->f_mode & FMODE_READ) &&
     (file->f_op->readv || file->f_op->read))
		ret = do_readv_writev32(VERIFY_WRITE, file, vector, count);
	fput(file);

bad_file:
	PPCDBG(PPCDBG_SYS32, "sys32_readv - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return ret;
}

asmlinkage long sys32_writev(u32 fd, struct iovec32 *vector, u32 count)
{
	struct file *file;
	int ret = -EBADF;
	
	PPCDBG(PPCDBG_SYS32, "sys32_writev - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	file = fget(fd);
	if(!file)
		goto bad_file;
	if (file->f_op && (file->f_mode & FMODE_WRITE) &&
	   (file->f_op->writev || file->f_op->write))
		ret = do_readv_writev32(VERIFY_READ, file, vector, count);
	fput(file);

bad_file:
	PPCDBG(PPCDBG_SYS32, "sys32_writev - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return ret;
}



static inline int get_flock(struct flock *kfl, struct flock32 *ufl)
{
	int err;
	
	err = get_user(kfl->l_type, &ufl->l_type);
	err |= __get_user(kfl->l_whence, &ufl->l_whence);
	err |= __get_user(kfl->l_start, &ufl->l_start);
	err |= __get_user(kfl->l_len, &ufl->l_len);
	err |= __get_user(kfl->l_pid, &ufl->l_pid);
	return err;
}

static inline int put_flock(struct flock *kfl, struct flock32 *ufl)
{
	int err;
	
	err = __put_user(kfl->l_type, &ufl->l_type);
	err |= __put_user(kfl->l_whence, &ufl->l_whence);
	err |= __put_user(kfl->l_start, &ufl->l_start);
	err |= __put_user(kfl->l_len, &ufl->l_len);
	err |= __put_user(kfl->l_pid, &ufl->l_pid);
	return err;
}

extern asmlinkage long sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);
asmlinkage long sys32_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	{
		struct flock f;
		mm_segment_t old_fs;
		long ret;

		if(get_flock(&f, (struct flock32 *)arg))
			return -EFAULT;
		old_fs = get_fs(); set_fs (KERNEL_DS);
		ret = sys_fcntl(fd, cmd, (unsigned long)&f);
		set_fs (old_fs);
		if(put_flock(&f, (struct flock32 *)arg))
			return -EFAULT;
		return ret;
	}
	default:
		return sys_fcntl(fd, cmd, (unsigned long)arg);
	}
}

struct ncp_mount_data32 {
        int version;
        unsigned int ncp_fd;
        __kernel_uid_t32 mounted_uid;
        __kernel_pid_t32 wdog_pid;
        unsigned char mounted_vol[NCP_VOLNAME_LEN + 1];
        unsigned int time_out;
        unsigned int retry_count;
        unsigned int flags;
        __kernel_uid_t32 uid;
        __kernel_gid_t32 gid;
        __kernel_mode_t32 file_mode;
        __kernel_mode_t32 dir_mode;
};

static void *do_ncp_super_data_conv(void *raw_data)
{
	struct ncp_mount_data *n = (struct ncp_mount_data *)raw_data;
	struct ncp_mount_data32 *n32 = (struct ncp_mount_data32 *)raw_data;

	n->dir_mode = n32->dir_mode;
	n->file_mode = n32->file_mode;
	n->gid = n32->gid;
	n->uid = n32->uid;
	memmove (n->mounted_vol, n32->mounted_vol, (sizeof (n32->mounted_vol) + 3 * sizeof (unsigned int)));
	n->wdog_pid = n32->wdog_pid;
	n->mounted_uid = n32->mounted_uid;
	return raw_data;
}

struct smb_mount_data32 {
        int version;
        __kernel_uid_t32 mounted_uid;
        __kernel_uid_t32 uid;
        __kernel_gid_t32 gid;
        __kernel_mode_t32 file_mode;
        __kernel_mode_t32 dir_mode;
};

static void *do_smb_super_data_conv(void *raw_data)
{
	struct smb_mount_data *s = (struct smb_mount_data *)raw_data;
	struct smb_mount_data32 *s32 = (struct smb_mount_data32 *)raw_data;

	s->version = s32->version;
	s->mounted_uid = s32->mounted_uid;
	s->uid = s32->uid;
	s->gid = s32->gid;
	s->file_mode = s32->file_mode;
	s->dir_mode = s32->dir_mode;
	return raw_data;
}

static int copy_mount_stuff_to_kernel(const void *user, unsigned long *kernel)
{
	int i;
	unsigned long page;
	struct vm_area_struct *vma;

	*kernel = 0;
	if(!user)
		return 0;
	vma = find_vma(current->mm, (unsigned long)user);
	if(!vma || (unsigned long)user < vma->vm_start)
		return -EFAULT;
	if(!(vma->vm_flags & VM_READ))
		return -EFAULT;
	i = vma->vm_end - (unsigned long) user;
	if(PAGE_SIZE <= (unsigned long) i)
		i = PAGE_SIZE - 1;
	if(!(page = __get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	if(copy_from_user((void *) page, user, i)) {
		free_page(page);
		return -EFAULT;
	}
	*kernel = page;
	return 0;
}

#define SMBFS_NAME	"smbfs"
#define NCPFS_NAME	"ncpfs"

asmlinkage long sys32_mount(char *dev_name, char *dir_name, char *type, unsigned long new_flags, u32 data)
{
	unsigned long type_page = 0;
	unsigned long data_page = 0;
	unsigned long dev_page = 0;
	unsigned long dir_page = 0;
	int err, is_smb, is_ncp;
	
	PPCDBG(PPCDBG_SYS32, "sys32_mount - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	is_smb = is_ncp = 0;

	err = copy_mount_stuff_to_kernel((const void *)type, &type_page);
	if (err)
		goto out;

	if (!type_page) {
		err = -EINVAL;
		goto out;
	}

	is_smb = !strcmp((char *)type_page, SMBFS_NAME);
	is_ncp = !strcmp((char *)type_page, NCPFS_NAME);

	err = copy_mount_stuff_to_kernel((const void *)AA(data), &data_page);
	if (err)
		goto type_out;

	err = copy_mount_stuff_to_kernel(dev_name, &dev_page);
	if (err)
		goto data_out;

	err = copy_mount_stuff_to_kernel(dir_name, &dir_page);
	if (err)
		goto dev_out;

	if (!is_smb && !is_ncp) {
		lock_kernel();
		err = do_mount((char*)dev_page, (char*)dir_page,
				(char*)type_page, new_flags, (char*)data_page);
		unlock_kernel();
	} else {
		if (is_ncp)
			do_ncp_super_data_conv((void *)data_page);
		else
			do_smb_super_data_conv((void *)data_page);

		lock_kernel();
		err = do_mount((char*)dev_page, (char*)dir_page,
				(char*)type_page, new_flags, (char*)data_page);
		unlock_kernel();
	}
	free_page(dir_page);

dev_out:
	free_page(dev_page);

data_out:
	free_page(data_page);

type_out:
	free_page(type_page);

out:
	
	PPCDBG(PPCDBG_SYS32, "sys32_mount - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return err;
}

struct user_dqblk32 {
    __u32 dqb_bhardlimit;
    __u32 dqb_bsoftlimit;
    __u32 dqb_curblocks;
    __u32 dqb_ihardlimit;
    __u32 dqb_isoftlimit;
    __u32 dqb_curinodes;
    __kernel_time_t32 dqb_btime;
    __kernel_time_t32 dqb_itime;
};
                                

extern asmlinkage long sys_quotactl(unsigned int cmd, const char *special, qid_t id, caddr_t addr);

/* Note: it is necessary to treat cmd and id as unsigned ints, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_quotactl(u32 cmd_parm, const char *special, u32 id_parm, caddr_t addr)
{
	unsigned int cmd = cmd_parm;
	qid_t id  = (qid_t)id_parm;
	unsigned int cmds = cmd >> SUBCMDSHIFT;
	int err;
	struct v1c_mem_dqblk d;
	mm_segment_t old_fs;
	char *spec;
	
	PPCDBG(PPCDBG_SYS32, "sys32_quotactl - entered - pid=%ld current=%lx comm=%s \n",
		    current->pid, current, current->comm);

	switch (cmds) {
	case Q_V1_GETQUOTA:
		break;
	case Q_V1_SETQUOTA:
	case Q_V1_SETUSE:
	case Q_V1_SETQLIM:
		if (copy_from_user(&d, addr, sizeof(struct user_dqblk32)))
			return -EFAULT;
		d.dqb_itime = ((struct user_dqblk32 *)&d)->dqb_itime;
		d.dqb_btime = ((struct user_dqblk32 *)&d)->dqb_btime;
		break;
	default:
		return sys_quotactl(cmd, special, id, addr);
	}
	spec = getname(special);
	err = PTR_ERR(spec);
	if (IS_ERR(spec))
		return err;
	old_fs = get_fs();
	set_fs (KERNEL_DS);
	err = sys_quotactl(cmd, (const char *)spec, id, (caddr_t)&d);
	set_fs (old_fs);
	putname (spec);
	if (cmds == Q_V1_GETQUOTA) {
		__kernel_time_t b = d.dqb_btime, i = d.dqb_itime;
		((struct user_dqblk32 *)&d)->dqb_itime = i;
		((struct user_dqblk32 *)&d)->dqb_btime = b;
		if (copy_to_user(addr, &d, sizeof(struct user_dqblk32)))
			return -EFAULT;
	}
	
	PPCDBG(PPCDBG_SYS32, "sys32_quotactl - exited - pid=%ld current=%lx comm=%s \n",
		    current->pid, current, current->comm);

	return err;
}



/* readdir & getdents */
#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP(x) (((x)+sizeof(u32)-1) & ~(sizeof(u32)-1))

struct old_linux_dirent32 {
	u32		d_ino;
	u32		d_offset;
	unsigned short	d_namlen;
	char		d_name[1];
};

struct readdir_callback32 {
	struct old_linux_dirent32 * dirent;
	int count;
};

static int fillonedir(void * __buf, const char * name, int namlen,
		                  off_t offset, ino_t ino, unsigned int d_type)
{
	struct readdir_callback32 * buf = (struct readdir_callback32 *) __buf;
	struct old_linux_dirent32 * dirent;

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

asmlinkage int old32_readdir(unsigned int fd, struct old_linux_dirent32 *dirent, unsigned int count)
{
	int error = -EBADF;
	struct file * file;
	struct readdir_callback32 buf;

	file = fget(fd);
	if (!file)
		goto out;

	buf.count = 0;
	buf.dirent = dirent;

	error = vfs_readdir(file, (filldir_t)fillonedir, &buf);
	if (error < 0)
		goto out_putf;
	error = buf.count;

out_putf:
	fput(file);
out:
	return error;
}

#if 0
struct linux_dirent32 {
	u32		d_ino;
	u32		d_off;
	unsigned short	d_reclen;
	char		d_name[1];
};
#else
struct linux_dirent32 {
	u32		d_ino;
	u32		d_off;
	unsigned short	d_reclen;
  /* unsigned char	d_type; */
	char		d_name[256];
};
#endif

struct getdents_callback32 {
	struct linux_dirent32 * current_dir;
	struct linux_dirent32 * previous;
	int count;
	int error;
};

static int
filldir(void * __buf, const char * name, int namlen, off_t offset, ino_t ino,
		               unsigned int d_type)
{
	struct linux_dirent32 * dirent;
	struct getdents_callback32 * buf = (struct getdents_callback32 *) __buf;
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

asmlinkage long sys32_getdents(unsigned int fd, struct linux_dirent32 *dirent, unsigned int count)
{
	struct file * file;
	struct linux_dirent32 * lastdirent;
	struct getdents_callback32 buf;
	int error = -EBADF;

	PPCDBG(PPCDBG_SYS32NI, "sys32_getdents - running - fd=%x, pid=%ld, comm=%s \n", fd, current->pid, current->comm);

	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, (filldir_t)filldir, &buf);
	if (error < 0)
		goto out_putf;
	lastdirent = buf.previous;
	error = buf.error;
	if(lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = count - buf.count;
	}
 out_putf:
	fput(file);

 out:
	return error;
}
/* end of readdir & getdents */



/* 32-bit timeval and related flotsam.  */

struct timeval32
{
	int tv_sec, tv_usec;
};

struct itimerval32
{
	struct timeval32 it_interval;
	struct timeval32 it_value;
};




/*
 * Ooo, nasty.  We need here to frob 32-bit unsigned longs to
 * 64-bit unsigned longs.
 */
static inline int
get_fd_set32(unsigned long n, unsigned long *fdset, u32 *ufdset)
{
	if (ufdset) {
		unsigned long odd;

		if (verify_area(VERIFY_WRITE, ufdset, n*sizeof(u32)))
			return -EFAULT;

		odd = n & 1UL;
		n &= ~1UL;
		while (n) {
			unsigned long h, l;
			__get_user(l, ufdset);
			__get_user(h, ufdset+1);
			ufdset += 2;
			*fdset++ = h << 32 | l;
			n -= 2;
		}
		if (odd)
			__get_user(*fdset, ufdset);
	} else {
		/* Tricky, must clear full unsigned long in the
		 * kernel fdset at the end, this makes sure that
		 * actually happens.
		 */
		memset(fdset, 0, ((n + 1) & ~1)*sizeof(u32));
	}
	return 0;
}

static inline void
set_fd_set32(unsigned long n, u32 *ufdset, unsigned long *fdset)
{
	unsigned long odd;

	if (!ufdset)
		return;

	odd = n & 1UL;
	n &= ~1UL;
	while (n) {
		unsigned long h, l;
		l = *fdset++;
		h = l >> 32;
		__put_user(l, ufdset);
		__put_user(h, ufdset+1);
		ufdset += 2;
		n -= 2;
	}
	if (odd)
		__put_user(*fdset, ufdset);
}



#define MAX_SELECT_SECONDS ((unsigned long) (MAX_SCHEDULE_TIMEOUT / HZ)-1)

asmlinkage long sys32_select(int n, u32 *inp, u32 *outp, u32 *exp, u32 tvp_x)
{
	fd_set_bits fds;
	struct timeval32 *tvp = (struct timeval32 *)AA(tvp_x);
	char *bits;
	unsigned long nn;
	long timeout;
	int ret, size;
	
  PPCDBG(PPCDBG_SYS32X, "sys32_select - entered - n=%x, inp=%p, outp=%p - pid=%ld comm=%s \n", n, inp, outp, current->pid, current->comm);

	timeout = MAX_SCHEDULE_TIMEOUT;
	if (tvp) {
		time_t sec, usec;
		if ((ret = verify_area(VERIFY_READ, tvp, sizeof(*tvp)))
		    || (ret = __get_user(sec, &tvp->tv_sec))
		    || (ret = __get_user(usec, &tvp->tv_usec)))
			goto out_nofds;

		ret = -EINVAL;
		if(sec < 0 || usec < 0)
			goto out_nofds;

		if ((unsigned long) sec < MAX_SELECT_SECONDS) {
			timeout = (usec + 1000000/HZ - 1) / (1000000/HZ);
			timeout += sec * (unsigned long) HZ;
		}
	}

	ret = -EINVAL;
	if (n < 0)
		goto out_nofds;
	if (n > current->files->max_fdset)
		n = current->files->max_fdset;

	/*
	 * We need 6 bitmaps (in/out/ex for both incoming and outgoing),
	 * since we used fdset we need to allocate memory in units of
	 * long-words. 
	 */
	ret = -ENOMEM;
	size = FDS_BYTES(n);
	bits = kmalloc(6 * size, GFP_KERNEL);
	if (!bits)
		goto out_nofds;
	fds.in      = (unsigned long *)  bits;
	fds.out     = (unsigned long *) (bits +   size);
	fds.ex      = (unsigned long *) (bits + 2*size);
	fds.res_in  = (unsigned long *) (bits + 3*size);
	fds.res_out = (unsigned long *) (bits + 4*size);
	fds.res_ex  = (unsigned long *) (bits + 5*size);

	nn = (n + 8*sizeof(u32) - 1) / (8*sizeof(u32));
	if ((ret = get_fd_set32(nn, fds.in, inp)) ||
	    (ret = get_fd_set32(nn, fds.out, outp)) ||
	    (ret = get_fd_set32(nn, fds.ex, exp)))
		goto out;
	zero_fd_set(n, fds.res_in);
	zero_fd_set(n, fds.res_out);
	zero_fd_set(n, fds.res_ex);

	ret = do_select(n, &fds, &timeout);

	if (tvp && !(current->personality & STICKY_TIMEOUTS)) {
		time_t sec = 0, usec = 0;
		if (timeout) {
			sec = timeout / HZ;
			usec = timeout % HZ;
			usec *= (1000000/HZ);
		}
		put_user(sec, &tvp->tv_sec);
		put_user(usec, &tvp->tv_usec);
	}

	if (ret < 0)
		goto out;
	if (!ret) {
		ret = -ERESTARTNOHAND;
		if (signal_pending(current))
			goto out;
		ret = 0;
	}

	set_fd_set32(nn, inp, fds.res_in);
	set_fd_set32(nn, outp, fds.res_out);
	set_fd_set32(nn, exp, fds.res_ex);
  
out:
	kfree(bits);

out_nofds:
	PPCDBG(PPCDBG_SYS32X, "sys32_select - exited - pid=%ld, comm=%s \n", current->pid, current->comm);
	return ret;
}

/* Note: it is necessary to treat n as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage int ppc32_select(u32 n, u32* inp, u32* outp, u32* exp, u32 tvp_x)
{
	return sys32_select((int)n, inp, outp, exp, tvp_x);
}

static int cp_new_stat32(struct inode *inode, struct stat32 *statbuf)
{
	unsigned long ino, blksize, blocks;
	kdev_t dev, rdev;
	umode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	off_t size;
	time_t atime, mtime, ctime;
	int err;

	/* Stream the loads of inode data into the load buffer,
	 * then we push it all into the store buffer below.  This
	 * should give optimal cache performance.
	 */
	ino = inode->i_ino;
	dev = inode->i_dev;
	mode = inode->i_mode;
	nlink = inode->i_nlink;
	uid = inode->i_uid;
	gid = inode->i_gid;
	rdev = inode->i_rdev;
	size = inode->i_size;
	atime = inode->i_atime;
	mtime = inode->i_mtime;
	ctime = inode->i_ctime;
	blksize = inode->i_blksize;
	blocks = inode->i_blocks;

	err  = put_user(kdev_t_to_nr(dev), &statbuf->st_dev);
	err |= put_user(ino, &statbuf->st_ino);
	err |= put_user(mode, &statbuf->st_mode);
	err |= put_user(nlink, &statbuf->st_nlink);
	err |= put_user(uid, &statbuf->st_uid);
	err |= put_user(gid, &statbuf->st_gid);
	err |= put_user(kdev_t_to_nr(rdev), &statbuf->st_rdev);
	err |= put_user(size, &statbuf->st_size);
	err |= put_user(atime, &statbuf->st_atime);
	err |= put_user(0, &statbuf->__unused1);
	err |= put_user(mtime, &statbuf->st_mtime);
	err |= put_user(0, &statbuf->__unused2);
	err |= put_user(ctime, &statbuf->st_ctime);
	err |= put_user(0, &statbuf->__unused3);
	if (blksize) {
		err |= put_user(blksize, &statbuf->st_blksize);
		err |= put_user(blocks, &statbuf->st_blocks);
	} else {
		unsigned int tmp_blocks;

#define D_B   7
#define I_B   (BLOCK_SIZE / sizeof(unsigned short))
		tmp_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
		if (tmp_blocks > D_B) {
			unsigned int indirect;

			indirect = (tmp_blocks - D_B + I_B - 1) / I_B;
			tmp_blocks += indirect;
			if (indirect > 1) {
				indirect = (indirect - 1 + I_B - 1) / I_B;
				tmp_blocks += indirect;
				if (indirect > 1)
					tmp_blocks++;
			}
		}
		err |= put_user(BLOCK_SIZE, &statbuf->st_blksize);
		err |= put_user((BLOCK_SIZE / 512) * tmp_blocks, &statbuf->st_blocks);
#undef D_B
#undef I_B
	}
	err |= put_user(0, &statbuf->__unused4[0]);
	err |= put_user(0, &statbuf->__unused4[1]);

	return err;
}

static __inline__ int
do_revalidate(struct dentry *dentry)
{
	struct inode * inode = dentry->d_inode;
	if (inode->i_op && inode->i_op->revalidate)
		return inode->i_op->revalidate(dentry);
	return 0;
}

asmlinkage long sys32_newstat(char* filename, struct stat32* statbuf)
{
	struct nameidata nd;
	int error;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_newstat - running - filename=%s, statbuf=%p, pid=%ld, comm=%s\n", filename, statbuf, current->pid, current->comm);

	error = user_path_walk(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_new_stat32(nd.dentry->d_inode, statbuf);
		path_release(&nd);
	}
	return error;
}

asmlinkage long sys32_newlstat(char * filename, struct stat32 *statbuf)
{
	struct nameidata nd;
	int error;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_newlstat - running - fn=%s, pid=%ld, comm=%s\n", filename, current->pid, current->comm);

	error = user_path_walk_link(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_new_stat32(nd.dentry->d_inode, statbuf);

		path_release(&nd);
	}
	return error;
}

asmlinkage long sys32_newfstat(unsigned int fd, struct stat32 *statbuf)
{
	struct file *f;
	int err = -EBADF;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_newfstat - running - fd=%x, pid=%ld, comm=%s\n", fd, current->pid, current->comm);

	f = fget(fd);
	if (f) {
		struct dentry * dentry = f->f_dentry;

		err = do_revalidate(dentry);
		if (!err)
			err = cp_new_stat32(dentry->d_inode, statbuf);
		fput(f);
	}
	return err;
}

static inline int put_statfs (struct statfs32 *ubuf, struct statfs *kbuf)
{
	struct statfs32 tmp;
	memset(&tmp, 0, sizeof(tmp));

	tmp.f_type = kbuf->f_type;
	tmp.f_bsize = kbuf->f_bsize;
	tmp.f_blocks = kbuf->f_blocks;
	tmp.f_bfree = kbuf->f_bfree;
	tmp.f_bavail = kbuf->f_bavail;
	tmp.f_files = kbuf->f_files;
	tmp.f_ffree = kbuf->f_ffree;
	tmp.f_namelen = kbuf->f_namelen;
	tmp.f_fsid.val[0] = kbuf->f_fsid.val[0];
	tmp.f_fsid.val[1] = kbuf->f_fsid.val[1];

	return copy_to_user(ubuf, &tmp, sizeof(tmp)) ? -EFAULT : 0;
}

extern asmlinkage int sys_statfs(const char * path, struct statfs * buf);

asmlinkage long sys32_statfs(const char * path, struct statfs32 *buf)
{
	int ret;
	struct statfs s;
	mm_segment_t old_fs = get_fs();
	char *pth;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_statfs - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
	
	pth = getname (path);
	ret = PTR_ERR(pth);
	if (!IS_ERR(pth)) {
		set_fs (KERNEL_DS);
		ret = sys_statfs((const char *)pth, &s);
		set_fs (old_fs);
		putname (pth);
		if (put_statfs(buf, &s))
			return -EFAULT;
	}
	
	PPCDBG(PPCDBG_SYS32X, "sys32_statfs - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return ret;
}

extern asmlinkage long sys_fstatfs(unsigned int fd, struct statfs * buf);

asmlinkage long sys32_fstatfs(unsigned int fd, struct statfs32 *buf)
{
	int ret;
	struct statfs s;
	mm_segment_t old_fs = get_fs();
	
	PPCDBG(PPCDBG_SYS32X, "sys32_fstatfs - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
	
	set_fs (KERNEL_DS);
	ret = sys_fstatfs(fd, &s);
	set_fs (old_fs);
	if (put_statfs(buf, &s))
		return -EFAULT;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_fstatfs - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return ret;
}



extern asmlinkage long sys_sysfs(int option, unsigned long arg1, unsigned long arg2);

/* Note: it is necessary to treat option as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sysfs(u32 option, u32 arg1, u32 arg2)
{
	PPCDBG(PPCDBG_SYS32, "sys32_sysfs - running - pid=%ld, comm=%s\n", current->pid, current->comm);
	return sys_sysfs((int)option, arg1, arg2);
}




extern unsigned long do_mremap(unsigned long addr,
	unsigned long old_len, unsigned long new_len,
	unsigned long flags, unsigned long new_addr);
                
asmlinkage unsigned long sys32_mremap(unsigned long addr, unsigned long old_len, unsigned long new_len,
                                    	unsigned long flags, u32 __new_addr)
{
	unsigned long ret = -EINVAL;
	unsigned long new_addr = AA(__new_addr);
	
	PPCDBG(PPCDBG_SYS32, "sys32_mremap - entered - pid=%ld current=%lx comm=%s\n",
		    current->pid, current, current->comm);


	if (old_len > 0xf0000000UL || new_len > 0xf0000000UL)
		goto out;
	if (addr > 0xf0000000UL - old_len)
		goto out;
	down_write(&current->mm->mmap_sem);
	if (flags & MREMAP_FIXED) {
		if (new_addr > 0xf0000000UL - new_len)
			goto out_sem;
	} else if (addr > 0xf0000000UL - new_len) {
		ret = -ENOMEM;
		if (!(flags & MREMAP_MAYMOVE))
			goto out_sem;
		new_addr = get_unmapped_area (NULL, addr, new_len, 0, 0);
		if (!new_addr)
			goto out_sem;
		flags |= MREMAP_FIXED;
	}
	ret = do_mremap(addr, old_len, new_len, flags, new_addr);
out_sem:
	up_write(&current->mm->mmap_sem);
out:
	
	PPCDBG(PPCDBG_SYS32, "sys32_mremap - exited - pid=%ld current=%lx comm=%s\n",
		    current->pid, current, current->comm);

	return ret;       
}



/* Handle adjtimex compatability. */
struct timex32 {
	u32 modes;
	s32 offset, freq, maxerror, esterror;
	s32 status, constant, precision, tolerance;
	struct timeval32 time;
	s32 tick;
	s32 ppsfreq, jitter, shift, stabil;
	s32 jitcnt, calcnt, errcnt, stbcnt;
	s32  :32; s32  :32; s32  :32; s32  :32;
	s32  :32; s32  :32; s32  :32; s32  :32;
	s32  :32; s32  :32; s32  :32; s32  :32;
};

extern int do_adjtimex(struct timex *);
extern void ppc_adjtimex(void);

asmlinkage long sys32_adjtimex(struct timex32 *utp)
{
	struct timex txc;
	int ret;
	
	PPCDBG(PPCDBG_SYS32, "sys32_adjtimex - running - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	memset(&txc, 0, sizeof(struct timex));

	if(get_user(txc.modes, &utp->modes) ||
	   __get_user(txc.offset, &utp->offset) ||
	   __get_user(txc.freq, &utp->freq) ||
	   __get_user(txc.maxerror, &utp->maxerror) ||
	   __get_user(txc.esterror, &utp->esterror) ||
	   __get_user(txc.status, &utp->status) ||
	   __get_user(txc.constant, &utp->constant) ||
	   __get_user(txc.precision, &utp->precision) ||
	   __get_user(txc.tolerance, &utp->tolerance) ||
	   __get_user(txc.time.tv_sec, &utp->time.tv_sec) ||
	   __get_user(txc.time.tv_usec, &utp->time.tv_usec) ||
	   __get_user(txc.tick, &utp->tick) ||
	   __get_user(txc.ppsfreq, &utp->ppsfreq) ||
	   __get_user(txc.jitter, &utp->jitter) ||
	   __get_user(txc.shift, &utp->shift) ||
	   __get_user(txc.stabil, &utp->stabil) ||
	   __get_user(txc.jitcnt, &utp->jitcnt) ||
	   __get_user(txc.calcnt, &utp->calcnt) ||
	   __get_user(txc.errcnt, &utp->errcnt) ||
	   __get_user(txc.stbcnt, &utp->stbcnt))
		return -EFAULT;

	ret = do_adjtimex(&txc);

	/* adjust the conversion of TB to time of day to track adjtimex */
	ppc_adjtimex();

	if(put_user(txc.modes, &utp->modes) ||
	   __put_user(txc.offset, &utp->offset) ||
	   __put_user(txc.freq, &utp->freq) ||
	   __put_user(txc.maxerror, &utp->maxerror) ||
	   __put_user(txc.esterror, &utp->esterror) ||
	   __put_user(txc.status, &utp->status) ||
	   __put_user(txc.constant, &utp->constant) ||
	   __put_user(txc.precision, &utp->precision) ||
	   __put_user(txc.tolerance, &utp->tolerance) ||
	   __put_user(txc.time.tv_sec, &utp->time.tv_sec) ||
	   __put_user(txc.time.tv_usec, &utp->time.tv_usec) ||
	   __put_user(txc.tick, &utp->tick) ||
	   __put_user(txc.ppsfreq, &utp->ppsfreq) ||
	   __put_user(txc.jitter, &utp->jitter) ||
	   __put_user(txc.shift, &utp->shift) ||
	   __put_user(txc.stabil, &utp->stabil) ||
	   __put_user(txc.jitcnt, &utp->jitcnt) ||
	   __put_user(txc.calcnt, &utp->calcnt) ||
	   __put_user(txc.errcnt, &utp->errcnt) ||
	   __put_user(txc.stbcnt, &utp->stbcnt))
		ret = -EFAULT;

	return ret;
}



#ifdef CONFIG_MODULES

extern asmlinkage unsigned long sys_create_module(const char *name_user, size_t size);

asmlinkage unsigned long sys32_create_module(const char *name_user, __kernel_size_t32 size)
{
	
	PPCDBG(PPCDBG_SYS32M, "sys32_create_module - running - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return sys_create_module(name_user, (size_t)size);
}



extern asmlinkage long sys_init_module(const char *name_user, struct module *mod_user);

asmlinkage long sys32_init_module(const char *name_user, struct module *mod_user)
{
	
	PPCDBG(PPCDBG_SYS32, "sys32_init_module - running - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return sys_init_module(name_user, mod_user);
}



extern asmlinkage long sys_delete_module(const char *name_user);

asmlinkage long sys32_delete_module(const char *name_user)
{
	
	PPCDBG(PPCDBG_SYS32, "sys32_delete_module - running - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return sys_delete_module(name_user);
}



struct module_info32 {
	u32 addr;
	u32 size;
	u32 flags;
	s32 usecount;
};

/* Query various bits about modules.  */

static inline long
get_mod_name(const char *user_name, char **buf)
{
	unsigned long page;
	long retval;

	if ((unsigned long)user_name >= TASK_SIZE
	    && !segment_eq(get_fs (), KERNEL_DS))
		return -EFAULT;

	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = strncpy_from_user((char *)page, user_name, PAGE_SIZE);
	if (retval > 0) {
		if (retval < PAGE_SIZE) {
			*buf = (char *)page;
			return retval;
		}
		retval = -ENAMETOOLONG;
	} else if (!retval)
		retval = -EINVAL;

	free_page(page);
	return retval;
}

static inline void
put_mod_name(char *buf)
{
	free_page((unsigned long)buf);
}

static __inline__ struct module *find_module(const char *name)
{
	struct module *mod;

	for (mod = module_list; mod ; mod = mod->next) {
		if (mod->flags & MOD_DELETED)
			continue;
		if (!strcmp(mod->name, name))
			break;
	}

	return mod;
}

static int
qm_modules(char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	struct module *mod;
	size_t nmod, space, len;

	nmod = space = 0;

	for (mod = module_list; mod->next != NULL; mod = mod->next, ++nmod) {
		len = strlen(mod->name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, mod->name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(nmod, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while ((mod = mod->next)->next != NULL)
		space += strlen(mod->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_deps(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	size_t i, space, len;

	if (mod->next == NULL)
		return -EINVAL;
	if (!MOD_CAN_QUERY(mod))
		return put_user(0, ret);

	space = 0;
	for (i = 0; i < mod->ndeps; ++i) {
		const char *dep_name = mod->deps[i].dep->name;

		len = strlen(dep_name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, dep_name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	return put_user(i, ret);

calc_space_needed:
	space += len;
	while (++i < mod->ndeps)
		space += strlen(mod->deps[i].dep->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_refs(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	size_t nrefs, space, len;
	struct module_ref *ref;

	if (mod->next == NULL)
		return -EINVAL;
	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = 0;
	for (nrefs = 0, ref = mod->refs; ref ; ++nrefs, ref = ref->next_ref) {
		const char *ref_name = ref->ref->name;

		len = strlen(ref_name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, ref_name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(nrefs, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while ((ref = ref->next_ref) != NULL)
		space += strlen(ref->ref->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static inline int
qm_symbols(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	size_t i, space, len;
	struct module_symbol *s;
	char *strings;
	unsigned *vals;

	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = mod->nsyms * 2*sizeof(u32);

	i = len = 0;
	s = mod->syms;

	if (space > bufsize)
		goto calc_space_needed;

	if (!access_ok(VERIFY_WRITE, buf, space))
		return -EFAULT;

	bufsize -= space;
	vals = (unsigned *)buf;
	strings = buf+space;

	for (; i < mod->nsyms ; ++i, ++s, vals += 2) {
		len = strlen(s->name)+1;
		if (len > bufsize)
			goto calc_space_needed;

		if (copy_to_user(strings, s->name, len)
		    || __put_user(s->value, vals+0)
		    || __put_user(space, vals+1))
			return -EFAULT;

		strings += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(i, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	for (; i < mod->nsyms; ++i, ++s)
		space += strlen(s->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static inline int
qm_info(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	int error = 0;

	if (mod->next == NULL)
		return -EINVAL;

	if (sizeof(struct module_info32) <= bufsize) {
		struct module_info32 info;
		info.addr = (unsigned long)mod;
		info.size = mod->size;
		info.flags = mod->flags;
		info.usecount =
			((mod_member_present(mod, can_unload)
			  && mod->can_unload)
			 ? -1 : atomic_read(&mod->uc.usecount));

		if (copy_to_user(buf, &info, sizeof(struct module_info32)))
			return -EFAULT;
	} else
		error = -ENOSPC;

	if (put_user(sizeof(struct module_info32), ret))
		return -EFAULT;

	return error;
}

/* Note: it is necessary to treat which as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_query_module(char *name_user, u32 which, char *buf, __kernel_size_t32 bufsize, u32 ret)
{
	struct module *mod;
	int err;
	
	PPCDBG(PPCDBG_SYS32M, "sys32_query_module - entered - pid=%ld current=%lx comm=%s\n",
		    current->pid, current, current->comm);

	lock_kernel();
	if (name_user == 0) {
		/* This finds "kernel_module" which is not exported. */
		for(mod = module_list; mod->next != NULL; mod = mod->next)
			;
	} else {
		long namelen;
		char *name;

		if ((namelen = get_mod_name(name_user, &name)) < 0) {
			err = namelen;
			goto out;
		}
		err = -ENOENT;
		if (namelen == 0) {
			/* This finds "kernel_module" which is not exported. */
			for(mod = module_list; mod->next != NULL; mod = mod->next)
				;
		} else if ((mod = find_module(name)) == NULL) {
			put_mod_name(name);
			goto out;
		}
		put_mod_name(name);
	}

	switch ((int)which)
	{
	case 0:
		err = 0;
		break;
	case QM_MODULES:
		err = qm_modules(buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_DEPS:
		err = qm_deps(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_REFS:
		err = qm_refs(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_SYMBOLS:
		err = qm_symbols(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_INFO:
		err = qm_info(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	default:
		err = -EINVAL;
		break;
	}
out:
	unlock_kernel();
	
	PPCDBG(PPCDBG_SYS32, "sys32_query_module - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return err;
}



struct kernel_sym32 {
	u32 value;
	char name[60];
};
		 
extern asmlinkage long sys_get_kernel_syms(struct kernel_sym *table);

asmlinkage long sys32_get_kernel_syms(struct kernel_sym32 *table)
{
	int len, i;
	struct kernel_sym *tbl;
	mm_segment_t old_fs;
	
	PPCDBG(PPCDBG_SYS32, "sys32_get_kernel_syms - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	
	len = sys_get_kernel_syms(NULL);
	if (!table) return len;
	tbl = kmalloc (len * sizeof (struct kernel_sym), GFP_KERNEL);
	if (!tbl) return -ENOMEM;
	old_fs = get_fs();
	set_fs (KERNEL_DS);
	sys_get_kernel_syms(tbl);
	set_fs (old_fs);
	for (i = 0; i < len; i++, table += sizeof (struct kernel_sym32)) {
		if (put_user (tbl[i].value, &table->value) ||
		    copy_to_user (table->name, tbl[i].name, 60))
			break;
	}
	kfree (tbl);
	
	PPCDBG(PPCDBG_SYS32, "sys32_get_kernel_syms - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return i;
}

#else /* CONFIG_MODULES */

asmlinkage unsigned long sys32_create_module(const char *name_user, size_t size)
{
	
	PPCDBG(PPCDBG_SYS32, "sys32_create_module - running - pid=%ld, comm=%s\n", current->pid, current->comm);

	return -ENOSYS;
}

asmlinkage long sys32_init_module(const char *name_user, struct module *mod_user)
{
	PPCDBG(PPCDBG_SYS32, "sys32_init_module - running - pid=%ld, comm=%s\n", current->pid, current->comm);

	return -ENOSYS;
}

asmlinkage long sys32_delete_module(const char *name_user)
{
	PPCDBG(PPCDBG_SYS32, "sys32_delete_module - running - pid=%ld, comm=%s\n", current->pid, current->comm);

	return -ENOSYS;
}

/* Note: it is necessary to treat which as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_query_module(const char *name_user, u32 which, char *buf, size_t bufsize, size_t *ret)
{
	PPCDBG(PPCDBG_SYS32, "sys32_query_module - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	/* Let the program know about the new interface.  Not that it'll do them much good. */
	if ((int)which == 0)
		return 0;

	PPCDBG(PPCDBG_SYS32, "sys32_query_module - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
	return -ENOSYS;
}

asmlinkage long sys32_get_kernel_syms(struct kernel_sym *table)
{
	PPCDBG(PPCDBG_SYS32, "sys32_get_kernel_syms - running - pid=%ld, comm=%s\n", current->pid, current->comm);

	return -ENOSYS;
}

#endif  /* CONFIG_MODULES */



/* Stuff for NFS server syscalls... */
struct nfsctl_svc32 {
	u16			svc32_port;
	s32			svc32_nthreads;
};

struct nfsctl_client32 {
	s8			cl32_ident[NFSCLNT_IDMAX+1];
	s32			cl32_naddr;
	struct in_addr		cl32_addrlist[NFSCLNT_ADDRMAX];
	s32			cl32_fhkeytype;
	s32			cl32_fhkeylen;
	u8			cl32_fhkey[NFSCLNT_KEYMAX];
};

struct nfsctl_export32 {
	s8			ex32_client[NFSCLNT_IDMAX+1];
	s8			ex32_path[NFS_MAXPATHLEN+1];
	__kernel_dev_t32	ex32_dev;
	__kernel_ino_t32	ex32_ino;
	s32			ex32_flags;
	__kernel_uid_t32	ex32_anon_uid;
	__kernel_gid_t32	ex32_anon_gid;
};

struct nfsctl_uidmap32 {
	u32			ug32_ident;   /* char * */
	__kernel_uid_t32	ug32_uidbase;
	s32			ug32_uidlen;
	u32			ug32_udimap;  /* uid_t * */
	__kernel_uid_t32	ug32_gidbase;
	s32			ug32_gidlen;
	u32			ug32_gdimap;  /* gid_t * */
};

struct nfsctl_fhparm32 {
	struct sockaddr		gf32_addr;
	__kernel_dev_t32	gf32_dev;
	__kernel_ino_t32	gf32_ino;
	s32			gf32_version;
};

struct nfsctl_fdparm32 {
	struct sockaddr		gd32_addr;
	s8			gd32_path[NFS_MAXPATHLEN+1];
	s32			gd32_version;
};

struct nfsctl_fsparm32 {
	struct sockaddr		gd32_addr;
	s8			gd32_path[NFS_MAXPATHLEN+1];
	s32			gd32_maxlen;
};

struct nfsctl_arg32 {
	s32			ca32_version;	/* safeguard */
	union {
		struct nfsctl_svc32	u32_svc;
		struct nfsctl_client32	u32_client;
		struct nfsctl_export32	u32_export;
		struct nfsctl_uidmap32	u32_umap;
		struct nfsctl_fhparm32	u32_getfh;
		struct nfsctl_fdparm32	u32_getfd;
		struct nfsctl_fsparm32	u32_getfs;
	} u;
#define ca32_svc	u.u32_svc
#define ca32_client	u.u32_client
#define ca32_export	u.u32_export
#define ca32_umap	u.u32_umap
#define ca32_getfh	u.u32_getfh
#define ca32_getfd	u.u32_getfd
#define ca32_getfs	u.u32_getfs
#define ca32_authd	u.u32_authd
};

union nfsctl_res32 {
	__u8			cr32_getfh[NFS_FHSIZE];
	struct knfsd_fh		cr32_getfs;
};

static int nfs_svc32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= __get_user(karg->ca_svc.svc_port, &arg32->ca32_svc.svc32_port);
	err |= __get_user(karg->ca_svc.svc_nthreads, &arg32->ca32_svc.svc32_nthreads);
	return err;
}

static int nfs_clnt32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_client.cl_ident[0],
			  &arg32->ca32_client.cl32_ident[0],
			  NFSCLNT_IDMAX);
	err |= __get_user(karg->ca_client.cl_naddr, &arg32->ca32_client.cl32_naddr);
	err |= copy_from_user(&karg->ca_client.cl_addrlist[0],
			  &arg32->ca32_client.cl32_addrlist[0],
			  (sizeof(struct in_addr) * NFSCLNT_ADDRMAX));
	err |= __get_user(karg->ca_client.cl_fhkeytype,
		      &arg32->ca32_client.cl32_fhkeytype);
	err |= __get_user(karg->ca_client.cl_fhkeylen,
		      &arg32->ca32_client.cl32_fhkeylen);
	err |= copy_from_user(&karg->ca_client.cl_fhkey[0],
			  &arg32->ca32_client.cl32_fhkey[0],
			  NFSCLNT_KEYMAX);

	if(err) return -EFAULT;
	return 0;
}

static int nfs_exp32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_export.ex_client[0],
			  &arg32->ca32_export.ex32_client[0],
			  NFSCLNT_IDMAX);
	err |= copy_from_user(&karg->ca_export.ex_path[0],
			  &arg32->ca32_export.ex32_path[0],
			  NFS_MAXPATHLEN);
	err |= __get_user(karg->ca_export.ex_dev,
		      &arg32->ca32_export.ex32_dev);
	err |= __get_user(karg->ca_export.ex_ino,
		      &arg32->ca32_export.ex32_ino);
	err |= __get_user(karg->ca_export.ex_flags,
		      &arg32->ca32_export.ex32_flags);
	err |= __get_user(karg->ca_export.ex_anon_uid,
		      &arg32->ca32_export.ex32_anon_uid);
	err |= __get_user(karg->ca_export.ex_anon_gid,
		      &arg32->ca32_export.ex32_anon_gid);
	karg->ca_export.ex_anon_uid = karg->ca_export.ex_anon_uid;
	karg->ca_export.ex_anon_gid = karg->ca_export.ex_anon_gid;

	if(err) return -EFAULT;
	return 0;
}

static int nfs_uud32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	u32 uaddr;
	int i;
	int err;

	memset(karg, 0, sizeof(*karg));
	if(__get_user(karg->ca_version, &arg32->ca32_version))
		return -EFAULT;
	karg->ca_umap.ug_ident = (char *)get_free_page(GFP_USER);
	if(!karg->ca_umap.ug_ident)
		return -ENOMEM;
	err = __get_user(uaddr, &arg32->ca32_umap.ug32_ident);
	if(strncpy_from_user(karg->ca_umap.ug_ident,
			     (char *)A(uaddr), PAGE_SIZE) <= 0)
		return -EFAULT;
	err |= __get_user(karg->ca_umap.ug_uidbase,
		      &arg32->ca32_umap.ug32_uidbase);
	err |= __get_user(karg->ca_umap.ug_uidlen,
		      &arg32->ca32_umap.ug32_uidlen);
	err |= __get_user(uaddr, &arg32->ca32_umap.ug32_udimap);
	if (err)
		return -EFAULT;
	karg->ca_umap.ug_udimap = kmalloc((sizeof(uid_t) * karg->ca_umap.ug_uidlen),
					  GFP_USER);
	if(!karg->ca_umap.ug_udimap)
		return -ENOMEM;
	for(i = 0; i < karg->ca_umap.ug_uidlen; i++)
		err |= __get_user(karg->ca_umap.ug_udimap[i],
			      &(((__kernel_uid_t32 *)A(uaddr))[i]));
	err |= __get_user(karg->ca_umap.ug_gidbase,
		      &arg32->ca32_umap.ug32_gidbase);
	err |= __get_user(karg->ca_umap.ug_uidlen,
		      &arg32->ca32_umap.ug32_gidlen);
	err |= __get_user(uaddr, &arg32->ca32_umap.ug32_gdimap);
	if (err)
		return -EFAULT;
	karg->ca_umap.ug_gdimap = kmalloc((sizeof(gid_t) * karg->ca_umap.ug_uidlen),
					  GFP_USER);
	if(!karg->ca_umap.ug_gdimap)
		return -ENOMEM;
	for(i = 0; i < karg->ca_umap.ug_gidlen; i++)
		err |= __get_user(karg->ca_umap.ug_gdimap[i],
			      &(((__kernel_gid_t32 *)A(uaddr))[i]));

	return err;
}

static int nfs_getfh32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_getfh.gf_addr,
			  &arg32->ca32_getfh.gf32_addr,
			  (sizeof(struct sockaddr)));
	err |= __get_user(karg->ca_getfh.gf_dev,
		      &arg32->ca32_getfh.gf32_dev);
	err |= __get_user(karg->ca_getfh.gf_ino,
		      &arg32->ca32_getfh.gf32_ino);
	err |= __get_user(karg->ca_getfh.gf_version,
		      &arg32->ca32_getfh.gf32_version);

	if(err) return -EFAULT;
	return 0;
}

static int nfs_getfd32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_getfd.gd_addr,
			  &arg32->ca32_getfd.gd32_addr,
			  (sizeof(struct sockaddr)));
	err |= copy_from_user(&karg->ca_getfd.gd_path,
			  &arg32->ca32_getfd.gd32_path,
			  (NFS_MAXPATHLEN+1));
	err |= __get_user(karg->ca_getfd.gd_version,
		      &arg32->ca32_getfd.gd32_version);

	if(err) return -EFAULT;
	return 0;
}

static int nfs_getfs32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_getfs.gd_addr,
			  &arg32->ca32_getfs.gd32_addr,
			  (sizeof(struct sockaddr)));
	err |= copy_from_user(&karg->ca_getfs.gd_path,
			  &arg32->ca32_getfs.gd32_path,
			  (NFS_MAXPATHLEN+1));
	err |= __get_user(karg->ca_getfs.gd_maxlen,
		      &arg32->ca32_getfs.gd32_maxlen);

	if(err) return -EFAULT;
	return 0;
}

/* This really doesn't need translations, we are only passing
 * back a union which contains opaque nfs file handle data.
 */
static int nfs_getfh32_res_trans(union nfsctl_res *kres, union nfsctl_res32 *res32)
{
	int err;

	err = copy_to_user(res32, kres, sizeof(*res32));

	if(err) return -EFAULT;
	return 0;
}

/* Note: it is necessary to treat cmd_parm as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
int asmlinkage sys32_nfsservctl(u32 cmd_parm, struct nfsctl_arg32 *arg32, union nfsctl_res32 *res32)
{
  int cmd = (int)cmd_parm;
	struct nfsctl_arg *karg = NULL;
	union nfsctl_res *kres = NULL;
	mm_segment_t oldfs;
	int err;

	karg = kmalloc(sizeof(*karg), GFP_USER);
	if(!karg)
		return -ENOMEM;
	if(res32) {
		kres = kmalloc(sizeof(*kres), GFP_USER);
		if(!kres) {
			kfree(karg);
			return -ENOMEM;
		}
	}
	switch(cmd) {
	case NFSCTL_SVC:
		err = nfs_svc32_trans(karg, arg32);
		break;
	case NFSCTL_ADDCLIENT:
		err = nfs_clnt32_trans(karg, arg32);
		break;
	case NFSCTL_DELCLIENT:
		err = nfs_clnt32_trans(karg, arg32);
		break;
	case NFSCTL_EXPORT:
	case NFSCTL_UNEXPORT:
		err = nfs_exp32_trans(karg, arg32);
		break;
	/* This one is unimplemented, be we're ready for it. */
	case NFSCTL_UGIDUPDATE:
		err = nfs_uud32_trans(karg, arg32);
		break;
	case NFSCTL_GETFH:
		err = nfs_getfh32_trans(karg, arg32);
		break;
	case NFSCTL_GETFD:
		err = nfs_getfd32_trans(karg, arg32);
		break;
	case NFSCTL_GETFS:
		err = nfs_getfs32_trans(karg, arg32);
		break;
	default:
		err = -EINVAL;
		break;
	}
	if(err)
		goto done;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_nfsservctl(cmd, karg, kres);
	set_fs(oldfs);

	if (err)
		goto done;

	if((cmd == NFSCTL_GETFH) ||
	   (cmd == NFSCTL_GETFD) ||
	   (cmd == NFSCTL_GETFS))
		err = nfs_getfh32_res_trans(kres, res32);

done:
	if(karg) {
		if(cmd == NFSCTL_UGIDUPDATE) {
			if(karg->ca_umap.ug_ident)
				kfree(karg->ca_umap.ug_ident);
			if(karg->ca_umap.ug_udimap)
				kfree(karg->ca_umap.ug_udimap);
			if(karg->ca_umap.ug_gdimap)
				kfree(karg->ca_umap.ug_gdimap);
		}
		kfree(karg);
	}
	if(kres)
		kfree(kres);
	return err;
}



struct timespec32 {
	s32    tv_sec;
	s32    tv_nsec;
};

extern asmlinkage long sys_nanosleep(struct timespec *rqtp, struct timespec *rmtp);

asmlinkage long sys32_nanosleep(struct timespec32 *rqtp, struct timespec32 *rmtp)
{
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs ();
	
	PPCDBG(PPCDBG_SYS32NI, "sys32_nanosleep - running - pid=%ld, comm=%s \n", current->pid, current->comm);
	
	if (get_user (t.tv_sec, &rqtp->tv_sec) ||
	    __get_user (t.tv_nsec, &rqtp->tv_nsec))
		return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_nanosleep(&t, rmtp ? &t : NULL);
	set_fs (old_fs);
	if (rmtp && ret == -EINTR) {
		if (__put_user (t.tv_sec, &rmtp->tv_sec) ||
	    	    __put_user (t.tv_nsec, &rmtp->tv_nsec))
			return -EFAULT;
	}
	
	return ret;
}




/* These are here just in case some old sparc32 binary calls it. */
asmlinkage long sys32_pause(void)
{
	
	PPCDBG(PPCDBG_SYS32, "sys32_pause - running - pid=%ld, comm=%s \n", current->pid, current->comm);

	current->state = TASK_INTERRUPTIBLE;
	schedule();
	
	return -ERESTARTNOHAND;
}



static inline long get_it32(struct itimerval *o, struct itimerval32 *i)
{
	return (!access_ok(VERIFY_READ, i, sizeof(*i)) ||
		(__get_user(o->it_interval.tv_sec, &i->it_interval.tv_sec) |
		 __get_user(o->it_interval.tv_usec, &i->it_interval.tv_usec) |
		 __get_user(o->it_value.tv_sec, &i->it_value.tv_sec) |
		 __get_user(o->it_value.tv_usec, &i->it_value.tv_usec)));
}

static inline long put_it32(struct itimerval32 *o, struct itimerval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->it_interval.tv_sec, &o->it_interval.tv_sec) |
		 __put_user(i->it_interval.tv_usec, &o->it_interval.tv_usec) |
		 __put_user(i->it_value.tv_sec, &o->it_value.tv_sec) |
		 __put_user(i->it_value.tv_usec, &o->it_value.tv_usec)));
}

static inline long get_tv32(struct timeval *o, struct timeval32 *i)
{
	return (!access_ok(VERIFY_READ, i, sizeof(*i)) ||
		(__get_user(o->tv_sec, &i->tv_sec) |
		 __get_user(o->tv_usec, &i->tv_usec)));
}

static inline long put_tv32(struct timeval32 *o, struct timeval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->tv_sec, &o->tv_sec) |
		 __put_user(i->tv_usec, &o->tv_usec)));
}




extern int do_getitimer(int which, struct itimerval *value);

/* Note: it is necessary to treat which as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_getitimer(u32 which, struct itimerval32 *it)
{
	struct itimerval kit;
	int error;
	
	PPCDBG(PPCDBG_SYS32, "sys32_getitimer - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	error = do_getitimer((int)which, &kit);
	if (!error && put_it32(it, &kit))
		error = -EFAULT;

	
	PPCDBG(PPCDBG_SYS32, "sys32_getitimer - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
	return error;
}



extern int do_setitimer(int which, struct itimerval *, struct itimerval *);

/* Note: it is necessary to treat which as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_setitimer(u32 which, struct itimerval32 *in, struct itimerval32 *out)
{
	struct itimerval kin, kout;
	int error;
	
	PPCDBG(PPCDBG_SYS32, "sys32_setitimer - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	if (in) {
		if (get_it32(&kin, in))
			return -EFAULT;
	} else
		memset(&kin, 0, sizeof(kin));

	error = do_setitimer((int)which, &kin, out ? &kout : NULL);
	if (error || !out)
		return error;
	if (put_it32(out, &kout))
		return -EFAULT;

	
	PPCDBG(PPCDBG_SYS32, "sys32_setitimer - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
	return 0;
}

#define RLIM_INFINITY32	0xffffffff
#define RESOURCE32(x) ((x > RLIM_INFINITY32) ? RLIM_INFINITY32 : x)

struct rlimit32 {
	u32	rlim_cur;
	u32	rlim_max;
};

extern asmlinkage long sys_getrlimit(unsigned int resource, struct rlimit *rlim);
asmlinkage long sys32_getrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	set_fs (KERNEL_DS);
	ret = sys_getrlimit(resource, &r);
	set_fs(old_fs);
	if (!ret) {
		ret = put_user(RESOURCE32(r.rlim_cur), &rlim->rlim_cur);
		ret |= __put_user(RESOURCE32(r.rlim_max), &rlim->rlim_max);
	}
	
	return ret;
}

/* Back compatibility for getrlimit. Needed for some apps. */
asmlinkage long sys32_old_getrlimit(unsigned int resource, struct rlimit32* rlim)
{
	struct rlimit   x;    // 64-bit version of the resource limits.
	struct rlimit32 x32;  // 32-bit version of the resource limits.
	long rc = 0;
	
	if (resource >= RLIM_NLIMITS) {
		PPCDBG(PPCDBG_SYS32, "sys32_old_getrlimit - specified resource is too large (%x) - pid=%ld, comm=%s\n", resource, current->pid, current->comm);
		return -EINVAL;
	}

	memcpy(&x, current->rlim+resource, sizeof(struct rlimit));

	if(x.rlim_cur > RLIM_INFINITY32)
		x32.rlim_cur = RLIM_INFINITY32;
	else
		x32.rlim_cur = x.rlim_cur;

	if(x.rlim_max > RLIM_INFINITY32)
		x32.rlim_max = RLIM_INFINITY32;
	else
		x32.rlim_max = x.rlim_max;

	rc = (copy_to_user(rlim, &x32, sizeof(x32))) ? (-EFAULT) : 0;
	if (rc == 0) {
		PPCDBG(PPCDBG_SYS32, "sys32_old_getrlimit - current=%x, maximum=%x - pid=%ld, comm=%s\n", x32.rlim_cur, x32.rlim_max, current->pid, current->comm);
	} else {
		PPCDBG(PPCDBG_SYS32, "sys32_old_getrlimit - unable to copy into user's storage - pid=%ld, comm=%s\n", current->pid, current->comm);
	}
	return rc;
}

extern asmlinkage long sys_setrlimit(unsigned int resource, struct rlimit *rlim);
asmlinkage long sys32_setrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	long ret;
	mm_segment_t old_fs = get_fs ();
	
	PPCDBG(PPCDBG_SYS32, "sys32_setrlimit - entered - resource=%x, rlim=%p - pid=%ld, comm=%s\n", resource, rlim, current->pid, current->comm);

	if (resource >= RLIM_NLIMITS) return -EINVAL;	
	if (get_user (r.rlim_cur, &rlim->rlim_cur) ||
	    __get_user (r.rlim_max, &rlim->rlim_max))
		return -EFAULT;
	if (r.rlim_cur >= RLIM_INFINITY32)
		r.rlim_cur = RLIM_INFINITY;
	if (r.rlim_max >= RLIM_INFINITY32)
		r.rlim_max = RLIM_INFINITY;
	set_fs (KERNEL_DS);
	ret = sys_setrlimit(resource, &r);
	set_fs (old_fs);
	
	PPCDBG(PPCDBG_SYS32, "sys32_setrlimit - exited w/ ret=%x - pid=%ld, comm=%s\n", ret, current->pid, current->comm);
	return ret;
}


struct rusage32 {
        struct timeval32 ru_utime;
        struct timeval32 ru_stime;
        s32    ru_maxrss;
        s32    ru_ixrss;
        s32    ru_idrss;
        s32    ru_isrss;
        s32    ru_minflt;
        s32    ru_majflt;
        s32    ru_nswap;
        s32    ru_inblock;
        s32    ru_oublock;
        s32    ru_msgsnd; 
        s32    ru_msgrcv; 
        s32    ru_nsignals;
        s32    ru_nvcsw;
        s32    ru_nivcsw;
};

static int put_rusage (struct rusage32 *ru, struct rusage *r)
{
	int err;
	
	err = put_user (r->ru_utime.tv_sec, &ru->ru_utime.tv_sec);
	err |= __put_user (r->ru_utime.tv_usec, &ru->ru_utime.tv_usec);
	err |= __put_user (r->ru_stime.tv_sec, &ru->ru_stime.tv_sec);
	err |= __put_user (r->ru_stime.tv_usec, &ru->ru_stime.tv_usec);
	err |= __put_user (r->ru_maxrss, &ru->ru_maxrss);
	err |= __put_user (r->ru_ixrss, &ru->ru_ixrss);
	err |= __put_user (r->ru_idrss, &ru->ru_idrss);
	err |= __put_user (r->ru_isrss, &ru->ru_isrss);
	err |= __put_user (r->ru_minflt, &ru->ru_minflt);
	err |= __put_user (r->ru_majflt, &ru->ru_majflt);
	err |= __put_user (r->ru_nswap, &ru->ru_nswap);
	err |= __put_user (r->ru_inblock, &ru->ru_inblock);
	err |= __put_user (r->ru_oublock, &ru->ru_oublock);
	err |= __put_user (r->ru_msgsnd, &ru->ru_msgsnd);
	err |= __put_user (r->ru_msgrcv, &ru->ru_msgrcv);
	err |= __put_user (r->ru_nsignals, &ru->ru_nsignals);
	err |= __put_user (r->ru_nvcsw, &ru->ru_nvcsw);
	err |= __put_user (r->ru_nivcsw, &ru->ru_nivcsw);
	return err;
}


extern asmlinkage long sys_getrusage(int who, struct rusage *ru);

/* Note: it is necessary to treat who as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_getrusage(u32 who, struct rusage32 *ru)
{
	struct rusage r;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	PPCDBG(PPCDBG_SYS32X, "sys32_getrusage - running - pid=%ld, comm=%s\n", current->pid, current->comm);
		
	set_fs (KERNEL_DS);
	ret = sys_getrusage((int)who, &r);
	set_fs (old_fs);
	if (put_rusage (ru, &r)) 
		return -EFAULT;

	return ret;
}




struct sysinfo32 {
        s32 uptime;
        u32 loads[3];
        u32 totalram;
        u32 freeram;
        u32 sharedram;
        u32 bufferram;
        u32 totalswap;
        u32 freeswap;
        unsigned short procs;
        unsigned short pad;
        u32 totalhigh;
        u32 freehigh;
        u32 mem_unit;
        char _f[20-2*sizeof(u32)-sizeof(int)];
};

extern asmlinkage long sys_sysinfo(struct sysinfo *info);

asmlinkage long sys32_sysinfo(struct sysinfo32 *info)
{
	struct sysinfo s;
	int ret, err;
	int bitcount = 0;
	mm_segment_t old_fs = get_fs ();
	
	PPCDBG(PPCDBG_SYS32, "sys32_sysinfo - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	
	set_fs (KERNEL_DS);
	ret = sys_sysinfo(&s);
	set_fs (old_fs);
	/* Check to see if any memory value is too large for 32-bit and scale
	 *  down if needed
	 */
	if ((s.totalram >> 32) || (s.totalswap >> 32)) {
		while (s.mem_unit < PAGE_SIZE) {
			s.mem_unit <<= 1;
			bitcount++;
		}
		s.totalram >>= bitcount;
		s.freeram >>= bitcount;
		s.sharedram >>= bitcount;
		s.bufferram >>= bitcount;
		s.totalswap >>= bitcount;
		s.freeswap >>= bitcount;
		s.totalhigh >>= bitcount;
		s.freehigh >>= bitcount;
	}
	
	err = put_user (s.uptime, &info->uptime);
	err |= __put_user (s.loads[0], &info->loads[0]);
	err |= __put_user (s.loads[1], &info->loads[1]);
	err |= __put_user (s.loads[2], &info->loads[2]);
	err |= __put_user (s.totalram, &info->totalram);
	err |= __put_user (s.freeram, &info->freeram);
	err |= __put_user (s.sharedram, &info->sharedram);
	err |= __put_user (s.bufferram, &info->bufferram);
	err |= __put_user (s.totalswap, &info->totalswap);
	err |= __put_user (s.freeswap, &info->freeswap);
	err |= __put_user (s.procs, &info->procs);
	err |= __put_user (s.totalhigh, &info->totalhigh);
	err |= __put_user (s.freehigh, &info->freehigh);
	err |= __put_user (s.mem_unit, &info->mem_unit);
	
	if (err)
		return -EFAULT;
	
	PPCDBG(PPCDBG_SYS32, "sys32_sysinfo - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return ret;
}




/* Translations due to time_t size differences.  Which affects all
   sorts of things, like timeval and itimerval.  */
extern struct timezone sys_tz;
extern int do_sys_settimeofday(struct timeval *tv, struct timezone *tz);

asmlinkage long sys32_gettimeofday(struct timeval32 *tv, struct timezone *tz)
{
	
	PPCDBG(PPCDBG_SYS32X, "sys32_gettimeofday - running - pid=%ld, comm=%s\n", current->pid, current->comm);

	if (tv) {
		struct timeval ktv;
		do_gettimeofday(&ktv);
		if (put_tv32(tv, &ktv))
			return -EFAULT;
	}
	if (tz) {
		if (copy_to_user(tz, &sys_tz, sizeof(sys_tz)))
			return -EFAULT;
	}
	
	return 0;
}



asmlinkage long sys32_settimeofday(struct timeval32 *tv, struct timezone *tz)
{
	struct timeval ktv;
	struct timezone ktz;
	
	PPCDBG(PPCDBG_SYS32, "sys32_settimeofday - running - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

 	if (tv) {
		if (get_tv32(&ktv, tv))
			return -EFAULT;
	}
	if (tz) {
		if (copy_from_user(&ktz, tz, sizeof(ktz)))
			return -EFAULT;
	}

	return do_sys_settimeofday(tv ? &ktv : NULL, tz ? &ktz : NULL);
}




struct tms32 {
	__kernel_clock_t32 tms_utime;
	__kernel_clock_t32 tms_stime;
	__kernel_clock_t32 tms_cutime;
	__kernel_clock_t32 tms_cstime;
};
                                
extern asmlinkage long sys_times(struct tms * tbuf);

asmlinkage long sys32_times(struct tms32 *tbuf)
{
	struct tms t;
	long ret;
	mm_segment_t old_fs = get_fs ();
	int err;
	
	PPCDBG(PPCDBG_SYS32, "sys32_times - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	
	set_fs (KERNEL_DS);
	ret = sys_times(tbuf ? &t : NULL);
	set_fs (old_fs);
	if (tbuf) {
		err = put_user (t.tms_utime, &tbuf->tms_utime);
		err |= __put_user (t.tms_stime, &tbuf->tms_stime);
		err |= __put_user (t.tms_cutime, &tbuf->tms_cutime);
		err |= __put_user (t.tms_cstime, &tbuf->tms_cstime);
		if (err)
			ret = -EFAULT;
	}
	
	PPCDBG(PPCDBG_SYS32, "sys32_times - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return ret;
}

struct msgbuf32 { s32 mtype; char mtext[1]; };

struct semid_ds32 {
	struct ipc_perm32 sem_perm;
	unsigned int _pad0;
	__kernel_time_t32 sem_otime;
	unsigned int _pad1;
	__kernel_time_t32 sem_ctime;
	u32 sem_base;
	u32 sem_pending;
	u32 sem_pending_last;
	u32 undo;
	unsigned short sem_nsems;
};

struct semid64_ds32 {
	struct ipc64_perm32 sem_perm;
	unsigned int __unused1;
	__kernel_time_t32 sem_otime;
	unsigned int __unused2;
	__kernel_time_t32 sem_ctime;
	u32 sem_nsems;
	u32 __unused3;
	u32 __unused4;
};

struct msqid_ds32
{
	struct ipc_perm32 msg_perm;
	u32 msg_first;
	u32 msg_last;
	__kernel_time_t32 msg_stime;
	__kernel_time_t32 msg_rtime;
	__kernel_time_t32 msg_ctime;
	u32 msg_lcbytes;
	u32 msg_lqbytes;
	unsigned short msg_cbytes;
	unsigned short msg_qnum;
	unsigned short msg_qbytes;
	__kernel_ipc_pid_t32 msg_lspid;
	__kernel_ipc_pid_t32 msg_lrpid;
};

struct msqid64_ds32 {
	struct ipc64_perm32 msg_perm;
	unsigned int __unused1;
	__kernel_time_t32 msg_stime;
	unsigned int __unused2;
	__kernel_time_t32 msg_rtime;
	unsigned int __unused3;
	__kernel_time_t32 msg_ctime;
	unsigned int msg_cbytes;
	unsigned int msg_qnum;
	unsigned int msg_qbytes;
	__kernel_pid_t32 msg_lspid;
	__kernel_pid_t32 msg_lrpid;
	unsigned int __unused4;
	unsigned int __unused5;
};

struct shmid_ds32 {
	struct ipc_perm32 shm_perm;
	int shm_segsz;
	__kernel_time_t32 shm_atime;
	__kernel_time_t32 shm_dtime;
	__kernel_time_t32 shm_ctime;
	__kernel_ipc_pid_t32 shm_cpid;
	__kernel_ipc_pid_t32 shm_lpid;
	unsigned short shm_nattch;
	unsigned short __unused;
	unsigned int __unused2;
	unsigned int __unused3;
};

struct shmid64_ds32 {
	struct ipc64_perm32 shm_perm;
	unsigned int __unused1;
	__kernel_time_t32 shm_atime;
	unsigned int __unused2;
	__kernel_time_t32 shm_dtime;
	unsigned int __unused3;
	__kernel_time_t32 shm_ctime;
	unsigned int __unused4;
	__kernel_size_t32 shm_segsz;
	__kernel_pid_t32 shm_cpid;
	__kernel_pid_t32 shm_lpid;
	unsigned int shm_nattch;
	unsigned int __unused5;
	unsigned int __unused6;
};

/*
 * sys32_ipc() is the de-multiplexer for the SysV IPC calls in 32bit
 * emulation..
 *
 * This is really horribly ugly.
 */
static long do_sys32_semctl(int first, int second, int third, void *uptr)
{
	union semun fourth;
	u32 pad;
	int err, err2;
	mm_segment_t old_fs;

	if (!uptr)
		return -EINVAL;
	err = -EFAULT;
	if (get_user(pad, (u32 *)uptr))
		return err;
	if ((third & (~IPC_64)) == SETVAL)
		fourth.val = (int)pad;
	else
		fourth.__pad = (void *)A(pad);
	switch (third & (~IPC_64)) {

	case IPC_INFO:
	case IPC_RMID:
	case SEM_INFO:
	case GETVAL:
	case GETPID:
	case GETNCNT:
	case GETZCNT:
	case GETALL:
	case SETALL:
	case SETVAL:
		err = sys_semctl(first, second, third, fourth);
		break;

	case IPC_STAT:
	case SEM_STAT:
		if (third & IPC_64) {
			struct semid64_ds s64;
			struct semid64_ds32 *usp;

			usp = (struct semid64_ds32 *)A(pad);
			fourth.__pad = &s64;
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_semctl(first, second, third, fourth);
			set_fs(old_fs);
			err2 = copy_to_user(&usp->sem_perm, &s64.sem_perm,
					    sizeof(struct ipc64_perm32));
			err2 |= __put_user(s64.sem_otime, &usp->sem_otime);
			err2 |= __put_user(s64.sem_ctime, &usp->sem_ctime);
			err2 |= __put_user(s64.sem_nsems, &usp->sem_nsems);
			if (err2)
				err = -EFAULT;
		} else {
			struct semid_ds s;
			struct semid_ds32 *usp;

			usp = (struct semid_ds32 *)A(pad);
			fourth.__pad = &s;
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_semctl(first, second, third, fourth);
			set_fs(old_fs);
			err2 = copy_to_user(&usp->sem_perm, &s.sem_perm,
					    sizeof(struct ipc_perm32));
			err2 |= __put_user(s.sem_otime, &usp->sem_otime);
			err2 |= __put_user(s.sem_ctime, &usp->sem_ctime);
			err2 |= __put_user(s.sem_nsems, &usp->sem_nsems);
			if (err2)
				err = -EFAULT;
		}
		break;
 
	case IPC_SET:
		if (third & IPC_64) {
			struct semid64_ds s64;
			struct semid64_ds32 *usp;

			usp = (struct semid64_ds32 *)A(pad);

			err = get_user(s64.sem_perm.uid, &usp->sem_perm.uid);
			err |= __get_user(s64.sem_perm.gid,
					  &usp->sem_perm.gid);
			err |= __get_user(s64.sem_perm.mode,
					  &usp->sem_perm.mode);
			if (err)
				goto out;
			fourth.__pad = &s64;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_semctl(first, second, third, fourth);
			set_fs(old_fs);

		} else {
			struct semid_ds s;
			struct semid_ds32 *usp;

			usp = (struct semid_ds32 *)A(pad);

			err = get_user(s.sem_perm.uid, &usp->sem_perm.uid);
			err |= __get_user(s.sem_perm.gid,
					  &usp->sem_perm.gid);
			err |= __get_user(s.sem_perm.mode,
					  &usp->sem_perm.mode);
			if (err)
				goto out;
			fourth.__pad = &s;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_semctl(first, second, third, fourth);
			set_fs(old_fs);
		}
		break;
	default:
		err = -EINVAL;
	}
out:
	return err;
}

#define MAXBUF (64*1024)

static int 
do_sys32_msgsnd(int first, int second, int third, void *uptr)
{
	struct msgbuf *p;
	struct msgbuf32 *up = (struct msgbuf32 *)uptr;
	mm_segment_t old_fs;
	int err;

	if (second < 0 || (second >= MAXBUF-sizeof(struct msgbuf)))
		return -EINVAL;

	p = kmalloc(second + sizeof(struct msgbuf), GFP_USER);
	if (!p)
		return -ENOMEM;
	err = get_user(p->mtype, &up->mtype);
	err |= copy_from_user(p->mtext, &up->mtext, second);
	if (err) {
		err = -EFAULT;
		goto out;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_msgsnd(first, p, second, third);
	set_fs(old_fs);
out:
	kfree(p);
	return err;
}

static int
do_sys32_msgrcv(int first, int second, int msgtyp, int third,
		int version, void *uptr)
{
	struct msgbuf32 *up;
	struct msgbuf *p;
	mm_segment_t old_fs;
	int err;

	if (second < 0 || (second >= MAXBUF-sizeof(struct msgbuf)))
		return -EINVAL;

	if (!version) {
		struct ipc_kludge_32 *uipck = (struct ipc_kludge_32 *)uptr;
		struct ipc_kludge_32 ipck;

		err = -EINVAL;
		if (!uptr)
			goto out;
		err = -EFAULT;
		if (copy_from_user(&ipck, uipck, sizeof(struct ipc_kludge_32)))
			goto out;
		uptr = (void *)A(ipck.msgp);
		msgtyp = ipck.msgtyp;
	}
	err = -ENOMEM;
	p = kmalloc(second + sizeof (struct msgbuf), GFP_USER);
	if (!p)
		goto out;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_msgrcv(first, p, second, msgtyp, third);
	set_fs(old_fs);
	if (err < 0)
		goto free_then_out;
	up = (struct msgbuf32 *)uptr;
	if (put_user(p->mtype, &up->mtype) ||
	    copy_to_user(&up->mtext, p->mtext, err))
		err = -EFAULT;
free_then_out:
	kfree(p);
out:
	return err;
}

static int
do_sys32_msgctl(int first, int second, void *uptr)
{
	int err = -EINVAL, err2;
	mm_segment_t old_fs;

	switch (second & (~IPC_64)) {

	case IPC_INFO:
	case IPC_RMID:
	case MSG_INFO:
		err = sys_msgctl(first, second, (struct msqid_ds *)uptr);
		break;

	case IPC_SET:
		if (second & IPC_64) {
			struct msqid64_ds m64;
			struct msqid64_ds32 *up = (struct msqid64_ds32 *)uptr;

			err2 = copy_from_user(&m64.msg_perm, &up->msg_perm,
					      sizeof(struct ipc64_perm32));
			err2 |= __get_user(m64.msg_qbytes, &up->msg_qbytes);
			if (err2) {
				err = -EFAULT;
				break;
			}
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_msgctl(first, second,
					 (struct msqid_ds *)&m64);
			set_fs(old_fs);
		} else {
			struct msqid_ds m;
			struct msqid_ds32 *up = (struct msqid_ds32 *)uptr;

			err2 = copy_from_user(&m.msg_perm, &up->msg_perm,
					      sizeof(struct ipc_perm32));
			err2 |= __get_user(m.msg_qbytes, &up->msg_qbytes);
			if (err2) {
				err = -EFAULT;
				break;
			}
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_msgctl(first, second, &m);
			set_fs(old_fs);
		}
		break;

	case IPC_STAT:
	case MSG_STAT:
		if (second & IPC_64) {
			struct msqid64_ds m64;
			struct msqid64_ds32 *up = (struct msqid64_ds32 *)uptr;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_msgctl(first, second,
					 (struct msqid_ds *)&m64);
			set_fs(old_fs);

			err2 = copy_to_user(&up->msg_perm, &m64.msg_perm,
					    sizeof(struct ipc64_perm32));
 			err2 |= __put_user(m64.msg_stime, &up->msg_stime);
			err2 |= __put_user(m64.msg_rtime, &up->msg_rtime);
			err2 |= __put_user(m64.msg_ctime, &up->msg_ctime);
			err2 |= __put_user(m64.msg_cbytes, &up->msg_cbytes);
			err2 |= __put_user(m64.msg_qnum, &up->msg_qnum);
			err2 |= __put_user(m64.msg_qbytes, &up->msg_qbytes);
			err2 |= __put_user(m64.msg_lspid, &up->msg_lspid);
			err2 |= __put_user(m64.msg_lrpid, &up->msg_lrpid);
			if (err2)
				err = -EFAULT;
		} else {
			struct msqid64_ds m;
			struct msqid_ds32 *up = (struct msqid_ds32 *)uptr;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_msgctl(first, second, (struct msqid_ds *)&m);
			set_fs(old_fs);

			err2 = copy_to_user(&up->msg_perm, &m.msg_perm,
					    sizeof(struct ipc_perm32));
 			err2 |= __put_user(m.msg_stime, &up->msg_stime);
			err2 |= __put_user(m.msg_rtime, &up->msg_rtime);
			err2 |= __put_user(m.msg_ctime, &up->msg_ctime);
			err2 |= __put_user(m.msg_cbytes, &up->msg_cbytes);
			err2 |= __put_user(m.msg_qnum, &up->msg_qnum);
			err2 |= __put_user(m.msg_qbytes, &up->msg_qbytes);
			err2 |= __put_user(m.msg_lspid, &up->msg_lspid);
			err2 |= __put_user(m.msg_lrpid, &up->msg_lrpid);
			if (err2)
				err = -EFAULT;
		}
		break;
	}
	return err;
}

static int
do_sys32_shmat(int first, int second, int third, int version, void *uptr)
{
	unsigned long raddr;
	u32 *uaddr = (u32 *)A((u32)third);
	int err = -EINVAL;

	if (version == 1)
		return err;
	err = sys_shmat(first, uptr, second, &raddr);
	if (err)
		return err;
	err = put_user(raddr, uaddr);
	return err;
}

static int
do_sys32_shmctl(int first, int second, void *uptr)
{
	int err = -EINVAL, err2;
	mm_segment_t old_fs;

	switch (second & (~IPC_64)) {

	case IPC_INFO:
	case IPC_RMID:
	case SHM_LOCK:
	case SHM_UNLOCK:
		err = sys_shmctl(first, second, (struct shmid_ds *)uptr);
		break;
	case IPC_SET:
		if (second & IPC_64) {
			struct shmid64_ds32 *up = (struct shmid64_ds32 *)uptr;
			struct shmid64_ds s64;

			err = get_user(s64.shm_perm.uid, &up->shm_perm.uid);
			err |= __get_user(s64.shm_perm.gid, &up->shm_perm.gid);
			err |= __get_user(s64.shm_perm.mode,
					  &up->shm_perm.mode);
			if (err)
				break;
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_shmctl(first, second,
					 (struct shmid_ds *)&s64);
			set_fs(old_fs);
		} else {
			struct shmid_ds32 *up = (struct shmid_ds32 *)uptr;
			struct shmid_ds s;

			err = get_user(s.shm_perm.uid, &up->shm_perm.uid);
			err |= __get_user(s.shm_perm.gid, &up->shm_perm.gid);
			err |= __get_user(s.shm_perm.mode, &up->shm_perm.mode);
			if (err)
				break;
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_shmctl(first, second, &s);
			set_fs(old_fs);
		}
		break;

	case IPC_STAT:
	case SHM_STAT:
		if (second & IPC_64) {
			struct shmid64_ds32 *up = (struct shmid64_ds32 *)uptr;
			struct shmid64_ds s64;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_shmctl(first, second,
					 (struct shmid_ds *)&s64);
			set_fs(old_fs);
			if (err < 0)
				break;

			err2 = copy_to_user(&up->shm_perm, &s64.shm_perm,
					    sizeof(struct ipc64_perm32));
			err2 |= __put_user(s64.shm_atime, &up->shm_atime);
			err2 |= __put_user(s64.shm_dtime, &up->shm_dtime);
			err2 |= __put_user(s64.shm_ctime, &up->shm_ctime);
			err2 |= __put_user(s64.shm_segsz, &up->shm_segsz);
			err2 |= __put_user(s64.shm_nattch, &up->shm_nattch);
			err2 |= __put_user(s64.shm_cpid, &up->shm_cpid);
			err2 |= __put_user(s64.shm_lpid, &up->shm_lpid);
			if (err2)
				err = -EFAULT;
		} else {
			struct shmid_ds32 *up = (struct shmid_ds32 *)uptr;
			struct shmid_ds s;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			err = sys_shmctl(first, second, &s);
			set_fs(old_fs);
			if (err < 0)
				break;

			err2 = copy_to_user(&up->shm_perm, &s.shm_perm,
					    sizeof(struct ipc_perm32));
			err2 |= __put_user (s.shm_atime, &up->shm_atime);
			err2 |= __put_user (s.shm_dtime, &up->shm_dtime);
			err2 |= __put_user (s.shm_ctime, &up->shm_ctime);
			err2 |= __put_user (s.shm_segsz, &up->shm_segsz);
			err2 |= __put_user (s.shm_nattch, &up->shm_nattch);
			err2 |= __put_user (s.shm_cpid, &up->shm_cpid);
			err2 |= __put_user (s.shm_lpid, &up->shm_lpid);
			if (err2)
				err = -EFAULT;
		}
		break;

	case SHM_INFO: {
		struct shm_info si;
		struct shm_info32 {
			int used_ids;
			u32 shm_tot, shm_rss, shm_swp;
			u32 swap_attempts, swap_successes;
		} *uip = (struct shm_info32 *)uptr;

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_shmctl(first, second, (struct shmid_ds *)&si);
		set_fs(old_fs);
		if (err < 0)
			break;
		err2 = put_user(si.used_ids, &uip->used_ids);
		err2 |= __put_user(si.shm_tot, &uip->shm_tot);
		err2 |= __put_user(si.shm_rss, &uip->shm_rss);
		err2 |= __put_user(si.shm_swp, &uip->shm_swp);
		err2 |= __put_user(si.swap_attempts, &uip->swap_attempts);
		err2 |= __put_user(si.swap_successes, &uip->swap_successes);
		if (err2)
			err = -EFAULT;
		break;
	}
	}
	return err;
}

/*
 * Note: it is necessary to treat first_parm, second_parm, and
 * third_parm as unsigned ints, with the corresponding cast to a
 * signed int to insure that the proper conversion (sign extension)
 * between the register representation of a signed int (msr in 32-bit
 * mode) and the register representation of a signed int (msr in
 * 64-bit mode) is performed.
 */
asmlinkage long sys32_ipc(u32 call, u32 first_parm, u32 second_parm, u32 third_parm, u32 ptr, u32 fifth)
{
	int first  = (int)first_parm;
	int second = (int)second_parm;
	int third  = (int)third_parm;
	int version, err;
	
	PPCDBG(PPCDBG_SYS32, "sys32_ipc - entered - call=%x, parm1=%x, parm2=%x, parm3=%x, parm4=%x, parm5=%x \n", 
         call, first_parm, second_parm, third_parm, ptr, fifth);

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	switch (call) {

	case SEMOP:
		/* struct sembuf is the same on 32 and 64bit :)) */
		err = sys_semop(first, (struct sembuf *)AA(ptr),
				second);
		break;
	case SEMGET:
		err = sys_semget(first, second, third);
		break;
	case SEMCTL:
		err = do_sys32_semctl(first, second, third,
				      (void *)AA(ptr));
		break;

	case MSGSND:
		err = do_sys32_msgsnd(first, second, third,
				      (void *)AA(ptr));
		break;
	case MSGRCV:
		err = do_sys32_msgrcv(first, second, fifth, third,
				      version, (void *)AA(ptr));
		break;
	case MSGGET:
		err = sys_msgget((key_t)first, second);
		break;
	case MSGCTL:
		err = do_sys32_msgctl(first, second, (void *)AA(ptr));
		break;

	case SHMAT:
		err = do_sys32_shmat(first, second, third,
				     version, (void *)AA(ptr));
		break;
	case SHMDT: 
		err = sys_shmdt((char *)AA(ptr));
		break;
	case SHMGET:
		err = sys_shmget(first, second, third);
		break;
	case SHMCTL:
		err = do_sys32_shmctl(first, second, (void *)AA(ptr));
		break;
	default:
		err = -ENOSYS;
		break;
	}

	
	PPCDBG(PPCDBG_SYS32, "sys32_ipc - exited w/ %d/0x%x \n", err, err);
	return err;
}

/* stat syscall methods. */
extern asmlinkage int sys_stat(char* filename, struct __old_kernel_stat* statbuf);

static int cp_old_stat32(struct inode* inode, struct __old_kernel_stat32* statbuf)
{
	static int warncount = 5;
	struct __old_kernel_stat32 tmp;

	if (warncount) {
		warncount--;
		printk("VFS: Warning: %s using old stat() call. Recompile your binary.\n",
			current->comm);
	}

	tmp.st_dev = kdev_t_to_nr(inode->i_dev);
	tmp.st_ino = inode->i_ino;
	tmp.st_mode = inode->i_mode;
	tmp.st_nlink = inode->i_nlink;
	SET_OLDSTAT_UID(tmp, inode->i_uid);
	SET_OLDSTAT_GID(tmp, inode->i_gid);
	tmp.st_rdev = kdev_t_to_nr(inode->i_rdev);
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;
	return copy_to_user(statbuf,&tmp,sizeof(tmp)) ? -EFAULT : 0;
}

asmlinkage long sys32_stat(char* filename, struct __old_kernel_stat32* statbuf)
{
	struct nameidata nd;
	int error;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_stat - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	error = user_path_walk(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_old_stat32(nd.dentry->d_inode, statbuf);
		path_release(&nd);
	}
	
	PPCDBG(PPCDBG_SYS32X, "sys32_stat - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return error;
}

asmlinkage long sys32_fstat(unsigned int fd, struct __old_kernel_stat32* statbuf)
{
	struct file *f;
	int err = -EBADF;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_fstat - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	f = fget(fd);
	if (f) {
		struct dentry * dentry = f->f_dentry;

		err = do_revalidate(dentry);
		if (!err)
			err = cp_old_stat32(dentry->d_inode, statbuf);
		fput(f);
	}
	
	PPCDBG(PPCDBG_SYS32X, "sys32_fstat - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return err;
}

asmlinkage long sys32_lstat(char* filename, struct __old_kernel_stat32* statbuf)
{
	struct nameidata nd;
	int error;
	
	PPCDBG(PPCDBG_SYS32X, "sys32_lstat - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	error = user_path_walk_link(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_old_stat32(nd.dentry->d_inode, statbuf);

		path_release(&nd);
	}
	
	PPCDBG(PPCDBG_SYS32X, "sys32_lstat - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return error;
}

extern asmlinkage ssize_t sys_sendfile(int out_fd, int in_fd, off_t* offset, size_t count);

/* Note: it is necessary to treat out_fd and in_fd as unsigned ints, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sendfile(u32 out_fd, u32 in_fd, __kernel_off_t32* offset, u32 count)
{
	mm_segment_t old_fs = get_fs();
	int ret;
	off_t of;
	
	if (offset && get_user(of, offset))
		return -EFAULT;
		
	set_fs(KERNEL_DS);
	ret = sys_sendfile((int)out_fd, (int)in_fd, offset ? &of : NULL, count);
	set_fs(old_fs);
	
	if (offset && put_user(of, offset))
		return -EFAULT;
		
	return ret;
}

extern asmlinkage int sys_setsockopt(int fd, int level, int optname, char *optval, int optlen);

asmlinkage long sys32_setsockopt(int fd, int level, int optname, char* optval, int optlen)
{
	
	PPCDBG(PPCDBG_SYS32,"sys32_setsockopt - running - pid=%ld, comm=%s\n", current->pid, current->comm);

	if (optname == SO_ATTACH_FILTER) {
		struct sock_fprog32 {
			__u16 len;
			__u32 filter;
		} *fprog32 = (struct sock_fprog32 *)optval;
		struct sock_fprog kfprog;
		struct sock_filter *kfilter;
		unsigned int fsize;
		mm_segment_t old_fs;
		__u32 uptr;
		int ret;

		if (get_user(kfprog.len, &fprog32->len) ||
		    __get_user(uptr, &fprog32->filter))
			return -EFAULT;
		kfprog.filter = (struct sock_filter *)A(uptr);
		fsize = kfprog.len * sizeof(struct sock_filter);
		kfilter = (struct sock_filter *)kmalloc(fsize, GFP_KERNEL);
		if (kfilter == NULL)
			return -ENOMEM;
		if (copy_from_user(kfilter, kfprog.filter, fsize)) {
			kfree(kfilter);
			return -EFAULT;
		}
		kfprog.filter = kfilter;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = sys_setsockopt(fd, level, optname,
				     (char *)&kfprog, sizeof(kfprog));
		set_fs(old_fs);
		kfree(kfilter);
		return ret;
	}
	return sys_setsockopt(fd, level, optname, optval, optlen);
}




#define MAX_SOCK_ADDR	128		/* 108 for Unix domain -  16 for IP, 16 for IPX, 24 for IPv6, about 80 for AX.25 */
#define __CMSG32_NXTHDR(ctl, len, cmsg, cmsglen) __cmsg32_nxthdr((ctl),(len),(cmsg),(cmsglen))
#define CMSG32_NXTHDR(mhdr, cmsg, cmsglen) cmsg32_nxthdr((mhdr), (cmsg), (cmsglen))

#define CMSG32_ALIGN(len) ( ((len)+sizeof(int)-1) & ~(sizeof(int)-1) )

#define CMSG32_DATA(cmsg)	((void *)((char *)(cmsg) + CMSG32_ALIGN(sizeof(struct cmsghdr32))))
#define CMSG32_SPACE(len) (CMSG32_ALIGN(sizeof(struct cmsghdr32)) + CMSG32_ALIGN(len))
#define CMSG32_LEN(len) (CMSG32_ALIGN(sizeof(struct cmsghdr32)) + (len))
#define __CMSG32_FIRSTHDR(ctl,len) ((len) >= sizeof(struct cmsghdr32) ? \
				    (struct cmsghdr32 *)(ctl) : \
				    (struct cmsghdr32 *)NULL)
#define CMSG32_FIRSTHDR(msg)	__CMSG32_FIRSTHDR((msg)->msg_control, (msg)->msg_controllen)

struct msghdr32
{
	u32               msg_name;
	int               msg_namelen;
	u32               msg_iov;
	__kernel_size_t32 msg_iovlen;
	u32               msg_control;
	__kernel_size_t32 msg_controllen;
	unsigned          msg_flags;
};

struct cmsghdr32
{
	__kernel_size_t32 cmsg_len;
	int               cmsg_level;
	int               cmsg_type;
};

__inline__ struct cmsghdr32 *__cmsg32_nxthdr(void *__ctl, __kernel_size_t __size,
					     struct cmsghdr32 *__cmsg, int __cmsg_len)
{
	struct cmsghdr32 * __ptr;

	__ptr = (struct cmsghdr32 *)(((unsigned char *) __cmsg) +
				     CMSG32_ALIGN(__cmsg_len));
	if ((unsigned long)((char*)(__ptr+1) - (char *) __ctl) > __size)
		return NULL;

	return __ptr;
}

__inline__ struct cmsghdr32 *cmsg32_nxthdr (struct msghdr *__msg,
					    struct cmsghdr32 *__cmsg,
					    int __cmsg_len)
{
	return __cmsg32_nxthdr(__msg->msg_control, __msg->msg_controllen,
			       __cmsg, __cmsg_len);
}

extern __inline__ struct socket *socki_lookup(struct inode *inode)
{
	return &inode->u.socket_i;
}

extern __inline__ struct socket *sockfd_lookup(int fd, int *err)
{
	struct file *file;
	struct inode *inode;

	if (!(file = fget(fd)))
	{
		*err = -EBADF;
		return NULL;
	}

	inode = file->f_dentry->d_inode;
	if (!inode || !inode->i_sock || !socki_lookup(inode))
	{
		*err = -ENOTSOCK;
		fput(file);
		return NULL;
	}

	return socki_lookup(inode);
}

extern __inline__ void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

static inline int msghdr_from_user32_to_kern(struct msghdr *kmsg, struct msghdr32 *umsg)
{
	u32 tmp1, tmp2, tmp3;
	int err;

	err = get_user(tmp1, &umsg->msg_name);
	err |= __get_user(tmp2, &umsg->msg_iov);
	err |= __get_user(tmp3, &umsg->msg_control);
	if (err)
		return -EFAULT;

	kmsg->msg_name = (void *)A(tmp1);
	kmsg->msg_iov = (struct iovec *)A(tmp2);
	kmsg->msg_control = (void *)A(tmp3);

	err = get_user(kmsg->msg_namelen, &umsg->msg_namelen);
	err |= get_user(kmsg->msg_iovlen, &umsg->msg_iovlen);
	err |= get_user(kmsg->msg_controllen, &umsg->msg_controllen);
	err |= get_user(kmsg->msg_flags, &umsg->msg_flags);
	
	return err;
}

static inline int iov_from_user32_to_kern(struct iovec *kiov,
					  struct iovec32 *uiov32,
					  int niov)
{
	int tot_len = 0;

	while(niov > 0) {
		u32 len, buf;

		if(get_user(len, &uiov32->iov_len) ||
		   get_user(buf, &uiov32->iov_base)) {
			tot_len = -EFAULT;
			break;
		}
		tot_len += len;
		kiov->iov_base = (void *)A(buf);
		kiov->iov_len = (__kernel_size_t) len;
		uiov32++;
		kiov++;
		niov--;
	}
	return tot_len;
}

/* I've named the args so it is easy to tell whose space the pointers are in. */
static int verify_iovec32(struct msghdr *kern_msg, struct iovec *kern_iov,
			  char *kern_address, int mode)
{
	int tot_len;

	if(kern_msg->msg_namelen) {
		if(mode==VERIFY_READ) {
			int err = move_addr_to_kernel(kern_msg->msg_name,
						      kern_msg->msg_namelen,
						      kern_address);
			if(err < 0)
				return err;
		}
		kern_msg->msg_name = kern_address;
	} else
		kern_msg->msg_name = NULL;

	if(kern_msg->msg_iovlen > UIO_FASTIOV) {
		kern_iov = kmalloc(kern_msg->msg_iovlen * sizeof(struct iovec),
				   GFP_KERNEL);
		if(!kern_iov)
			return -ENOMEM;
	}

	tot_len = iov_from_user32_to_kern(kern_iov,
					  (struct iovec32 *)kern_msg->msg_iov,
					  kern_msg->msg_iovlen);
	if(tot_len >= 0)
		kern_msg->msg_iov = kern_iov;
	else if(kern_msg->msg_iovlen > UIO_FASTIOV)
		kfree(kern_iov);

	return tot_len;
}

/* There is a lot of hair here because the alignment rules (and
 * thus placement) of cmsg headers and length are different for
 * 32-bit apps.  -DaveM
 */
static int cmsghdr_from_user32_to_kern(struct msghdr *kmsg,
				       unsigned char *stackbuf, int stackbuf_size)
{
	struct cmsghdr32 *ucmsg;
	struct cmsghdr *kcmsg, *kcmsg_base;
	__kernel_size_t32 ucmlen;
	__kernel_size_t kcmlen, tmp;

	kcmlen = 0;
	kcmsg_base = kcmsg = (struct cmsghdr *)stackbuf;
	ucmsg = CMSG32_FIRSTHDR(kmsg);
	while(ucmsg != NULL) {
		if(get_user(ucmlen, &ucmsg->cmsg_len))
			return -EFAULT;

		/* Catch bogons. */
		if(CMSG32_ALIGN(ucmlen) <
		   CMSG32_ALIGN(sizeof(struct cmsghdr32)))
			return -EINVAL;
		if((unsigned long)(((char *)ucmsg - (char *)kmsg->msg_control)
				   + ucmlen) > kmsg->msg_controllen)
			return -EINVAL;

		tmp = ((ucmlen - CMSG32_ALIGN(sizeof(*ucmsg))) +
		       CMSG_ALIGN(sizeof(struct cmsghdr)));
		kcmlen += tmp;
		ucmsg = CMSG32_NXTHDR(kmsg, ucmsg, ucmlen);
	}
	if (kcmlen == 0)
		return -EINVAL;

	/* The kcmlen holds the 64-bit version of the control length.
	 * It may not be modified as we do not stick it into the kmsg
	 * until we have successfully copied over all of the data
	 * from the user.
	 */
	if (kcmlen > stackbuf_size)
		kcmsg_base = kcmsg = kmalloc(kcmlen, GFP_KERNEL);
	if (kcmsg == NULL)
		return -ENOBUFS;

	/* Now copy them over neatly. */
	memset(kcmsg, 0, kcmlen);
	ucmsg = CMSG32_FIRSTHDR(kmsg);
	while (ucmsg != NULL) {
		__get_user(ucmlen, &ucmsg->cmsg_len);
		tmp = ((ucmlen - CMSG32_ALIGN(sizeof(*ucmsg))) +
		       CMSG_ALIGN(sizeof(struct cmsghdr)));
		kcmsg->cmsg_len = tmp;
		__get_user(kcmsg->cmsg_level, &ucmsg->cmsg_level);
		__get_user(kcmsg->cmsg_type, &ucmsg->cmsg_type);

		/* Copy over the data. */
		if(copy_from_user(CMSG_DATA(kcmsg),
				  CMSG32_DATA(ucmsg),
				  (ucmlen - CMSG32_ALIGN(sizeof(*ucmsg)))))
			goto out_free_efault;

		/* Advance. */
		kcmsg = (struct cmsghdr *)((char *)kcmsg + CMSG_ALIGN(tmp));
		ucmsg = CMSG32_NXTHDR(kmsg, ucmsg, ucmlen);
	}

	/* Ok, looks like we made it.  Hook it up and return success. */
	kmsg->msg_control = kcmsg_base;
	kmsg->msg_controllen = kcmlen;
	return 0;

out_free_efault:
	if(kcmsg_base != (struct cmsghdr *)stackbuf)
		kfree(kcmsg_base);
	return -EFAULT;
}

asmlinkage long sys32_sendmsg(int fd, struct msghdr32* user_msg, unsigned int user_flags)
{
	struct socket *sock;
	char address[MAX_SOCK_ADDR];
	struct iovec iov[UIO_FASTIOV];
	unsigned char ctl[sizeof(struct cmsghdr) + 20];
	unsigned char *ctl_buf = ctl;
	struct msghdr kern_msg;
	int err, total_len;
	
	PPCDBG(PPCDBG_SYS32, "sys32_sendmsg - entered - fd=%x, user_msg@=%p, user_flags=%x \n", fd, user_msg, user_flags);

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EMSGSIZE;
	err = verify_iovec32(&kern_msg, iov, address, VERIFY_READ);
	if (err < 0)
		goto out;
	total_len = err;

	if(kern_msg.msg_controllen) {
		err = cmsghdr_from_user32_to_kern(&kern_msg, ctl, sizeof(ctl));
		if(err)
			goto out_freeiov;
		ctl_buf = kern_msg.msg_control;
	}
	kern_msg.msg_flags = user_flags;

	sock = sockfd_lookup(fd, &err);
	if (sock != NULL) {
		if (sock->file->f_flags & O_NONBLOCK)
			kern_msg.msg_flags |= MSG_DONTWAIT;
		err = sock_sendmsg(sock, &kern_msg, total_len);
		sockfd_put(sock);
	}

	/* N.B. Use kfree here, as kern_msg.msg_controllen might change? */
	if(ctl_buf != ctl)
		kfree(ctl_buf);
out_freeiov:
	if(kern_msg.msg_iov != iov)
		kfree(kern_msg.msg_iov);
out:
	
	PPCDBG(PPCDBG_SYS32, "sys32_sendmsg - exited w/ %lx \n", err);
	return err;
}

static void put_cmsg32(struct msghdr *kmsg, int level, int type,
		       int len, void *data)
{
	struct cmsghdr32 *cm = (struct cmsghdr32 *) kmsg->msg_control;
	struct cmsghdr32 cmhdr;
	int cmlen = CMSG32_LEN(len);

	if (cm == NULL || kmsg->msg_controllen < sizeof(*cm)) {
		kmsg->msg_flags |= MSG_CTRUNC;
		return;
	}

	if (kmsg->msg_controllen < cmlen) {
		kmsg->msg_flags |= MSG_CTRUNC;
		cmlen = kmsg->msg_controllen;
	}
	cmhdr.cmsg_level = level;
	cmhdr.cmsg_type = type;
	cmhdr.cmsg_len = cmlen;

	if (copy_to_user(cm, &cmhdr, sizeof cmhdr))
		return;
	if (copy_to_user(CMSG32_DATA(cm), data, cmlen - sizeof(struct cmsghdr32)))
		return;
	cmlen = CMSG32_SPACE(len);
	kmsg->msg_control += cmlen;
	kmsg->msg_controllen -= cmlen;
}


static void scm_detach_fds32(struct msghdr *kmsg, struct scm_cookie *scm)
{
	struct cmsghdr32 *cm = (struct cmsghdr32 *) kmsg->msg_control;
	int fdmax = (kmsg->msg_controllen - sizeof(struct cmsghdr32)) / sizeof(int);
	int fdnum = scm->fp->count;
	struct file **fp = scm->fp->fp;
	int *cmfptr;
	int err = 0, i;

	if (fdnum < fdmax)
		fdmax = fdnum;

	for (i = 0, cmfptr = (int *) CMSG32_DATA(cm); i < fdmax; i++, cmfptr++) {
		int new_fd;
		err = get_unused_fd();
		if (err < 0)
			break;
		new_fd = err;
		err = put_user(new_fd, cmfptr);
		if (err) {
			put_unused_fd(new_fd);
			break;
		}
		/* Bump the usage count and install the file. */
		get_file(fp[i]);
		fd_install(new_fd, fp[i]);
	}

	if (i > 0) {
		int cmlen = CMSG32_LEN(i * sizeof(int));
		if (!err)
			err = put_user(SOL_SOCKET, &cm->cmsg_level);
		if (!err)
			err = put_user(SCM_RIGHTS, &cm->cmsg_type);
		if (!err)
			err = put_user(cmlen, &cm->cmsg_len);
		if (!err) {
			cmlen = CMSG32_SPACE(i * sizeof(int));
			kmsg->msg_control += cmlen;
			kmsg->msg_controllen -= cmlen;
		}
	}
	if (i < fdnum)
		kmsg->msg_flags |= MSG_CTRUNC;

	/*
	 * All of the files that fit in the message have had their
	 * usage counts incremented, so we just free the list.
	 */
	__scm_destroy(scm);
}

/* In these cases we (currently) can just copy to data over verbatim
 * because all CMSGs created by the kernel have well defined types which
 * have the same layout in both the 32-bit and 64-bit API.  One must add
 * some special cased conversions here if we start sending control messages
 * with incompatible types.
 *
 * SCM_RIGHTS and SCM_CREDENTIALS are done by hand in recvmsg32 right after
 * we do our work.  The remaining cases are:
 *
 * SOL_IP	IP_PKTINFO	struct in_pktinfo	32-bit clean
 *		IP_TTL		int			32-bit clean
 *		IP_TOS		__u8			32-bit clean
 *		IP_RECVOPTS	variable length		32-bit clean
 *		IP_RETOPTS	variable length		32-bit clean
 *		(these last two are clean because the types are defined
 *		 by the IPv4 protocol)
 *		IP_RECVERR	struct sock_extended_err +
 *				struct sockaddr_in	32-bit clean
 * SOL_IPV6	IPV6_RECVERR	struct sock_extended_err +
 *				struct sockaddr_in6	32-bit clean
 *		IPV6_PKTINFO	struct in6_pktinfo	32-bit clean
 *		IPV6_HOPLIMIT	int			32-bit clean
 *		IPV6_FLOWINFO	u32			32-bit clean
 *		IPV6_HOPOPTS	ipv6 hop exthdr		32-bit clean
 *		IPV6_DSTOPTS	ipv6 dst exthdr(s)	32-bit clean
 *		IPV6_RTHDR	ipv6 routing exthdr	32-bit clean
 *		IPV6_AUTHHDR	ipv6 auth exthdr	32-bit clean
 */
static void cmsg32_recvmsg_fixup(struct msghdr *kmsg, unsigned long orig_cmsg_uptr)
{
	unsigned char *workbuf, *wp;
	unsigned long bufsz, space_avail;
	struct cmsghdr *ucmsg;

	bufsz = ((unsigned long)kmsg->msg_control) - orig_cmsg_uptr;
	space_avail = kmsg->msg_controllen + bufsz;
	wp = workbuf = kmalloc(bufsz, GFP_KERNEL);
	if(workbuf == NULL)
		goto fail;

	/* To make this more sane we assume the kernel sends back properly
	 * formatted control messages.  Because of how the kernel will truncate
	 * the cmsg_len for MSG_TRUNC cases, we need not check that case either.
	 */
	ucmsg = (struct cmsghdr *) orig_cmsg_uptr;
	while(((unsigned long)ucmsg) <=
	      (((unsigned long)kmsg->msg_control) - sizeof(struct cmsghdr))) {
		struct cmsghdr32 *kcmsg32 = (struct cmsghdr32 *) wp;
		int clen64, clen32;

		/* UCMSG is the 64-bit format CMSG entry in user-space.
		 * KCMSG32 is within the kernel space temporary buffer
		 * we use to convert into a 32-bit style CMSG.
		 */
		__get_user(kcmsg32->cmsg_len, &ucmsg->cmsg_len);
		__get_user(kcmsg32->cmsg_level, &ucmsg->cmsg_level);
		__get_user(kcmsg32->cmsg_type, &ucmsg->cmsg_type);

		clen64 = kcmsg32->cmsg_len;
		copy_from_user(CMSG32_DATA(kcmsg32), CMSG_DATA(ucmsg),
			       clen64 - CMSG_ALIGN(sizeof(*ucmsg)));
		clen32 = ((clen64 - CMSG_ALIGN(sizeof(*ucmsg))) +
			  CMSG32_ALIGN(sizeof(struct cmsghdr32)));
		kcmsg32->cmsg_len = clen32;

		switch (kcmsg32->cmsg_type) {
			/*
			 * The timestamp type's data needs to be converted
			 * from 64-bit time values to 32-bit time values
			 */
		case SO_TIMESTAMP: {
			__kernel_time_t32* ptr_time32 = CMSG32_DATA(kcmsg32);
			__kernel_time_t*   ptr_time   = CMSG_DATA(ucmsg);
			*ptr_time32     = *ptr_time;
			*(ptr_time32+1) = *(ptr_time+1);
			kcmsg32->cmsg_len -= 2*(sizeof(__kernel_time_t) -
						sizeof(__kernel_time_t32));
		}
		default:;
		}

		ucmsg = (struct cmsghdr *) (((char *)ucmsg) + CMSG_ALIGN(clen64));
		wp = (((char *)kcmsg32) + CMSG32_ALIGN(kcmsg32->cmsg_len));
	}

	/* Copy back fixed up data, and adjust pointers. */
	bufsz = (wp - workbuf);
	copy_to_user((void *)orig_cmsg_uptr, workbuf, bufsz);

	kmsg->msg_control = (struct cmsghdr *)
		(((char *)orig_cmsg_uptr) + bufsz);
	kmsg->msg_controllen = space_avail - bufsz;

	kfree(workbuf);
	return;

fail:
	/* If we leave the 64-bit format CMSG chunks in there,
	 * the application could get confused and crash.  So to
	 * ensure greater recovery, we report no CMSGs.
	 */
	kmsg->msg_controllen += bufsz;
	kmsg->msg_control = (void *) orig_cmsg_uptr;
}

asmlinkage long sys32_recvmsg(int fd, struct msghdr32* user_msg, unsigned int user_flags)
{
	struct iovec iovstack[UIO_FASTIOV];
	struct msghdr kern_msg;
	char addr[MAX_SOCK_ADDR];
	struct socket *sock;
	struct iovec *iov = iovstack;
	struct sockaddr *uaddr;
	int *uaddr_len;
	unsigned long cmsg_ptr;
	int err, total_len, len = 0;
	
	PPCDBG(PPCDBG_SYS32, "sys32_recvmsg - entered - fd=%x, user_msg@=%p, user_flags=%x \n", fd, user_msg, user_flags);

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EMSGSIZE;

	uaddr = kern_msg.msg_name;
	uaddr_len = &user_msg->msg_namelen;
	err = verify_iovec32(&kern_msg, iov, addr, VERIFY_WRITE);
	if (err < 0)
		goto out;
	total_len = err;

	cmsg_ptr = (unsigned long) kern_msg.msg_control;
	kern_msg.msg_flags = 0;

	sock = sockfd_lookup(fd, &err);
	if (sock != NULL) {
		struct scm_cookie scm;

		if (sock->file->f_flags & O_NONBLOCK)
			user_flags |= MSG_DONTWAIT;
		memset(&scm, 0, sizeof(scm));
		err = sock->ops->recvmsg(sock, &kern_msg, total_len,
					 user_flags, &scm);
		if(err >= 0) {
			len = err;
			if(!kern_msg.msg_control) {
				if(sock->passcred || scm.fp)
					kern_msg.msg_flags |= MSG_CTRUNC;
				if(scm.fp)
					__scm_destroy(&scm);
			} else {
				/* If recvmsg processing itself placed some
				 * control messages into user space, it's is
				 * using 64-bit CMSG processing, so we need
				 * to fix it up before we tack on more stuff.
				 */
				if((unsigned long) kern_msg.msg_control != cmsg_ptr)
					cmsg32_recvmsg_fixup(&kern_msg, cmsg_ptr);

				/* Wheee... */
				if(sock->passcred)
					put_cmsg32(&kern_msg,
						   SOL_SOCKET, SCM_CREDENTIALS,
						   sizeof(scm.creds), &scm.creds);
				if(scm.fp != NULL)
					scm_detach_fds32(&kern_msg, &scm);
			}
		}
		sockfd_put(sock);
	}

  if (uaddr != NULL && err >= 0 && kern_msg.msg_namelen)
		err = move_addr_to_user(addr, kern_msg.msg_namelen, uaddr, uaddr_len);
	if(cmsg_ptr != 0 && err >= 0) {
		unsigned long ucmsg_ptr = ((unsigned long)kern_msg.msg_control);
		__kernel_size_t32 uclen = (__kernel_size_t32) (ucmsg_ptr - cmsg_ptr);
		err |= __put_user(uclen, &user_msg->msg_controllen);
	}
	if(err >= 0)
		err = __put_user(kern_msg.msg_flags, &user_msg->msg_flags);
	if(kern_msg.msg_iov != iov)
		kfree(kern_msg.msg_iov);
out:
	if(err < 0)
		return err;
	
	PPCDBG(PPCDBG_SYS32, "sys32_recvmsg - exited w/ %lx \n", len);
	return len;
}

/*
 * count32() counts the number of arguments/envelopes
 */
static int count32(u32 * argv, int max)
{
	int i = 0;

	if (argv != NULL) {
		for (;;) {
			u32 p; int error;

			error = get_user(p,argv);
			if (error)
				return error;
			if (!p)
				break;
			argv++;
			if (++i > max)
				return -E2BIG;
		}
	}
	return i;
}

/*
 * 'copy_string32()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 */
static int copy_strings32(int argc, u32 * argv, struct linux_binprm *bprm)
{
	while (argc-- > 0) {
		u32 str;
		int len;
		unsigned long pos;

		if (get_user(str, argv + argc) ||
		    !str ||
		    !(len = strnlen_user((char *)A(str), bprm->p)))
			return -EFAULT;

		if (bprm->p < len)
			return -E2BIG;

		bprm->p -= len;

		pos = bprm->p;
		while (len) {
			char *kaddr;
			struct page *page;
			int offset, bytes_to_copy, new, err;

			offset = pos % PAGE_SIZE;
			page = bprm->page[pos / PAGE_SIZE];
			new = 0;
			if (!page) {
				page = alloc_page(GFP_USER);
				bprm->page[pos / PAGE_SIZE] = page;
				if (!page)
					return -ENOMEM;
				new = 1;
			}
			kaddr = (char *)kmap(page);

			if (new && offset)
				memset(kaddr, 0, offset);
			bytes_to_copy = PAGE_SIZE - offset;
			if (bytes_to_copy > len) {
				bytes_to_copy = len;
				if (new)
					memset(kaddr+offset+len, 0,
					       PAGE_SIZE-offset-len);
			}

			err = copy_from_user(kaddr + offset, (char *)A(str),
					     bytes_to_copy);
			flush_page_to_ram(page);
			kunmap((unsigned long)kaddr);

			if (err)
				return -EFAULT;

			pos += bytes_to_copy;
			str += bytes_to_copy;
			len -= bytes_to_copy;
		}
	}
	return 0;
}

/*
 * sys32_execve() executes a new program.
 */
static int do_execve32(char * filename, u32 * argv, u32 * envp, struct pt_regs * regs)
{
	struct linux_binprm bprm;
	struct file * file;
	int retval;
	int i;

	bprm.p = PAGE_SIZE*MAX_ARG_PAGES-sizeof(void *);
	memset(bprm.page, 0, MAX_ARG_PAGES * sizeof(bprm.page[0]));

	file = open_exec(filename);

	retval = PTR_ERR(file);
	if (IS_ERR(file))
		return retval;

	bprm.file = file;
	bprm.filename = filename;
	bprm.sh_bang = 0;
	bprm.loader = 0;
	bprm.exec = 0;
	if ((bprm.argc = count32(argv, bprm.p / sizeof(u32))) < 0) {
		allow_write_access(file);
		fput(file);
		return bprm.argc;
	}
	if ((bprm.envc = count32(envp, bprm.p / sizeof(u32))) < 0) {
		allow_write_access(file);
		fput(file);
		return bprm.argc;
	}
  
	retval = prepare_binprm(&bprm);
	if (retval < 0)
		goto out;
	
	retval = copy_strings_kernel(1, &bprm.filename, &bprm);
	if (retval < 0)
		goto out;

	bprm.exec = bprm.p;
	retval = copy_strings32(bprm.envc, envp, &bprm);
	if (retval < 0)
		goto out;

	retval = copy_strings32(bprm.argc, argv, &bprm);
	if (retval < 0)
		goto out;

	retval = search_binary_handler(&bprm, regs);
	if (retval >= 0)
		/* execve success */
		return retval;

out:
	/* Something went wrong, return the inode and free the argument pages*/
	allow_write_access(bprm.file);
	if (bprm.file)
		fput(bprm.file);

	for (i=0 ; i<MAX_ARG_PAGES ; i++)
		if (bprm.page[i])
			__free_page(bprm.page[i]);

	return retval;
}

asmlinkage long sys32_execve(unsigned long a0, unsigned long a1, unsigned long a2,
			                       unsigned long a3, unsigned long a4, unsigned long a5,
			                       struct pt_regs *regs)
{
	int error;
	char * filename;
	
	filename = getname((char *) a0);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	if (regs->msr & MSR_FP)
		giveup_fpu(current);

	error = do_execve32(filename, (u32*) a1, (u32*) a2, regs);

	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);

out:
	return error;
}

/* Set up a thread for executing a new program. */
void start_thread32(struct pt_regs* regs, unsigned long nip, unsigned long sp)
{
	set_fs(USER_DS);
	memset(regs->gpr, 0, sizeof(regs->gpr));
	memset(&regs->ctr, 0, 4 * sizeof(regs->ctr));
	regs->nip = nip;
	regs->gpr[1] = sp;
	regs->msr = MSR_USER32;
#ifndef CONFIG_SMP
	if (last_task_used_math == current)
		last_task_used_math = 0;
#endif
	current->thread.fpscr = 0;
}

extern asmlinkage int sys_prctl(int option, unsigned long arg2, unsigned long arg3,
				unsigned long arg4, unsigned long arg5);

/* Note: it is necessary to treat option as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_prctl(u32 option, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
	PPCDBG(PPCDBG_SYS32, "sys32_prctl - running - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return sys_prctl((int)option,
			 (unsigned long) arg2,
			 (unsigned long) arg3,
			 (unsigned long) arg4,
			 (unsigned long) arg5);
}

extern asmlinkage int sys_sched_rr_get_interval(pid_t pid, struct timespec *interval);

/* Note: it is necessary to treat pid as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage int sys32_sched_rr_get_interval(u32 pid, struct timespec32 *interval)
{
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs ();
	
	PPCDBG(PPCDBG_SYS32, "sys32_sched_rr_get_interval - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	
	set_fs (KERNEL_DS);
	ret = sys_sched_rr_get_interval((int)pid, &t);
	set_fs (old_fs);
	if (put_user (t.tv_sec, &interval->tv_sec) ||
	    __put_user (t.tv_nsec, &interval->tv_nsec))
		return -EFAULT;
	
	PPCDBG(PPCDBG_SYS32, "sys32_sched_rr_get_interval - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return ret;
}

extern asmlinkage int sys_pciconfig_read(unsigned long bus, unsigned long dfn, unsigned long off,
					 unsigned long len, unsigned char *buf);

asmlinkage int sys32_pciconfig_read(u32 bus, u32 dfn, u32 off, u32 len, u32 ubuf)
{
	
	PPCDBG(PPCDBG_SYS32, "sys32_pciconfig_read - running - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return sys_pciconfig_read((unsigned long) bus,
				  (unsigned long) dfn,
				  (unsigned long) off,
				  (unsigned long) len,
				  (unsigned char *)AA(ubuf));
}




extern asmlinkage int sys_pciconfig_write(unsigned long bus, unsigned long dfn, unsigned long off,
					                                unsigned long len, unsigned char *buf);

asmlinkage int sys32_pciconfig_write(u32 bus, u32 dfn, u32 off, u32 len, u32 ubuf)
{
	
	PPCDBG(PPCDBG_SYS32, "sys32_pciconfig_write - running - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return sys_pciconfig_write((unsigned long) bus,
				   (unsigned long) dfn,
				   (unsigned long) off,
				   (unsigned long) len,
				   (unsigned char *)AA(ubuf));
}

extern asmlinkage int sys_newuname(struct new_utsname * name);

asmlinkage int ppc64_newuname(struct new_utsname * name)
{
	int errno = sys_newuname(name);

	if (current->personality == PER_LINUX32 && !errno) {
		if(copy_to_user(name->machine, "ppc\0\0", 8)) {
			errno = -EFAULT;
		}
	}
	return errno;
}

extern asmlinkage long sys_personality(unsigned long);

asmlinkage int sys32_personality(unsigned long personality)
{
	int ret;
	if (current->personality == PER_LINUX32 && personality == PER_LINUX)
		personality = PER_LINUX32;
	ret = sys_personality(personality);
	if (ret == PER_LINUX32)
		ret = PER_LINUX;
	return ret;
}



extern asmlinkage long sys_access(const char * filename, int mode);

/* Note: it is necessary to treat mode as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_access(const char * filename, u32 mode)
{
	return sys_access(filename, (int)mode);
}


extern asmlinkage int sys_clone(int p1, int p2, int p3, int p4, int p5, int p6, struct pt_regs *regs);

/* Note: it is necessary to treat p1, p2, p3, p4, p5, p7, and regs as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage int sys32_clone(u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6, struct pt_regs *regs)
{
	return sys_clone((int)p1, (int)p2, (int)p3, (int)p4, (int)p5, (int)p6, regs);
}


extern asmlinkage long sys_creat(const char * pathname, int mode);

/* Note: it is necessary to treat mode as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_creat(const char * pathname, u32 mode)
{
	return sys_creat(pathname, (int)mode);
}


extern asmlinkage long sys_exit(int error_code);

/* Note: it is necessary to treat error_code as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_exit(u32 error_code)
{
	return sys_exit((int)error_code);
}


extern asmlinkage long sys_wait4(pid_t pid, unsigned int * stat_addr, int options, struct rusage * ru);

/* Note: it is necessary to treat pid and options as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_wait4(u32 pid, unsigned int * stat_addr, u32 options, struct rusage * ru)
{
        PPCDBG(PPCDBG_SYS32, "sys32_wait4 - running - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	if (!ru)
		return sys_wait4((int)pid, stat_addr, options, NULL);
	else {
		struct rusage r;
		int ret;
		unsigned int status;
		mm_segment_t old_fs = get_fs();
		
		set_fs (KERNEL_DS);
		ret = sys_wait4((int)pid, stat_addr ? &status : NULL, options, &r);
		set_fs (old_fs);
		if (put_rusage ((struct rusage32 *)ru, &r)) return -EFAULT;
		if (stat_addr && put_user (status, stat_addr))
			return -EFAULT;
		return ret;
	}
	
}


extern asmlinkage long sys_waitpid(pid_t pid, unsigned int * stat_addr, int options);

/* Note: it is necessary to treat pid and options as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_waitpid(u32 pid, unsigned int * stat_addr, u32 options)
{
	return sys_waitpid((int)pid, stat_addr, (int)options);
}


extern asmlinkage int sys_fork(int p1, int p2, int p3, int p4, int p5, int p6, struct pt_regs *regs);

/* Note: it is necessary to treat p1, p2, p3, p4, p5, and p6 as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage int sys32_fork(u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6, struct pt_regs *regs)
{
	return sys_fork((int)p1, (int)p2, (int)p3, (int)p4, (int)p5, (int)p6, regs);
}


extern asmlinkage long sys_getgroups(int gidsetsize, gid_t *grouplist);

/* Note: it is necessary to treat gidsetsize as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_getgroups(u32 gidsetsize, gid_t *grouplist)
{
	return sys_getgroups((int)gidsetsize, grouplist);
}


extern asmlinkage long sys_getpgid(pid_t pid);

/* Note: it is necessary to treat pid as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_getpgid(u32 pid)
{
	return sys_getpgid((int)pid);
}


extern asmlinkage long sys_getpriority(int which, int who);

/* Note: it is necessary to treat which and who as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_getpriority(u32 which, u32 who)
{
	return sys_getpriority((int)which, (int)who);
}


extern asmlinkage long sys_getsid(pid_t pid);

/* Note: it is necessary to treat pid as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_getsid(u32 pid)
{
	return sys_getsid((int)pid);
}


extern asmlinkage long sys_kill(int pid, int sig);

/* Note: it is necessary to treat pid and sig as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_kill(u32 pid, u32 sig)
{
	return sys_kill((int)pid, (int)sig);
}


extern asmlinkage long sys_mkdir(const char * pathname, int mode);

/* Note: it is necessary to treat mode as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_mkdir(const char * pathname, u32 mode)
{
	return sys_mkdir(pathname, (int)mode);
}


extern asmlinkage long sys_mlockall(int flags);

/* Note: it is necessary to treat flags as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_mlockall(u32 flags)
{
	return sys_mlockall((int)flags);
}


extern asmlinkage long sys_msync(unsigned long start, size_t len, int flags);

/* Note: it is necessary to treat flags as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_msync(unsigned long start, size_t len, u32 flags)
{
	return sys_msync(start, len, (int)flags);
}


extern asmlinkage long sys_nice(int increment);

/* Note: it is necessary to treat increment as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_nice(u32 increment)
{
	return sys_nice((int)increment);
}

/*
 * This is just a version for 32-bit applications which does
 * not force O_LARGEFILE on.
 */
long sys32_open(const char * filename, int flags, int mode)
{
	char * tmp;
	int fd, error;

	tmp = getname(filename);
	fd = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {
		fd = get_unused_fd();
		if (fd >= 0) {
			struct file * f = filp_open(tmp, flags, mode);
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

extern asmlinkage long sys_readlink(const char * path, char * buf, int bufsiz);

/* Note: it is necessary to treat bufsiz as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_readlink(const char * path, char * buf, u32 bufsiz)
{
	return sys_readlink(path, buf, (int)bufsiz);
}


extern asmlinkage long sys_reboot(int magic1, int magic2, unsigned int cmd, void * arg);

/* Note: it is necessary to treat magic1 and magic2 as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_reboot(u32 magic1, u32 magic2, unsigned int cmd, void * arg)
{
	return sys_reboot((int)magic1, (int)magic2, cmd, arg);
}


extern asmlinkage long sys_sched_get_priority_max(int policy);

/* Note: it is necessary to treat option as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sched_get_priority_max(u32 policy)
{
	return sys_sched_get_priority_max((int)policy);
}


extern asmlinkage long sys_sched_get_priority_min(int policy);

/* Note: it is necessary to treat policy as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sched_get_priority_min(u32 policy)
{
	return sys_sched_get_priority_min((int)policy);
}


extern asmlinkage long sys_sched_getparam(pid_t pid, struct sched_param *param);

/* Note: it is necessary to treat pid as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sched_getparam(u32 pid, struct sched_param *param)
{
	return sys_sched_getparam((int)pid, param);
}


extern asmlinkage long sys_sched_getscheduler(pid_t pid);

/* Note: it is necessary to treat pid as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sched_getscheduler(u32 pid)
{
	return sys_sched_getscheduler((int)pid);
}


extern asmlinkage long sys_sched_setparam(pid_t pid, struct sched_param *param);

/* Note: it is necessary to treat pid as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sched_setparam(u32 pid, struct sched_param *param)
{
	return sys_sched_setparam((int)pid, param);
}


extern asmlinkage long sys_sched_setscheduler(pid_t pid, int policy, struct sched_param *param);

/* Note: it is necessary to treat pid and policy as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sched_setscheduler(u32 pid, u32 policy, struct sched_param *param)
{
	return sys_sched_setscheduler((int)pid, (int)policy, param);
}


extern asmlinkage long sys_setdomainname(char *name, int len);

/* Note: it is necessary to treat len as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_setdomainname(char *name, u32 len)
{
	return sys_setdomainname(name, (int)len);
}


extern asmlinkage long sys_setgroups(int gidsetsize, gid_t *grouplist);

/* Note: it is necessary to treat gidsetsize as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_setgroups(u32 gidsetsize, gid_t *grouplist)
{
	return sys_setgroups((int)gidsetsize, grouplist);
}


extern asmlinkage long sys_sethostname(char *name, int len);

/* Note: it is necessary to treat len as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sethostname(char *name, u32 len)
{
	return sys_sethostname(name, (int)len);
}


extern asmlinkage long sys_setpgid(pid_t pid, pid_t pgid);

/* Note: it is necessary to treat pid and pgid as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_setpgid(u32 pid, u32 pgid)
{
	return sys_setpgid((int)pid, (int)pgid);
}


extern asmlinkage long sys_setpriority(int which, int who, int niceval);

/* Note: it is necessary to treat which, who, and niceval as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_setpriority(u32 which, u32 who, u32 niceval)
{
	return sys_setpriority((int)which, (int)who, (int)niceval);
}


extern asmlinkage long sys_ssetmask(int newmask);

/* Note: it is necessary to treat newmask as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_ssetmask(u32 newmask)
{
	return sys_ssetmask((int) newmask);
}


extern asmlinkage long sys_swapon(const char * specialfile, int swap_flags);

/* Note: it is necessary to treat swap_flags as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_swapon(const char * specialfile, u32 swap_flags)
{
	return sys_swapon(specialfile, (int)swap_flags);
}


extern asmlinkage long sys_syslog(int type, char * buf, int len);

/* Note: it is necessary to treat type and len as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_syslog(u32 type, char * buf, u32 len)
{
	return sys_syslog((int)type, buf, (int)len);
}


extern asmlinkage long sys_umask(int mask);

/* Note: it is necessary to treat mask as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_umask(u32 mask)
{
	return sys_umask((int)mask);
}


extern asmlinkage long sys_umount(char * name, int flags);

/* Note: it is necessary to treat flags as an unsigned int,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_umount(char * name, u32 flags)
{
	return sys_umount(name, (int)flags);
}


extern asmlinkage int sys_vfork(int p1, int p2, int p3, int p4, int p5, int p6, struct pt_regs *regs);

/* Note: it is necessary to treat p1, p2, p3, p4, p5, and p6 as unsigned ints,
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage int sys32_vfork(u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6, struct pt_regs *regs)
{
	return sys_vfork((int)p1, (int)p2, (int)p3, (int)p4, (int)p5, (int)p6, regs);
}

extern asmlinkage ssize_t sys_pread(unsigned int fd, char * buf,
				    size_t count, loff_t pos);

extern asmlinkage ssize_t sys_pwrite(unsigned int fd, const char * buf,
				     size_t count, loff_t pos);

asmlinkage ssize_t32 sys32_pread(unsigned int fd, char *ubuf,
				 __kernel_size_t32 count, u32 reg6, u32 poshi, u32 poslo)
{
	return sys_pread(fd, ubuf, count, ((loff_t)AA(poshi) << 32) | AA(poslo));
}

asmlinkage ssize_t32 sys32_pwrite(unsigned int fd, char *ubuf,
				  __kernel_size_t32 count, u32 reg6 ,u32 poshi, u32 poslo)
{
	return sys_pwrite(fd, ubuf, count, ((loff_t)AA(poshi) << 32) | AA(poslo));
}

extern ssize_t sys_readahead(int fd, loff_t offset, size_t count);

ssize_t32 sys32_readahead(int fd, u32 r4, u32 offhi, u32 offlo, u32 count)
{
        return sys_readahead(fd, ((loff_t)offhi << 32) | offlo, AA(count));
}

extern asmlinkage long sys_truncate(const char * path, unsigned long length);
extern asmlinkage long sys_ftruncate(unsigned int fd, unsigned long length);

asmlinkage int sys32_truncate64(const char * path, u32 reg4, unsigned long high, unsigned long low)
{
     	if ((int)high < 0)
		return -EINVAL;
	else
		return sys_truncate(path, (high << 32) | low);
}

asmlinkage int sys32_ftruncate64(unsigned int fd, u32 reg4, unsigned long high, unsigned long low)
{
     	if ((int)high < 0)
		return -EINVAL;
	else
		return sys_ftruncate(fd, (high << 32) | low);
}



asmlinkage long sys32_fcntl64(unsigned int fd, unsigned int cmd, unsigned long arg)
{
     	if (cmd >= F_GETLK64 && cmd <= F_SETLKW64)
		return sys_fcntl(fd, cmd + F_GETLK - F_GETLK64, arg);
	return sys32_fcntl(fd, cmd, arg);
}




struct __sysctl_args32 {
	u32 name;
	int nlen;
	u32 oldval;
	u32 oldlenp;
	u32 newval;
	u32 newlen;
	u32 __unused[4];
};

extern asmlinkage long sys32_sysctl(struct __sysctl_args32 *args)
{
	struct __sysctl_args32 tmp;
	int error;
	size_t oldlen, *oldlenp = NULL;
	unsigned long addr = (((long)&args->__unused[0]) + 7) & ~7;

	if (copy_from_user(&tmp, args, sizeof(tmp)))
		return -EFAULT;

	if (tmp.oldval && tmp.oldlenp) {
		/* Duh, this is ugly and might not work if sysctl_args
		   is in read-only memory, but do_sysctl does indirectly
		   a lot of uaccess in both directions and we'd have to
		   basically copy the whole sysctl.c here, and
		   glibc's __sysctl uses rw memory for the structure
		   anyway.  */
		if (get_user(oldlen, (u32 *)A(tmp.oldlenp)) ||
		    put_user(oldlen, (size_t *)addr))
			return -EFAULT;
		oldlenp = (size_t *)addr;
	}

	lock_kernel();
	error = do_sysctl((int *)A(tmp.name), tmp.nlen, (void *)A(tmp.oldval),
			  oldlenp, (void *)A(tmp.newval), tmp.newlen);
	unlock_kernel();
	if (oldlenp) {
		if (!error) {
			if (get_user(oldlen, (size_t *)addr) ||
			    put_user(oldlen, (u32 *)A(tmp.oldlenp)))
				error = -EFAULT;
		}
		copy_to_user(args->__unused, tmp.__unused, sizeof(tmp.__unused));
	}
	return error;
}

asmlinkage long sys32_time(__kernel_time_t32* tloc)
{
	__kernel_time_t32 secs;

	struct timeval tv;

	do_gettimeofday( &tv );
	secs = tv.tv_sec;

	if (tloc) {
		if (put_user(secs,tloc))
			secs = -EFAULT;
	}

	return secs;
}

extern unsigned long sys_mmap(unsigned long addr, size_t len,
			      unsigned long prot, unsigned long flags,
			      unsigned long fd, off_t offset);

unsigned long sys32_mmap2(unsigned long addr, size_t len,
			  unsigned long prot, unsigned long flags,
			  unsigned long fd, unsigned long pgoff)
{
	/* This should remain 12 even if PAGE_SIZE changes */
	return sys_mmap(addr, len, prot, flags, fd, pgoff << 12);
}
