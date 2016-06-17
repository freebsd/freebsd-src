/*
 * sys_parisc32.c: Conversion between 32bit and 64bit native syscalls.
 *
 * Copyright (C) 2000-2001 Hewlett Packard Company
 * Copyright (C) 2000 John Marvin
 * Copyright (C) 2001 Matthew Wilcox
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * environment. Based heavily on sys_ia32.c and sys_sparc32.c.
 */

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
#include <linux/module.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/cache.h>
#include <linux/nfsd/xdr.h>
#include <linux/nfsd/syscall.h>
#include <linux/poll.h>
#include <linux/personality.h>
#include <linux/stat.h>
#include <linux/filter.h>			/* for setsockopt() */
#include <linux/icmpv6.h>			/* for setsockopt() */
#include <linux/netfilter_ipv4/ip_queue.h>	/* for setsockopt() */
#include <linux/netfilter_ipv4/ip_tables.h>	/* for setsockopt() */
#include <linux/netfilter_ipv6/ip6_tables.h>	/* for setsockopt() */
#include <linux/highmem.h>
#include <linux/highuid.h>
#include <linux/mman.h>

#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include "sys32.h"

#define A(__x) ((unsigned long)(__x))


#undef DEBUG

#ifdef DEBUG
#define DBG(x)	printk x
#else
#define DBG(x)
#endif


/*
 * count32() counts the number of arguments/envelopes. It is basically
 *           a copy of count() from fs/exec.c, except that it works
 *           with 32 bit argv and envp pointers.
 */

static int count32(u32 *argv, int max)
{
	int i = 0;

	if (argv != NULL) {
		for (;;) {
			u32 p;
			int error;

			error = get_user(p,argv);
			if (error)
				return error;
			if (!p)
				break;
			argv++;
			if(++i > max)
				return -E2BIG;
		}
	}
	return i;
}


/*
 * copy_strings32() is basically a copy of copy_strings() from fs/exec.c
 *                  except that it works with 32 bit argv and envp pointers.
 */


static int copy_strings32(int argc, u32 *argv, struct linux_binprm *bprm)
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
		while (len > 0) {
			char *kaddr;
			int i, new, err;
			struct page *page;
			int offset, bytes_to_copy;

			offset = pos % PAGE_SIZE;
			i = pos/PAGE_SIZE;
			page = bprm->page[i];
			new = 0;
			if (!page) {
				page = alloc_page(GFP_HIGHUSER);
				bprm->page[i] = page;
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
					memset(kaddr+offset+len, 0, PAGE_SIZE-offset-len);
			}
			err = copy_from_user(kaddr + offset, (char *)A(str), bytes_to_copy);
			flush_dcache_page(page);
			flush_page_to_ram(page);
			kunmap(page);

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
 * do_execve32() is mostly a copy of do_execve(), with the exception
 * that it processes 32 bit argv and envp pointers.
 */

static inline int 
do_execve32(char * filename, u32 * argv, u32 * envp, struct pt_regs * regs)
{
	struct linux_binprm bprm;
	struct file *file;
	int retval;
	int i;

	file = open_exec(filename);

	retval = PTR_ERR(file);
	if (IS_ERR(file))
		return retval;

	bprm.p = PAGE_SIZE*MAX_ARG_PAGES-sizeof(void *);
	memset(bprm.page, 0, MAX_ARG_PAGES*sizeof(bprm.page[0]));

	DBG(("do_execve32(%s, %p, %p, %p)\n", filename, argv, envp, regs));

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
		return bprm.envc;
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

	retval = search_binary_handler(&bprm,regs);
	if (retval >= 0)
		/* execve success */
		return retval;

out:
	/* Something went wrong, return the inode and free the argument pages*/
	allow_write_access(bprm.file);
	if (bprm.file)
		fput(bprm.file);

	for (i = 0 ; i < MAX_ARG_PAGES ; i++) {
		struct page * page = bprm.page[i];
		if (page)
			__free_page(page);
	}

	return retval;
}

/*
 * sys32_execve() executes a new program.
 */

asmlinkage int sys32_execve(struct pt_regs *regs)
{
	int error;
	char *filename;

	DBG(("sys32_execve(%p) r26 = 0x%lx\n", regs, regs->gr[26]));
	filename = getname((char *) regs->gr[26]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve32(filename, (u32 *) regs->gr[25],
		(u32 *) regs->gr[24], regs);
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
out:

	return error;
}

asmlinkage long sys32_unimplemented(int r26, int r25, int r24, int r23,
	int r22, int r21, int r20)
{
    printk(KERN_ERR "%s(%d): Unimplemented 32 on 64 syscall #%d!\n", 
    	current->comm, current->pid, r20);
    return -ENOSYS;
}

/* 32-bit user apps use struct statfs which uses 'long's */
struct statfs32 {
	__s32 f_type;
	__s32 f_bsize;
	__s32 f_blocks;
	__s32 f_bfree;
	__s32 f_bavail;
	__s32 f_files;
	__s32 f_ffree;
	__kernel_fsid_t f_fsid;
	__s32 f_namelen;
	__s32 f_spare[6];
};

/* convert statfs struct to statfs32 struct and copy result to user */
static unsigned long statfs32_to_user(struct statfs32 *ust32, struct statfs *st)
{
    struct statfs32 st32;
#undef CP
#define CP(a) st32.a = st->a
    CP(f_type);
    CP(f_bsize);
    CP(f_blocks);
    CP(f_bfree);
    CP(f_bavail);
    CP(f_files);
    CP(f_ffree);
    CP(f_fsid);
    CP(f_namelen);
    return copy_to_user(ust32, &st32, sizeof st32);
}

/* The following statfs calls are copies of code from linux/fs/open.c and
 * should be checked against those from time to time */
asmlinkage long sys32_statfs(const char * path, struct statfs32 * buf)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(path, &nd);
	if (!error) {
		struct statfs tmp;
		error = vfs_statfs(nd.dentry->d_inode->i_sb, &tmp);
		if (!error && statfs32_to_user(buf, &tmp))
			error = -EFAULT;
		path_release(&nd);
	}
	return error;
}

asmlinkage long sys32_fstatfs(unsigned int fd, struct statfs32 * buf)
{
	struct file * file;
	struct statfs tmp;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;
	error = vfs_statfs(file->f_dentry->d_inode->i_sb, &tmp);
	if (!error && statfs32_to_user(buf, &tmp))
		error = -EFAULT;
	fput(file);
out:
	return error;
}

/* These may not work without my local types changes, but I wanted the
 * code available in case it's useful to others. -PB
 */

/* from utime.h */
struct utimbuf32 {
	__kernel_time_t32 actime;
	__kernel_time_t32 modtime;
};

asmlinkage long sys32_utime(char *filename, struct utimbuf32 *times)
{
    struct utimbuf32 times32;
    struct utimbuf times64;
    extern long sys_utime(char *filename, struct utimbuf *times);
    char *fname;
    long ret;

    if (!times)
    	return sys_utime(filename, NULL);

    /* get the 32-bit struct from user space */
    if (copy_from_user(&times32, times, sizeof times32))
    	return -EFAULT;

    /* convert it into the 64-bit one */
    times64.actime = times32.actime;
    times64.modtime = times32.modtime;

    /* grab the file name */
    fname = getname(filename);

    KERNEL_SYSCALL(ret, sys_utime, fname, &times64);

    /* free the file name */
    putname(fname);

    return ret;
}

struct tms32 {
	__kernel_clock_t32 tms_utime;
	__kernel_clock_t32 tms_stime;
	__kernel_clock_t32 tms_cutime;
	__kernel_clock_t32 tms_cstime;
};
                                
asmlinkage long sys32_times(struct tms32 *tbuf)
{
	struct tms t;
	long ret;
	extern asmlinkage long sys_times(struct tms * tbuf);
int err;
	
	KERNEL_SYSCALL(ret, sys_times, tbuf ? &t : NULL);
	if (tbuf) {
		err = put_user (t.tms_utime, &tbuf->tms_utime);
		err |= __put_user (t.tms_stime, &tbuf->tms_stime);
		err |= __put_user (t.tms_cutime, &tbuf->tms_cutime);
		err |= __put_user (t.tms_cstime, &tbuf->tms_cstime);
		if (err)
			ret = -EFAULT;
	}
	return ret;
}

struct flock32 {
	short l_type;
	short l_whence;
	__kernel_off_t32 l_start;
	__kernel_off_t32 l_len;
	__kernel_pid_t32 l_pid;
};


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
			long ret;
			
			if(get_flock(&f, (struct flock32 *)arg))
				return -EFAULT;
			KERNEL_SYSCALL(ret, sys_fcntl, fd, cmd, (unsigned long)&f);
			if (ret) return ret;
			if (f.l_start >= 0x7fffffffUL ||
			    f.l_len >= 0x7fffffffUL ||
			    f.l_start + f.l_len >= 0x7fffffffUL)
				return -EOVERFLOW;
			if(put_flock(&f, (struct flock32 *)arg))
				return -EFAULT;
			return 0;
		}
	default:
		return sys_fcntl(fd, cmd, (unsigned long)arg);
	}
}

#ifdef CONFIG_SYSCTL

struct __sysctl_args32 {
	u32 name;
	int nlen;
	u32 oldval;
	u32 oldlenp;
	u32 newval;
	u32 newlen;
	u32 __unused[4];
};

asmlinkage long sys32_sysctl(struct __sysctl_args32 *args)
{
	struct __sysctl_args32 tmp;
	int error;
	unsigned int oldlen32;
	size_t oldlen, *oldlenp = NULL;
	unsigned long addr = (((long)&args->__unused[0]) + 7) & ~7;
	extern int do_sysctl(int *name, int nlen, void *oldval, size_t *oldlenp,
	       void *newval, size_t newlen);

	DBG(("sysctl32(%p)\n", args));

	if (copy_from_user(&tmp, args, sizeof(tmp)))
		return -EFAULT;

	if (tmp.oldval && tmp.oldlenp) {
		/* Duh, this is ugly and might not work if sysctl_args
		   is in read-only memory, but do_sysctl does indirectly
		   a lot of uaccess in both directions and we'd have to
		   basically copy the whole sysctl.c here, and
		   glibc's __sysctl uses rw memory for the structure
		   anyway.  */
		/* a possibly better hack than this, which will avoid the
		 * problem if the struct is read only, is to push the
		 * 'oldlen' value out to the user's stack instead. -PB
		 */
		if (get_user(oldlen32, (u32 *)(u64)tmp.oldlenp))
			return -EFAULT;
		oldlen = oldlen32;
		if (put_user(oldlen, (size_t *)addr))
			return -EFAULT;
		oldlenp = (size_t *)addr;
	}

	lock_kernel();
	error = do_sysctl((int *)(u64)tmp.name, tmp.nlen, (void *)(u64)tmp.oldval,
			  oldlenp, (void *)(u64)tmp.newval, tmp.newlen);
	unlock_kernel();
	if (oldlenp) {
		if (!error) {
			if (get_user(oldlen, (size_t *)addr)) {
				error = -EFAULT;
			} else {
				oldlen32 = oldlen;
				if (put_user(oldlen32, (u32 *)(u64)tmp.oldlenp))
					error = -EFAULT;
			}
		}
		if (copy_to_user(args->__unused, tmp.__unused, sizeof(tmp.__unused)))
			error = -EFAULT;
	}
	return error;
}

#else /* CONFIG_SYSCTL */

asmlinkage long sys32_sysctl(struct __sysctl_args *args)
{
	return -ENOSYS;
}
#endif /* CONFIG_SYSCTL */

struct timespec32 {
	s32    tv_sec;
	s32    tv_nsec;
};
                
static int
put_timespec32(struct timespec32 *u, struct timespec *t)
{
	struct timespec32 t32;
	t32.tv_sec = t->tv_sec;
	t32.tv_nsec = t->tv_nsec;
	return copy_to_user(u, &t32, sizeof t32);
}

asmlinkage int sys32_nanosleep(struct timespec32 *rqtp, struct timespec32 *rmtp)
{
	struct timespec t;
	struct timespec32 t32;
	int ret;
	extern asmlinkage int sys_nanosleep(struct timespec *rqtp, struct timespec *rmtp);
	
	if (copy_from_user(&t32, rqtp, sizeof t32))
		return -EFAULT;
	t.tv_sec = t32.tv_sec;
	t.tv_nsec = t32.tv_nsec;

	DBG(("sys32_nanosleep({%d, %d})\n", t32.tv_sec, t32.tv_nsec));

	KERNEL_SYSCALL(ret, sys_nanosleep, &t, rmtp ? &t : NULL);
	if (rmtp && ret == -EINTR) {
		if (put_timespec32(rmtp, &t))
			return -EFAULT;
	}
	return ret;
}

asmlinkage long sys32_sched_rr_get_interval(pid_t pid,
	struct timespec32 *interval)
{
	struct timespec t;
	int ret;
	extern asmlinkage long sys_sched_rr_get_interval(pid_t pid, struct timespec *interval);
	
	KERNEL_SYSCALL(ret, sys_sched_rr_get_interval, pid, &t);
	if (put_timespec32(interval, &t))
		return -EFAULT;
	return ret;
}

typedef __kernel_time_t32 time_t32;

static int
put_timeval32(struct timeval32 *u, struct timeval *t)
{
	struct timeval32 t32;
	t32.tv_sec = t->tv_sec;
	t32.tv_usec = t->tv_usec;
	return copy_to_user(u, &t32, sizeof t32);
}

static int
get_timeval32(struct timeval32 *u, struct timeval *t)
{
	int err;
	struct timeval32 t32;

	if ((err = copy_from_user(&t32, u, sizeof t32)) == 0)
	{
	    t->tv_sec = t32.tv_sec;
	    t->tv_usec = t32.tv_usec;
	}
	return err;
}

asmlinkage long sys32_time(time_t32 *tloc)
{
    time_t now = CURRENT_TIME;
    time_t32 now32 = now;

    if (tloc)
    	if (put_user(now32, tloc))
		now32 = -EFAULT;

    return now32;
}

asmlinkage int
sys32_gettimeofday(struct timeval32 *tv, struct timezone *tz)
{
    extern void do_gettimeofday(struct timeval *tv);

    if (tv) {
	    struct timeval ktv;
	    do_gettimeofday(&ktv);
	    if (put_timeval32(tv, &ktv))
		    return -EFAULT;
    }
    if (tz) {
	    extern struct timezone sys_tz;
	    if (copy_to_user(tz, &sys_tz, sizeof(sys_tz)))
		    return -EFAULT;
    }
    return 0;
}

asmlinkage int
sys32_settimeofday(struct timeval32 *tv, struct timezone *tz)
{
    struct timeval ktv;
    struct timezone ktz;
    extern int do_sys_settimeofday(struct timeval *tv, struct timezone *tz);

    if (tv) {
	    if (get_timeval32(tv, &ktv))
		    return -EFAULT;
    }
    if (tz) {
	    if (copy_from_user(&ktz, tz, sizeof(ktz)))
		    return -EFAULT;
    }

    return do_sys_settimeofday(tv ? &ktv : NULL, tz ? &ktz : NULL);
}

struct	itimerval32 {
	struct	timeval32 it_interval;	/* timer interval */
	struct	timeval32 it_value;	/* current value */
};

asmlinkage long sys32_getitimer(int which, struct itimerval32 *ov32)
{
	int error = -EFAULT;
	struct itimerval get_buffer;
	extern int do_getitimer(int which, struct itimerval *value);

	if (ov32) {
		error = do_getitimer(which, &get_buffer);
		if (!error) {
			struct itimerval32 gb32;
			gb32.it_interval.tv_sec = get_buffer.it_interval.tv_sec;
			gb32.it_interval.tv_usec = get_buffer.it_interval.tv_usec;
			gb32.it_value.tv_sec = get_buffer.it_value.tv_sec;
			gb32.it_value.tv_usec = get_buffer.it_value.tv_usec;
			if (copy_to_user(ov32, &gb32, sizeof(gb32)))
				error = -EFAULT; 
		}
	}
	return error;
}

asmlinkage long sys32_setitimer(int which, struct itimerval32 *v32,
			      struct itimerval32 *ov32)
{
	struct itimerval set_buffer, get_buffer;
	struct itimerval32 sb32, gb32;
	extern int do_setitimer(int which, struct itimerval *value, struct itimerval *ov32);
	int error;

	if (v32) {
		if(copy_from_user(&sb32, v32, sizeof(sb32)))
			return -EFAULT;

		set_buffer.it_interval.tv_sec = sb32.it_interval.tv_sec;
		set_buffer.it_interval.tv_usec = sb32.it_interval.tv_usec;
		set_buffer.it_value.tv_sec = sb32.it_value.tv_sec;
		set_buffer.it_value.tv_usec = sb32.it_value.tv_usec;
	} else
		memset((char *) &set_buffer, 0, sizeof(set_buffer));

	error = do_setitimer(which, &set_buffer, ov32 ? &get_buffer : 0);
	if (error || !ov32)
		return error;

	gb32.it_interval.tv_sec = get_buffer.it_interval.tv_sec;
	gb32.it_interval.tv_usec = get_buffer.it_interval.tv_usec;
	gb32.it_value.tv_sec = get_buffer.it_value.tv_sec;
	gb32.it_value.tv_usec = get_buffer.it_value.tv_usec;
	if (copy_to_user(ov32, &gb32, sizeof(gb32)))
		return -EFAULT; 
	return 0;
}

struct rusage32 {
        struct timeval32 ru_utime;
        struct timeval32 ru_stime;
        int    ru_maxrss;
        int    ru_ixrss;
        int    ru_idrss;
        int    ru_isrss;
        int    ru_minflt;
        int    ru_majflt;
        int    ru_nswap;
        int    ru_inblock;
        int    ru_oublock;
        int    ru_msgsnd; 
        int    ru_msgrcv; 
        int    ru_nsignals;
        int    ru_nvcsw;
        int    ru_nivcsw;
};

static int
put_rusage32(struct rusage32 *ru32p, struct rusage *r)
{
	struct rusage32 r32;
#undef CP
#define CP(t) r32.t = r->t;
	CP(ru_utime.tv_sec); CP(ru_utime.tv_usec);
	CP(ru_stime.tv_sec); CP(ru_stime.tv_usec);
	CP(ru_maxrss);
	CP(ru_ixrss);
	CP(ru_idrss);
	CP(ru_isrss);
	CP(ru_minflt);
	CP(ru_majflt);
	CP(ru_nswap);
	CP(ru_inblock);
	CP(ru_oublock);
	CP(ru_msgsnd);
	CP(ru_msgrcv);
	CP(ru_nsignals);
	CP(ru_nvcsw);
	CP(ru_nivcsw);
	return copy_to_user(ru32p, &r32, sizeof r32);
}

asmlinkage int
sys32_getrusage(int who, struct rusage32 *ru)
{
	struct rusage r;
	int ret;
	extern asmlinkage int sys_getrusage(int who, struct rusage *ru);
		
	KERNEL_SYSCALL(ret, sys_getrusage, who, &r);
	if (put_rusage32(ru, &r)) return -EFAULT;
	return ret;
}

asmlinkage int
sys32_wait4(__kernel_pid_t32 pid, unsigned int * stat_addr, int options,
	    struct rusage32 * ru)
{
	if (!ru)
		return sys_wait4(pid, stat_addr, options, NULL);
	else {
		struct rusage r;
		int ret;
		unsigned int status;
	
		KERNEL_SYSCALL(ret, sys_wait4, pid, stat_addr ? &status : NULL, options, &r);
		if (put_rusage32(ru, &r)) return -EFAULT;
		if (stat_addr && put_user(status, stat_addr))
			return -EFAULT;
		return ret;
	}
}

struct stat32 {
	__kernel_dev_t32		st_dev;		/* dev_t is 32 bits on parisc */
	__kernel_ino_t32		st_ino;		/* 32 bits */
	__kernel_mode_t32	st_mode;	/* 16 bits */
	__kernel_nlink_t32		st_nlink;	/* 16 bits */
	unsigned short	st_reserved1;	/* old st_uid */
	unsigned short	st_reserved2;	/* old st_gid */
	__kernel_dev_t32		st_rdev;
	__kernel_off_t32		st_size;
	__kernel_time_t32	st_atime;
	unsigned int	st_spare1;
	__kernel_time_t32	st_mtime;
	unsigned int	st_spare2;
	__kernel_time_t32	st_ctime;
	unsigned int	st_spare3;
	int		st_blksize;
	int		st_blocks;
	unsigned int	__unused1;	/* ACL stuff */
	__kernel_dev_t32		__unused2;	/* network */
	__kernel_ino_t32		__unused3;	/* network */
	unsigned int	__unused4;	/* cnodes */
	unsigned short	__unused5;	/* netsite */
	short		st_fstype;
	__kernel_dev_t32		st_realdev;
	unsigned short	st_basemode;
	unsigned short	st_spareshort;
	__kernel_uid_t32		st_uid;
	__kernel_gid_t32		st_gid;
	unsigned int	st_spare4[3];
};

/*
 * Revalidate the inode. This is required for proper NFS attribute caching.
 */
static __inline__ int
do_revalidate(struct dentry *dentry)
{
	struct inode * inode = dentry->d_inode;
	if (inode->i_op && inode->i_op->revalidate)
		return inode->i_op->revalidate(dentry);
	return 0;
}


static int cp_new_stat32(struct inode *inode, struct stat32 *statbuf)
{
	struct stat32 tmp;
	unsigned int blocks, indirect;

	memset(&tmp, 0, sizeof(tmp));
	tmp.st_dev = kdev_t_to_nr(inode->i_dev);
	tmp.st_ino = inode->i_ino;
	tmp.st_mode = inode->i_mode;
	tmp.st_nlink = inode->i_nlink;
	SET_STAT_UID(tmp, inode->i_uid);
	SET_STAT_GID(tmp, inode->i_gid);
	tmp.st_rdev = kdev_t_to_nr(inode->i_rdev);
#if BITS_PER_LONG == 32
	if (inode->i_size > 0x7fffffff)
		return -EOVERFLOW;
#endif	
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;
/*
 * st_blocks and st_blksize are approximated with a simple algorithm if
 * they aren't supported directly by the filesystem. The minix and msdos
 * filesystems don't keep track of blocks, so they would either have to
 * be counted explicitly (by delving into the file itself), or by using
 * this simple algorithm to get a reasonable (although not 100% accurate)
 * value.
 */

/*
 * Use minix fs values for the number of direct and indirect blocks.  The
 * count is now exact for the minix fs except that it counts zero blocks.
 * Everything is in units of BLOCK_SIZE until the assignment to
 * tmp.st_blksize.
 */
#define D_B   7
#define I_B   (BLOCK_SIZE / sizeof(unsigned short))

	if (!inode->i_blksize) {
		blocks = (tmp.st_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
		if (blocks > D_B) {
			indirect = (blocks - D_B + I_B - 1) / I_B;
			blocks += indirect;
			if (indirect > 1) {
				indirect = (indirect - 1 + I_B - 1) / I_B;
				blocks += indirect;
				if (indirect > 1)
					blocks++;
			}
		}
		tmp.st_blocks = (BLOCK_SIZE / 512) * blocks;
		tmp.st_blksize = BLOCK_SIZE;
	} else {
		tmp.st_blocks = inode->i_blocks;
		tmp.st_blksize = inode->i_blksize;
	}
	return copy_to_user(statbuf,&tmp,sizeof(tmp)) ? -EFAULT : 0;
}

asmlinkage long sys32_newstat(char * filename, struct stat32 *statbuf)
{
	struct nameidata nd;
	int error;

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
	struct file * f;
	int err = -EBADF;

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

struct linux32_dirent {
	u32	d_ino;
	__kernel_off_t32	d_off;
	u16	d_reclen;
	char	d_name[1];
};

struct old_linux32_dirent {
	u32	d_ino;
	u32	d_offset;
	u16	d_namlen;
	char	d_name[1];
};

struct getdents32_callback {
	struct linux32_dirent * current_dir;
	struct linux32_dirent * previous;
	int count;
	int error;
};

struct readdir32_callback {
	struct old_linux32_dirent * dirent;
	int count;
};

#define ROUND_UP(x,a)	((__typeof__(x))(((unsigned long)(x) + ((a) - 1)) & ~((a) - 1)))
#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
static int
filldir32 (void *__buf, const char *name, int namlen, loff_t offset, ino_t ino,
	   unsigned int d_type)
{
	struct linux32_dirent * dirent;
	struct getdents32_callback * buf = (struct getdents32_callback *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1, 4);

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

asmlinkage long
sys32_getdents (unsigned int fd, void * dirent, unsigned int count)
{
	struct file * file;
	struct linux32_dirent * lastdirent;
	struct getdents32_callback buf;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = (struct linux32_dirent *) dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, filldir32, &buf);
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

static int
fillonedir32 (void * __buf, const char * name, int namlen, loff_t offset, ino_t ino,
	      unsigned int d_type)
{
	struct readdir32_callback * buf = (struct readdir32_callback *) __buf;
	struct old_linux32_dirent * dirent;

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

asmlinkage long
sys32_readdir (unsigned int fd, void * dirent, unsigned int count)
{
	int error;
	struct file * file;
	struct readdir32_callback buf;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.count = 0;
	buf.dirent = dirent;

	error = vfs_readdir(file, fillonedir32, &buf);
	if (error >= 0)
		error = buf.count;
	fput(file);
out:
	return error;
}

struct rlimit32 {
	__u32	rlim_cur;
	__u32	rlim_max;
};

#define RLIM32_INFINITY 0xffffffff

asmlinkage long sys32_getrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit32 rlim32;
	struct rlimit *rlimip;

	if (resource >= RLIM_NLIMITS)
		return -EINVAL;
	rlimip = current->rlim + resource;
	if (rlimip->rlim_cur >= RLIM32_INFINITY) {
		rlim32.rlim_cur = RLIM32_INFINITY;
	} else {
		rlim32.rlim_cur = rlimip->rlim_cur;
	}
	if (rlimip->rlim_max >= RLIM32_INFINITY) {
		rlim32.rlim_max = RLIM32_INFINITY;
	} else {
		rlim32.rlim_max = rlimip->rlim_max;
	}
	return copy_to_user(rlim, &rlim32, sizeof (struct rlimit32));
}

asmlinkage long sys32_setrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit32 rlim32;
	struct rlimit new_rlim, *old_rlim;

	if (resource >= RLIM_NLIMITS)
		return -EINVAL;
	if (copy_from_user(&rlim32, rlim, sizeof(rlim)))
		return -EFAULT;
	if (rlim32.rlim_cur == RLIM32_INFINITY) {
		new_rlim.rlim_cur = RLIM_INFINITY;
	} else {
		new_rlim.rlim_cur = rlim32.rlim_cur;
	}
	if (rlim32.rlim_max == RLIM32_INFINITY) {
		new_rlim.rlim_max = RLIM_INFINITY;
	} else {
		new_rlim.rlim_max = rlim32.rlim_max;
	}

	old_rlim = current->rlim + resource;
	if (((new_rlim.rlim_cur > old_rlim->rlim_max) ||
	     (new_rlim.rlim_max > old_rlim->rlim_max)) &&
	    !capable(CAP_SYS_RESOURCE))
		return -EPERM;
	if (resource == RLIMIT_NOFILE) {
		if (new_rlim.rlim_cur > NR_OPEN || new_rlim.rlim_max > NR_OPEN)
			return -EPERM;
	}
	if (resource == RLIMIT_STACK) {
		if (new_rlim.rlim_max > 1024 * 1024 * 1024) {
			new_rlim.rlim_max = 1024 * 1024 * 1024;
		}
		new_rlim.rlim_max = PAGE_ALIGN(new_rlim.rlim_max);
	}
	
	*old_rlim = new_rlim;
	return 0;
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

asmlinkage int sys32_mount(char *dev_name, char *dir_name, char *type, unsigned long new_flags, u32 data)
{
	unsigned long type_page = 0;
	unsigned long data_page = 0;
	unsigned long dev_page = 0;
	unsigned long dir_page = 0;
	int err, is_smb, is_ncp;

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

	err = copy_mount_stuff_to_kernel((const void *)(unsigned long)data, &data_page);
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
			panic("NCP mounts not yet supported 32/64 parisc");
			/* do_ncp_super_data_conv((void *)data_page); */
		else {
			panic("SMB mounts not yet supported 32/64 parisc");
			/* do_smb_super_data_conv((void *)data_page); */
		}

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
	return err;
}


#ifdef CONFIG_MODULES

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

asmlinkage int sys32_query_module(char *name_user, int which, char *buf, __kernel_size_t32 bufsize, __kernel_size_t32 *ret)
{
	struct module *mod;
	int err;

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

	switch (which)
	{
	case 0:
		err = 0;
		break;
	case QM_MODULES:
		err = qm_modules(buf, bufsize, ret);
		break;
	case QM_DEPS:
		err = qm_deps(mod, buf, bufsize, ret);
		break;
	case QM_REFS:
		err = qm_refs(mod, buf, bufsize, ret);
		break;
	case QM_SYMBOLS:
		err = qm_symbols(mod, buf, bufsize, ret);
		break;
	case QM_INFO:
		err = qm_info(mod, buf, bufsize, ret);
		break;
	default:
		err = -EINVAL;
		break;
	}
out:
	unlock_kernel();
	return err;
}

struct kernel_sym32 {
	u32 value;
	char name[60];
};
		 
extern asmlinkage int sys_get_kernel_syms(struct kernel_sym *table);

asmlinkage int sys32_get_kernel_syms(struct kernel_sym32 *table)
{
	int len, i;
	struct kernel_sym *tbl;
	mm_segment_t old_fs;
	
	len = sys_get_kernel_syms(NULL);
	if (!table) return len;
	tbl = kmalloc (len * sizeof (struct kernel_sym), GFP_KERNEL);
	if (!tbl) return -ENOMEM;
	old_fs = get_fs();
	set_fs (KERNEL_DS);
	sys_get_kernel_syms(tbl);
	set_fs (old_fs);
	for (i = 0; i < len; i++, table++) {
		if (put_user (tbl[i].value, &table->value) ||
		    copy_to_user (table->name, tbl[i].name, 60))
			break;
	}
	kfree (tbl);
	return i;
}

#else /* CONFIG_MODULES */

asmlinkage int
sys32_query_module(const char *name_user, int which, char *buf, size_t bufsize,
		 size_t *ret)
{
	/* Let the program know about the new interface.  Not that
	   it'll do them much good.  */
	if (which == 0)
		return 0;

	return -ENOSYS;
}

asmlinkage int
sys32_get_kernel_syms(struct kernel_sym *table)
{
	return -ENOSYS;
}

#endif  /* CONFIG_MODULES */

/* readv/writev stolen from mips64 */
struct iovec32 { unsigned int iov_base; int iov_len; };

typedef ssize_t (*IO_fn_t)(struct file *, char *, size_t, loff_t *);

static long
do_readv_writev32(int type, struct file *file, const struct iovec32 *vector,
		  u32 count)
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
	if(verify_area(VERIFY_READ, vector, sizeof(struct iovec32)*count))
		return -EFAULT;
	if (count > UIO_MAXIOV)
		return -EINVAL;
	if (count > UIO_FASTIOV) {
		iov = kmalloc(count*sizeof(struct iovec), GFP_KERNEL);
		if (!iov)
			return -ENOMEM;
	}

	tot_len = 0;
	i = count;
	ivp = iov;
	while (i > 0) {
		u32 len;
		u32 buf;

		__get_user(len, &vector->iov_len);
		__get_user(buf, &vector->iov_base);
		tot_len += len;
		ivp->iov_base = (void *)A(buf);
		ivp->iov_len = (__kernel_size_t) len;
		vector++;
		ivp++;
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

asmlinkage long
sys32_readv(int fd, struct iovec32 *vector, u32 count)
{
	struct file *file;
	ssize_t ret;

	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (file->f_op && (file->f_mode & FMODE_READ) &&
	    (file->f_op->readv || file->f_op->read))
		ret = do_readv_writev32(VERIFY_WRITE, file, vector, count);

	fput(file);

bad_file:
	return ret;
}

asmlinkage long
sys32_writev(int fd, struct iovec32 *vector, u32 count)
{
	struct file *file;
	ssize_t ret;

	ret = -EBADF;
	file = fget(fd);
	if(!file)
		goto bad_file;
	if (file->f_op && (file->f_mode & FMODE_WRITE) &&
	    (file->f_op->writev || file->f_op->write))
	        ret = do_readv_writev32(VERIFY_READ, file, vector, count);
	fput(file);

bad_file:
	return ret;
}

/********** Borrowed from sparc64 -- hardly reviewed, not tested *****/
#include <net/scm.h>
/* XXX This really belongs in some header file... -DaveM */
#define MAX_SOCK_ADDR	128		/* 108 for Unix domain - 
					   16 for IP, 16 for IPX,
					   24 for IPv6,
					   about 80 for AX.25 */

extern struct socket *sockfd_lookup(int fd, int *err);

/* XXX This as well... */
extern __inline__ void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

struct msghdr32 {
        u32               msg_name;
        int               msg_namelen;
        u32               msg_iov;
        __kernel_size_t32 msg_iovlen;
        u32               msg_control;
        __kernel_size_t32 msg_controllen;
        unsigned          msg_flags;
};

struct cmsghdr32 {
        __kernel_size_t32 cmsg_len;
        int               cmsg_level;
        int               cmsg_type;
};

/* Bleech... */
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

static inline int msghdr_from_user32_to_kern(struct msghdr *kmsg,
					     struct msghdr32 *umsg)
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
	if(kcmlen == 0)
		return -EINVAL;

	/* The kcmlen holds the 64-bit version of the control length.
	 * It may not be modified as we do not stick it into the kmsg
	 * until we have successfully copied over all of the data
	 * from the user.
	 */
	if(kcmlen > stackbuf_size)
		kcmsg_base = kcmsg = kmalloc(kcmlen, GFP_KERNEL);
	if(kcmsg == NULL)
		return -ENOBUFS;

	/* Now copy them over neatly. */
	memset(kcmsg, 0, kcmlen);
	ucmsg = CMSG32_FIRSTHDR(kmsg);
	while(ucmsg != NULL) {
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

static void put_cmsg32(struct msghdr *kmsg, int level, int type,
		       int len, void *data)
{
	struct cmsghdr32 *cm = (struct cmsghdr32 *) kmsg->msg_control;
	struct cmsghdr32 cmhdr;
	int cmlen = CMSG32_LEN(len);

	if(cm == NULL || kmsg->msg_controllen < sizeof(*cm)) {
		kmsg->msg_flags |= MSG_CTRUNC;
		return;
	}

	if(kmsg->msg_controllen < cmlen) {
		kmsg->msg_flags |= MSG_CTRUNC;
		cmlen = kmsg->msg_controllen;
	}
	cmhdr.cmsg_level = level;
	cmhdr.cmsg_type = type;
	cmhdr.cmsg_len = cmlen;

	if(copy_to_user(cm, &cmhdr, sizeof cmhdr))
		return;
	if(copy_to_user(CMSG32_DATA(cm), data, cmlen - sizeof(struct cmsghdr32)))
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

		ucmsg = (struct cmsghdr *) (((char *)ucmsg) + CMSG_ALIGN(clen64));
		wp = (((char *)kcmsg32) + CMSG32_ALIGN(clen32));
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

asmlinkage int sys32_sendmsg(int fd, struct msghdr32 *user_msg, unsigned user_flags)
{
	struct socket *sock;
	char address[MAX_SOCK_ADDR];
	struct iovec iov[UIO_FASTIOV];
	unsigned char ctl[sizeof(struct cmsghdr) + 20];
	unsigned char *ctl_buf = ctl;
	struct msghdr kern_msg;
	int err, total_len;

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EINVAL;
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
	return err;
}

asmlinkage int sys32_recvmsg(int fd, struct msghdr32 *user_msg, unsigned int user_flags)
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

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EINVAL;

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

	if(uaddr != NULL && err >= 0)
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
	return len;
}


extern asmlinkage int sys_setsockopt(int fd, int level, int optname,
				     char *optval, int optlen);

static int do_set_attach_filter(int fd, int level, int optname,
				char *optval, int optlen)
{
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

static int do_set_icmpv6_filter(int fd, int level, int optname,
				char *optval, int optlen)
{
	struct icmp6_filter kfilter;
	mm_segment_t old_fs;
	int ret, i;

	if (copy_from_user(&kfilter, optval, sizeof(kfilter)))
		return -EFAULT;


	for (i = 0; i < 8; i += 2) {
		u32 tmp = kfilter.data[i];

		kfilter.data[i] = kfilter.data[i + 1];
		kfilter.data[i + 1] = tmp;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_setsockopt(fd, level, optname,
			     (char *) &kfilter, sizeof(kfilter));
	set_fs(old_fs);

	return ret;
}


static int do_ipv4_set_replace(int fd, int level, int optname,
				char *optval, int optlen)
#if 1
/* Fields happen to be padded such that this works.
** Don't need to change iptables.h:struct ipt_replace
*/
{
	struct ipt_replace *repl = (struct ipt_replace *) optval;
	unsigned long ptr64;
	unsigned int ptr32;
	int ret;

	if (copy_from_user(&ptr32, &repl->counters, sizeof(ptr32)))
		return -EFAULT;
	ptr64 = (unsigned long) ptr32;
	if (copy_to_user(&repl->counters, &ptr64, sizeof(ptr64)))
		return -EFAULT;

	ret = sys_setsockopt(fd, level, optname, (char *) optval, optlen);

	/* Restore 32-bit ptr */
	if (copy_to_user(&repl->counters, &ptr32, sizeof(ptr32)))
		return -EFAULT;

	return ret;
}
#else
/* This version tries to "do it right". ie allocate kernel buffers for
** everything and copy data in/out. Way too complicated.
** NOT TESTED for correctness!
*/
{
	struct ipt_replace  *kern_repl;
	struct ipt_counters *kern_counters;
	unsigned int user_counters;
	mm_segment_t old_fs;
	int ret = 0;

	kern_repl = (struct ipt_replace *) kmalloc(optlen+8, GFP_KERNEL);
	if (!kern_repl)
		return -ENOMEM;

	if (copy_from_user(kern_repl, optval, optlen)) {
		ret = -EFAULT;
		goto err02;
	}

	/* 32-bit ptr is in the MSB's */
	user_counters = (unsigned int) (((unsigned long) kern_repl->counters) >> 32);
	/*
	** We are going to set_fs() to kernel space - and thus need
	** "relocate" the counters buffer to the kernel space.
	*/
	kern_counters = (struct ipt_counters *) kmalloc(kern_repl->num_counters * sizeof(struct ipt_counters), GFP_KERNEL);
	if (!user_counters) {
		ret = -ENOMEM;
		goto err02;
	}

	if (copy_from_user(kern_counters, (char *) user_counters, optlen)) {
		ret = -EFAULT;
		goto err01;
	}

	/* We can update the kernel ptr now that we have the data. */
	kern_repl->counters = kern_counters;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = sys_setsockopt(fd, level, optname, (char *) optval, optlen);

	set_fs(old_fs);

	/* Copy counters back out to user space */
	if (copy_to_user((char *) user_counters, kern_counters,
			kern_repl->num_counters * sizeof(struct ipt_counters)))
	{
		ret = -EFAULT;
		goto err01;
	}

	/* restore counters so userspace can consume it */
	kern_repl->counters = NULL;
	(unsigned int) kern_repl->counters = user_counters;

	/* Copy repl back out to user space */
	if (copy_to_user(optval, kern_repl, optlen))
	{
		ret = -EFAULT;
	}

err01:
	kfree(kern_counters);
err02:
	kfree(kern_repl);
	return ret;
}
#endif


asmlinkage int sys32_setsockopt(int fd, int level, int optname,
				char *optval, int optlen)
{
	if (optname == SO_ATTACH_FILTER)
		return do_set_attach_filter(fd, level, optname, optval, optlen);

	if (level == SOL_ICMPV6   && optname == ICMPV6_FILTER)
		return do_set_icmpv6_filter(fd, level, optname, optval, optlen);

	/*
	** Beware:    IPT_SO_SET_REPLACE == IP6T_SO_SET_REPLACE
	*/
	if (level == IPPROTO_IP   && optname == IPT_SO_SET_REPLACE)
		return do_ipv4_set_replace(fd, level, optname, optval, optlen);

	if (level == IPPROTO_IPV6 && optname == IP6T_SO_SET_REPLACE)
#if 0
		/* FIXME: I don't (yet) use IPV6. -ggg */
		return do_ipv6_set_replace(fd, level, optname, optval, optlen);
#else
	{
		BUG();
		return -ENXIO;
	}
#endif

	return sys_setsockopt(fd, level, optname, optval, optlen);
}


/*** copied from mips64 ***/
/*
 * Ooo, nasty.  We need here to frob 32-bit unsigned longs to
 * 64-bit unsigned longs.
 */

static inline int
get_fd_set32(unsigned long n, u32 *ufdset, unsigned long *fdset)
{
	n = (n + 8*sizeof(u32) - 1) / (8*sizeof(u32));
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
	n = (n + 8*sizeof(u32) - 1) / (8*sizeof(u32));

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

/*** This is a virtual copy of sys_select from fs/select.c and probably
 *** should be compared to it from time to time
 ***/
static inline void *select_bits_alloc(int size)
{
	return kmalloc(6 * size, GFP_KERNEL);
}

static inline void select_bits_free(void *bits, int size)
{
	kfree(bits);
}

/*
 * We can actually return ERESTARTSYS instead of EINTR, but I'd
 * like to be certain this leads to no problems. So I return
 * EINTR just for safety.
 *
 * Update: ERESTARTSYS breaks at least the xview clock binary, so
 * I'm trying ERESTARTNOHAND which restart only when you want to.
 */
#define MAX_SELECT_SECONDS \
	((unsigned long) (MAX_SCHEDULE_TIMEOUT / HZ)-1)
#define DIVIDE_ROUND_UP(x,y) (((x)+(y)-1)/(y))

asmlinkage long
sys32_select(int n, u32 *inp, u32 *outp, u32 *exp, struct timeval32 *tvp)
{
	fd_set_bits fds;
	char *bits;
	long timeout;
	int ret, size, err;

	timeout = MAX_SCHEDULE_TIMEOUT;
	if (tvp) {
		struct timeval32 tv32;
		time_t sec, usec;

		if ((ret = copy_from_user(&tv32, tvp, sizeof tv32)))
			goto out_nofds;

		sec = tv32.tv_sec;
		usec = tv32.tv_usec;

		ret = -EINVAL;
		if (sec < 0 || usec < 0)
			goto out_nofds;

		if ((unsigned long) sec < MAX_SELECT_SECONDS) {
			timeout = DIVIDE_ROUND_UP(usec, 1000000/HZ);
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
	bits = select_bits_alloc(size);
	if (!bits)
		goto out_nofds;
	fds.in      = (unsigned long *)  bits;
	fds.out     = (unsigned long *) (bits +   size);
	fds.ex      = (unsigned long *) (bits + 2*size);
	fds.res_in  = (unsigned long *) (bits + 3*size);
	fds.res_out = (unsigned long *) (bits + 4*size);
	fds.res_ex  = (unsigned long *) (bits + 5*size);

	if ((ret = get_fd_set32(n, inp, fds.in)) ||
	    (ret = get_fd_set32(n, outp, fds.out)) ||
	    (ret = get_fd_set32(n, exp, fds.ex)))
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
		err = put_user(sec, &tvp->tv_sec);
		err |= __put_user(usec, &tvp->tv_usec);
		if (err)
			ret = -EFAULT;
	}

	if (ret < 0)
		goto out;
	if (!ret) {
		ret = -ERESTARTNOHAND;
		if (signal_pending(current))
			goto out;
		ret = 0;
	}

	set_fd_set32(n, inp, fds.res_in);
	set_fd_set32(n, outp, fds.res_out);
	set_fd_set32(n, exp, fds.res_ex);

out:
	select_bits_free(bits, size);
out_nofds:
	return ret;
}

struct msgbuf32 {
    int mtype;
    char mtext[1];
};

asmlinkage long sys32_msgsnd(int msqid,
				struct msgbuf32 *umsgp32,
				size_t msgsz, int msgflg)
{
	struct msgbuf *mb;
	struct msgbuf32 mb32;
	int err;

	if ((mb = kmalloc(msgsz + sizeof *mb + 4, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	err = get_user(mb32.mtype, &umsgp32->mtype);
	mb->mtype = mb32.mtype;
	err |= copy_from_user(mb->mtext, &umsgp32->mtext, msgsz);

	if (err)
		err = -EFAULT;
	else
		KERNEL_SYSCALL(err, sys_msgsnd, msqid, mb, msgsz, msgflg);

	kfree(mb);
	return err;
}

asmlinkage long sys32_msgrcv(int msqid,
				struct msgbuf32 *umsgp32,
				size_t msgsz, long msgtyp, int msgflg)
{
	struct msgbuf *mb;
	struct msgbuf32 mb32;
	int err, len;

	if ((mb = kmalloc(msgsz + sizeof *mb + 4, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	KERNEL_SYSCALL(err, sys_msgrcv, msqid, mb, msgsz, msgtyp, msgflg);

	if (err >= 0) {
		len = err;
		mb32.mtype = mb->mtype;
		err = put_user(mb32.mtype, &umsgp32->mtype);
		err |= copy_to_user(&umsgp32->mtext, mb->mtext, len);
		if (err)
			err = -EFAULT;
		else
			err = len;
	}

	kfree(mb);
	return err;
}

/* LFS */

extern asmlinkage long sys_truncate(const char *, loff_t);
extern asmlinkage long sys_ftruncate(unsigned int, loff_t);
extern asmlinkage long sys_fcntl(unsigned int, unsigned int, unsigned long);
extern asmlinkage ssize_t sys_pread(unsigned int, char *, size_t, loff_t);
extern asmlinkage ssize_t sys_pwrite(unsigned int, char *, size_t, loff_t);

asmlinkage long sys32_truncate64(const char * path, unsigned int high, unsigned int low)
{
	return sys_truncate(path, (loff_t)high << 32 | low);
}

asmlinkage long sys32_ftruncate64(unsigned int fd, unsigned int high, unsigned int low)
{
	return sys_ftruncate(fd, (loff_t)high << 32 | low);
}

asmlinkage long sys32_fcntl64(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	if (cmd >= F_GETLK64 && cmd <= F_SETLKW64)
		return sys_fcntl(fd, cmd + F_GETLK - F_GETLK64, arg);
	return sys32_fcntl(fd, cmd, arg);
}

asmlinkage int sys32_pread(int fd, void *buf, size_t count, unsigned int high, unsigned int low)
{
	return sys_pread(fd, buf, count, (loff_t)high << 32 | low);
}

asmlinkage int sys32_pwrite(int fd, void *buf, size_t count, unsigned int high, unsigned int low)
{
       return sys_pwrite(fd, buf, count, (loff_t)high << 32 | low);
}

/* EXPORT/UNEXPORT */
struct nfsctl_export32 {
	char			ex_client[NFSCLNT_IDMAX+1];
	char			ex_path[NFS_MAXPATHLEN+1];
	__kernel_dev_t		ex_dev;
	__kernel_ino_t32	ex_ino;
	int			ex_flags;
	__kernel_uid_t		ex_anon_uid;
	__kernel_gid_t		ex_anon_gid;
};

/* GETFH */
struct nfsctl_fhparm32 {
	struct sockaddr		gf_addr;
	__kernel_dev_t		gf_dev;
	__kernel_ino_t32	gf_ino;
	int			gf_version;
};

/* UGIDUPDATE */
struct nfsctl_uidmap32 {
	__kernel_caddr_t32	ug_ident;
	__kernel_uid_t		ug_uidbase;
	int			ug_uidlen;
	__kernel_caddr_t32	ug_udimap;
	__kernel_gid_t		ug_gidbase;
	int			ug_gidlen;
	__kernel_caddr_t32	ug_gdimap;
};

struct nfsctl_arg32 {
	int			ca_version;	/* safeguard */
	/* wide kernel places this union on 8-byte boundary, narrow on 4 */
	union {
		struct nfsctl_svc	u_svc;
		struct nfsctl_client	u_client;
		struct nfsctl_export32	u_export;
		struct nfsctl_uidmap32	u_umap;
		struct nfsctl_fhparm32	u_getfh;
		struct nfsctl_fdparm	u_getfd;
		struct nfsctl_fsparm	u_getfs;
	} u;
};

asmlinkage int sys32_nfsservctl(int cmd, void *argp, void *resp)
{
	int ret, tmp;
	struct nfsctl_arg32 n32;
	struct nfsctl_arg n;

	ret = copy_from_user(&n, argp, sizeof n.ca_version);
	if (ret != 0)
		return ret;

	/* adjust argp to point at the union inside the user's n32 struct */
	tmp = (unsigned long)&n32.u - (unsigned long)&n32;
	argp = (void *)((unsigned long)argp + tmp);
	switch(cmd) {
	case NFSCTL_SVC:
		ret = copy_from_user(&n.u, argp, sizeof n.u.u_svc);
		break;

	case NFSCTL_ADDCLIENT:
	case NFSCTL_DELCLIENT:
		ret = copy_from_user(&n.u, argp, sizeof n.u.u_client);
		break;

	case NFSCTL_GETFD:
		ret = copy_from_user(&n.u, argp, sizeof n.u.u_getfd);
		break;

	case NFSCTL_GETFS:
		ret = copy_from_user(&n.u, argp, sizeof n.u.u_getfs);
		break;

	case NFSCTL_GETFH:		/* nfsctl_fhparm */
		ret = copy_from_user(&n32.u, argp, sizeof n32.u.u_getfh);
#undef CP
#define CP(x)	n.u.u_getfh.gf_##x = n32.u.u_getfh.gf_##x
		CP(addr);
		CP(dev);
		CP(ino);
		CP(version);
		break;

	case NFSCTL_UGIDUPDATE:		/* nfsctl_uidmap */
		ret = copy_from_user(&n32.u, argp, sizeof n32.u.u_umap);
#undef CP
#define CP(x)	n.u.u_umap.ug_##x = n32.u.u_umap.ug_##x
		n.u.u_umap.ug_ident = (char *)(u_long)n32.u.u_umap.ug_ident;
		CP(uidbase);
		CP(uidlen);
		n.u.u_umap.ug_udimap = (__kernel_uid_t *)(u_long)n32.u.u_umap.ug_udimap;
		CP(gidbase);
		CP(gidlen);
		n.u.u_umap.ug_gdimap = (__kernel_gid_t *)(u_long)n32.u.u_umap.ug_gdimap;
		break;

	case NFSCTL_UNEXPORT:		/* nfsctl_export */
	case NFSCTL_EXPORT:		/* nfsctl_export */
		ret = copy_from_user(&n32.u, argp, sizeof n32.u.u_export);
#undef CP
#define CP(x)	n.u.u_export.ex_##x = n32.u.u_export.ex_##x
		memcpy(n.u.u_export.ex_client, n32.u.u_export.ex_client, sizeof n32.u.u_export.ex_client);
		memcpy(n.u.u_export.ex_path, n32.u.u_export.ex_path, sizeof n32.u.u_export.ex_path);
		CP(dev);
		CP(ino);
		CP(flags);
		CP(anon_uid);
		CP(anon_gid);
		break;

	default:
		BUG(); /* new cmd values to be translated... */
		ret = -EINVAL;
		break;
	}

	if (ret == 0) {
		unsigned char rbuf[NFS_FHSIZE + sizeof (struct knfsd_fh)];
		KERNEL_SYSCALL(ret, sys_nfsservctl, cmd, &n, &rbuf);
		if (cmd == NFSCTL_GETFH || cmd == NFSCTL_GETFD) {
			ret = copy_to_user(resp, rbuf, NFS_FHSIZE);
		} else if (cmd == NFSCTL_GETFS) {
			ret = copy_to_user(resp, rbuf, sizeof (struct knfsd_fh));
		}
	}

	return ret;
}

#include <linux/quota.h>

struct dqblk32 {
    __u32 dqb_bhardlimit;
    __u32 dqb_bsoftlimit;
    __u32 dqb_curblocks;
    __u32 dqb_ihardlimit;
    __u32 dqb_isoftlimit;
    __u32 dqb_curinodes;
    __kernel_time_t32 dqb_btime;
    __kernel_time_t32 dqb_itime;
};
                                

asmlinkage int sys32_quotactl(int cmd, const char *special, int id, unsigned long addr)
{
	extern int sys_quotactl(int cmd, const char *special, int id, caddr_t addr);
	int cmds = cmd >> SUBCMDSHIFT;
	int err;
	struct dqblk d;
	char *spec;
	
	switch (cmds) {
	case Q_GETQUOTA:
		break;
	case Q_SETQUOTA:
	case Q_SETUSE:
	case Q_SETQLIM:
		if (copy_from_user (&d, (struct dqblk32 *)addr,
				    sizeof (struct dqblk32)))
			return -EFAULT;
		d.dqb_itime = ((struct dqblk32 *)&d)->dqb_itime;
		d.dqb_btime = ((struct dqblk32 *)&d)->dqb_btime;
		break;
	default:
		return sys_quotactl(cmd, special,
				    id, (caddr_t)addr);
	}
	spec = getname (special);
	err = PTR_ERR(spec);
	if (IS_ERR(spec)) return err;
	KERNEL_SYSCALL(err, sys_quotactl, cmd, (const char *)spec, id, (caddr_t)&d);
	putname (spec);
	if (cmds == Q_GETQUOTA) {
		__kernel_time_t b = d.dqb_btime, i = d.dqb_itime;
		((struct dqblk32 *)&d)->dqb_itime = i;
		((struct dqblk32 *)&d)->dqb_btime = b;
		if (copy_to_user ((struct dqblk32 *)addr, &d,
				  sizeof (struct dqblk32)))
			return -EFAULT;
	}
	return err;
}

struct timex32 {
	unsigned int modes;	/* mode selector */
	int offset;		/* time offset (usec) */
	int freq;		/* frequency offset (scaled ppm) */
	int maxerror;		/* maximum error (usec) */
	int esterror;		/* estimated error (usec) */
	int status;		/* clock command/status */
	int constant;		/* pll time constant */
	int precision;		/* clock precision (usec) (read only) */
	int tolerance;		/* clock frequency tolerance (ppm)
				 * (read only)
				 */
	struct timeval32 time;	/* (read only) */
	int tick;		/* (modified) usecs between clock ticks */

	int ppsfreq;           /* pps frequency (scaled ppm) (ro) */
	int jitter;            /* pps jitter (us) (ro) */
	int shift;              /* interval duration (s) (shift) (ro) */
	int stabil;            /* pps stability (scaled ppm) (ro) */
	int jitcnt;            /* jitter limit exceeded (ro) */
	int calcnt;            /* calibration intervals (ro) */
	int errcnt;            /* calibration errors (ro) */
	int stbcnt;            /* stability limit exceeded (ro) */

	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
};

asmlinkage long sys32_adjtimex(struct timex32 *txc_p32)
{
	struct timex txc;
	struct timex32 t32;
	int ret;
	extern int do_adjtimex(struct timex *txc);

	if(copy_from_user(&t32, txc_p32, sizeof(struct timex32)))
		return -EFAULT;
#undef CP
#define CP(x) txc.x = t32.x
	CP(modes); CP(offset); CP(freq); CP(maxerror); CP(esterror);
	CP(status); CP(constant); CP(precision); CP(tolerance);
	CP(time.tv_sec); CP(time.tv_usec); CP(tick); CP(ppsfreq); CP(jitter);
	CP(shift); CP(stabil); CP(jitcnt); CP(calcnt); CP(errcnt);
	CP(stbcnt);
	ret = do_adjtimex(&txc);
#define CP(x) t32.x = txc.x
	CP(modes); CP(offset); CP(freq); CP(maxerror); CP(esterror);
	CP(status); CP(constant); CP(precision); CP(tolerance);
	CP(time.tv_sec); CP(time.tv_usec); CP(tick); CP(ppsfreq); CP(jitter);
	CP(shift); CP(stabil); CP(jitcnt); CP(calcnt); CP(errcnt);
	CP(stbcnt);
	return copy_to_user(txc_p32, &t32, sizeof(struct timex32)) ? -EFAULT : ret;
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
	u32 totalhigh;
	u32 freehigh;
	u32 mem_unit;
	char _f[12];
};

/* We used to call sys_sysinfo and translate the result.  But sys_sysinfo
 * undoes the good work done elsewhere, and rather than undoing the
 * damage, I decided to just duplicate the code from sys_sysinfo here.
 */

asmlinkage int sys32_sysinfo(struct sysinfo32 *info)
{
	struct sysinfo val;
	int err;

	/* We don't need a memset here because we copy the
	 * struct to userspace once element at a time.
	 */

	cli();
	val.uptime = jiffies / HZ;

	val.loads[0] = avenrun[0] << (SI_LOAD_SHIFT - FSHIFT);
	val.loads[1] = avenrun[1] << (SI_LOAD_SHIFT - FSHIFT);
	val.loads[2] = avenrun[2] << (SI_LOAD_SHIFT - FSHIFT);

	val.procs = nr_threads-1;
	sti();

	si_meminfo(&val);
	si_swapinfo(&val);
	
	err = put_user (val.uptime, &info->uptime);
	err |= __put_user (val.loads[0], &info->loads[0]);
	err |= __put_user (val.loads[1], &info->loads[1]);
	err |= __put_user (val.loads[2], &info->loads[2]);
	err |= __put_user (val.totalram, &info->totalram);
	err |= __put_user (val.freeram, &info->freeram);
	err |= __put_user (val.sharedram, &info->sharedram);
	err |= __put_user (val.bufferram, &info->bufferram);
	err |= __put_user (val.totalswap, &info->totalswap);
	err |= __put_user (val.freeswap, &info->freeswap);
	err |= __put_user (val.procs, &info->procs);
	err |= __put_user (val.totalhigh, &info->totalhigh);
	err |= __put_user (val.freehigh, &info->freehigh);
	err |= __put_user (val.mem_unit, &info->mem_unit);
	return err ? -EFAULT : 0;
}


/* lseek() needs a wrapper because 'offset' can be negative, but the top
 * half of the argument has been zeroed by syscall.S.
 */

extern asmlinkage off_t sys_lseek(unsigned int fd, off_t offset, unsigned int origin);

asmlinkage int sys32_lseek(unsigned int fd, int offset, unsigned int origin)
{
	return sys_lseek(fd, offset, origin);
}

asmlinkage long sys32_semctl_broken(int semid, int semnum, int cmd, union semun arg)
{
        union semun u;
	
	cmd &= ~IPC_64; /* should be removed together with the _broken suffix */

        if (cmd == SETVAL) {
                /* Ugh.  arg is a union of int,ptr,ptr,ptr, so is 8 bytes.
                 * The int should be in the first 4, but our argument
                 * frobbing has left it in the last 4.
                 */
                u.val = *((int *)&arg + 1);
                return sys_semctl (semid, semnum, cmd, u);
	}
	return sys_semctl (semid, semnum, cmd, arg);
}

