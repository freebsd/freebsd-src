/*
 * Conversion between 32-bit and 64-bit native system calls.
 *
 * Copyright (C) 2000 Silicon Graphics, Inc.
 * Written by Ulf Carlsson (ulfc@engr.sgi.com)
 * sys32_execve from ia64/ia32 code, Feb 2000, Kanoj Sarcar (kanoj@sgi.com)
 */
#include <linux/config.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/smp_lock.h>
#include <linux/highuid.h>
#include <linux/dirent.h>
#include <linux/resource.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/filter.h>
#include <linux/shm.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/icmpv6.h>
#include <linux/sysctl.h>
#include <linux/utime.h>
#include <linux/utsname.h>
#include <linux/personality.h>
#include <linux/timex.h>
#include <linux/dnotify.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <net/sock.h>
#include <net/scm.h>

#include <asm/uaccess.h>
#include <asm/mman.h>
#include <asm/ipc.h>

extern asmlinkage long sys_socket(int family, int type, int protocol);
extern asmlinkage long sys_bind(int fd, struct sockaddr *umyaddr, int addrlen);
extern asmlinkage long sys_connect(int fd, struct sockaddr *uservaddr,
	int addrlen);
extern asmlinkage long sys_listen(int fd, int backlog);
extern asmlinkage long sys_accept(int fd, struct sockaddr *upeer_sockaddr,
	int *upeer_addrlen);
extern asmlinkage long sys_getsockname(int fd, struct sockaddr *usockaddr,
	int *usockaddr_len);
extern asmlinkage long sys_getpeername(int fd, struct sockaddr *usockaddr,
	int *usockaddr_len);
extern asmlinkage long sys_socketpair(int family, int type, int protocol,
	int *usockvec);
extern asmlinkage long sys_send(int fd, void * buff, size_t len,
	unsigned flags);
extern asmlinkage long sys_sendto(int fd, void * buff, size_t len,
	unsigned flags, struct sockaddr *addr, int addr_len);
extern asmlinkage long sys_recv(int fd, void * ubuf, size_t size,
	unsigned flags);
extern asmlinkage long sys_recvfrom(int fd, void * ubuf, size_t size,
	unsigned flags, struct sockaddr *addr, int *addr_len);
extern asmlinkage long sys_shutdown(int fd, int how);
extern asmlinkage long sys_setsockopt(int fd, int level, int optname,
	char *optval, int optlen);
extern asmlinkage long sys_getsockopt(int fd, int level, int optname,
	char *optval, int *optlen);
extern asmlinkage long sys_sendmsg(int fd, struct msghdr *msg, unsigned flags);
extern asmlinkage long sys_recvmsg(int fd, struct msghdr *msg,
	unsigned int flags);


/* Use this to get at 32-bit user passed pointers. */
/* A() macro should be used for places where you e.g.
   have some internal variable u32 and just want to get
   rid of a compiler warning. AA() has to be used in
   places where you want to convert a function argument
   to 32bit pointer or when you e.g. access pt_regs
   structure and want to consider 32bit registers only.
 */
#define A(__x) ((unsigned long)(__x))
#define AA(__x) ((unsigned long)((int)__x))

#ifdef __MIPSEB__
#define merge_64(r1,r2)	((((r1) & 0xffffffffUL) << 32) + ((r2) & 0xffffffffUL))
#endif
#ifdef __MIPSEL__
#define merge_64(r1,r2)	((((r2) & 0xffffffffUL) << 32) + ((r1) & 0xffffffffUL))
#endif

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

static int cp_new_stat32(struct inode * inode, struct stat32 * statbuf)
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
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;

	/*
	 * st_blocks and st_blksize are approximated with a simple algorithm if
	 * they aren't supported directly by the filesystem. The minix and msdos
	 * filesystems don't keep track of blocks, so they would either have to
	 * be counted explicitly (by delving into the file itself), or by using
	 * this simple algorithm to get a reasonable (although not 100%
	 * accurate) value.
	 */

	/*
	 * Use minix fs values for the number of direct and indirect blocks.
	 * The count is now exact for the minix fs except that it counts zero
	 * blocks.  Everything is in units of BLOCK_SIZE until the assignment
	 * to tmp.st_blksize.
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

asmlinkage int sys32_newstat(char * filename, struct stat32 *statbuf)
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

asmlinkage int sys32_newlstat(char * filename, struct stat32 *statbuf)
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

asmlinkage long sys32_newfstat(unsigned int fd, struct stat32 * statbuf)
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

asmlinkage unsigned long
sys32_mmap2(unsigned long addr, size_t len, unsigned long prot,
         unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	struct file * file = NULL;
	unsigned long error;

	error = -EINVAL;
	if (!(flags & MAP_ANONYMOUS)) {
		error = -EBADF;
		file = fget(fd);
		if (!file)
			goto out;
	}
	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);
	if (file)
		fput(file);

out:
	return error;
}


asmlinkage long sys_truncate(const char * path, unsigned long length);

asmlinkage int sys_truncate64(const char *path, unsigned int high,
			      unsigned int low)
{
	if ((int)high < 0)
		return -EINVAL;
	return sys_truncate(path, ((long) high << 32) | low);
}

asmlinkage long sys_ftruncate(unsigned int fd, unsigned long length);

asmlinkage int sys_ftruncate64(unsigned int fd, unsigned int high,
			       unsigned int low)
{
	if ((int)high < 0)
		return -EINVAL;
	return sys_ftruncate(fd, ((long) high << 32) | low);
}

extern asmlinkage int sys_utime(char * filename, struct utimbuf * times);

struct utimbuf32 {
	__kernel_time_t32 actime, modtime;
};

asmlinkage int sys32_utime(char * filename, struct utimbuf32 *times)
{
	struct utimbuf t;
	mm_segment_t old_fs;
	int ret;
	char *filenam;

	if (!times)
		return sys_utime(filename, NULL);
	if (get_user (t.actime, &times->actime) ||
	    __get_user (t.modtime, &times->modtime))
		return -EFAULT;
	filenam = getname (filename);
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

#if 0
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
 * 'copy_strings32()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 */
int copy_strings32(int argc, u32 * argv, struct linux_binprm *bprm)
{
	while (argc-- > 0) {
		u32 str;
		int len;
		unsigned long pos;

		if (get_user(str, argv+argc) || !str ||
		     !(len = strnlen_user((char *)A(str), bprm->p)))
			return -EFAULT;
		if (bprm->p < len)
			return -E2BIG;

		bprm->p -= len;
		/* XXX: add architecture specific overflow check here. */

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
			kaddr = kmap(page);

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
 * sys_execve32() executes a new program.
 */
int do_execve32(char * filename, u32 * argv, u32 * envp, struct pt_regs * regs)
{
	struct linux_binprm bprm;
	struct dentry * dentry;
	int retval;
	int i;

	bprm.p = PAGE_SIZE*MAX_ARG_PAGES-sizeof(void *);
	memset(bprm.page, 0, MAX_ARG_PAGES*sizeof(bprm.page[0]));

	dentry = open_namei(filename, 0, 0);
	retval = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		return retval;

	bprm.dentry = dentry;
	bprm.filename = filename;
	bprm.sh_bang = 0;
	bprm.loader = 0;
	bprm.exec = 0;
	if ((bprm.argc = count32(argv, bprm.p / sizeof(u32))) < 0) {
		dput(dentry);
		return bprm.argc;
	}

	if ((bprm.envc = count32(envp, bprm.p / sizeof(u32))) < 0) {
		dput(dentry);
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
	if (bprm.dentry)
		dput(bprm.dentry);

	/* Assumes that free_page() can take a NULL argument. */
	/* I hope this is ok for all architectures */
	for (i = 0 ; i < MAX_ARG_PAGES ; i++)
		if (bprm.page[i])
			__free_page(bprm.page[i]);

	return retval;
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys32_execve(abi64_no_regargs, struct pt_regs regs)
{
	int error;
	char * filename;

	filename = getname((char *) (long)regs.regs[4]);
	printk("Executing: %s\n", filename);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve32(filename, (u32 *) (long)regs.regs[5],
	                  (u32 *) (long)regs.regs[6], &regs);
	putname(filename);

out:
	return error;
}
#else
static int nargs(unsigned int arg, char **ap)
{
	unsigned int addr;
	int n, err;

	if (!arg)
		return 0;

	n = 0;
	do {
		err = get_user(addr, (unsigned int *)A(arg));
		if (err)
			return err;
		if (ap)
			*ap++ = (char *) A(addr);
		arg += sizeof(unsigned int);
		n++;
	} while (addr);
	return n - 1;
}

asmlinkage int
sys32_execve(abi64_no_regargs, struct pt_regs regs)
{
	extern asmlinkage int sys_execve(abi64_no_regargs, struct pt_regs regs);
	extern asmlinkage long sys_munmap(unsigned long addr, size_t len);
	unsigned int argv = (unsigned int)regs.regs[5];
	unsigned int envp = (unsigned int)regs.regs[6];
	char **av, **ae;
	int na, ne, r, len;
	char * filename;

	na = nargs(argv, NULL);
	if (na < 0)
		return na;
	ne = nargs(envp, NULL);
	if (ne < 0)
		return ne;
	len = (na + ne + 2) * sizeof(*av);

	/*
	 *  kmalloc won't work because the `sys_exec' code will attempt
	 *  to do a `get_user' on the arg list and `get_user' will fail
	 *  on a kernel address (simplifies `get_user').  Instead we
	 *  do an mmap to get a user address.  Note that since a successful
	 *  `execve' frees all current memory we only have to do an
	 *  `munmap' if the `execve' fails.
	 */
	down_write(&current->mm->mmap_sem);
	av = (char **) do_mmap_pgoff(0, 0, len, PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_ANONYMOUS, 0);
	up_write(&current->mm->mmap_sem);

	if (IS_ERR(av))
		return (long) av;
	ae = av + na + 1;
	r = __put_user(0, (av + na));
	r |= __put_user(0, (ae + ne));
	if (r)
		goto out;
	r = nargs(argv, av);
	if (r < 0)
		goto out;
	r = nargs(envp, ae);
	if (r < 0)
		goto out;
	filename = getname((char *) (long)regs.regs[4]);
	r = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	r = do_execve(filename, av, ae, &regs);
	putname(filename);
	if (r)
out:
		sys_munmap((unsigned long)av, len);
	return r ;
}
#endif

struct dirent32 {
	unsigned int	d_ino;
	unsigned int	d_off;
	unsigned short	d_reclen;
	char		d_name[NAME_MAX + 1];
};

static void
xlate_dirent(void *dirent64, void *dirent32, long n)
{
	long off;
	struct dirent *dirp;
	struct dirent32 *dirp32;

	off = 0;
	while (off < n) {
		dirp = (struct dirent *)(dirent64 + off);
		dirp32 = (struct dirent32 *)(dirent32 + off);
		off += dirp->d_reclen;
		dirp32->d_ino = dirp->d_ino;
		dirp32->d_off = (unsigned int)dirp->d_off;
		dirp32->d_reclen = dirp->d_reclen;
		strncpy(dirp32->d_name, dirp->d_name, dirp->d_reclen - ((3 * 4) + 2));
	}
	return;
}

asmlinkage long sys_getdents(unsigned int fd, void * dirent, unsigned int count);

asmlinkage long
sys32_getdents(unsigned int fd, void * dirent32, unsigned int count)
{
	long n;
	void *dirent64;

	dirent64 = (void *)((unsigned long)(dirent32 + (sizeof(long) - 1)) & ~(sizeof(long) - 1));
	if ((n = sys_getdents(fd, dirent64, count - (dirent64 - dirent32))) < 0)
		return(n);
	xlate_dirent(dirent64, dirent32, n);
	return(n);
}

asmlinkage int old_readdir(unsigned int fd, void * dirent, unsigned int count);

asmlinkage int
sys32_readdir(unsigned int fd, void * dirent32, unsigned int count)
{
	int n;
	struct dirent dirent64;

	if ((n = old_readdir(fd, &dirent64, count)) < 0)
		return(n);
	xlate_dirent(&dirent64, dirent32, dirent64.d_reclen);
	return(n);
}

struct timeval32
{
    int tv_sec, tv_usec;
};

struct itimerval32
{
    struct timeval32 it_interval;
    struct timeval32 it_value;
};

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
put_rusage (struct rusage32 *ru, struct rusage *r)
{
	int err;

	if (verify_area(VERIFY_WRITE, ru, sizeof *ru))
		return -EFAULT;

	err = __put_user (r->ru_utime.tv_sec, &ru->ru_utime.tv_sec);
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
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);
		ret = sys_wait4(pid, stat_addr ? &status : NULL, options, &r);
		set_fs(old_fs);
		if (put_rusage (ru, &r)) return -EFAULT;
		if (stat_addr && put_user (status, stat_addr))
			return -EFAULT;
		return ret;
	}
}

asmlinkage int
sys32_waitpid(__kernel_pid_t32 pid, unsigned int *stat_addr, int options)
{
	return sys32_wait4(pid, stat_addr, options, NULL);
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
        u16 procs;
	u32 totalhigh;
	u32 freehigh;
	u32 mem_unit;
	char _f[8];
};

extern asmlinkage int sys_sysinfo(struct sysinfo *info);

asmlinkage int sys32_sysinfo(struct sysinfo32 *info)
{
	struct sysinfo s;
	int ret, err;
	mm_segment_t old_fs = get_fs ();
	
	set_fs (KERNEL_DS);
	ret = sys_sysinfo(&s);
	set_fs (old_fs);
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
	return ret;
}

#define RLIM_INFINITY32	0x7fffffff
#define RESOURCE32(x) ((x > RLIM_INFINITY32) ? RLIM_INFINITY32 : x)

struct rlimit32 {
	int	rlim_cur;
	int	rlim_max;
};

extern asmlinkage int sys_old_getrlimit(unsigned int resource, struct rlimit *rlim);

asmlinkage int
sys32_getrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	int ret;
	mm_segment_t old_fs = get_fs ();

	set_fs (KERNEL_DS);
	ret = sys_old_getrlimit(resource, &r);
	set_fs (old_fs);
	if (!ret) {
		ret = put_user (RESOURCE32(r.rlim_cur), &rlim->rlim_cur);
		ret |= __put_user (RESOURCE32(r.rlim_max), &rlim->rlim_max);
	}
	return ret;
}

extern asmlinkage int sys_setrlimit(unsigned int resource, struct rlimit *rlim);

asmlinkage int
sys32_setrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	int ret;
	mm_segment_t old_fs = get_fs ();

	if (resource >= RLIM_NLIMITS) return -EINVAL;
	if (get_user (r.rlim_cur, &rlim->rlim_cur) ||
	    __get_user (r.rlim_max, &rlim->rlim_max))
		return -EFAULT;
	if (r.rlim_cur == RLIM_INFINITY32)
		r.rlim_cur = RLIM_INFINITY;
	if (r.rlim_max == RLIM_INFINITY32)
		r.rlim_max = RLIM_INFINITY;
	set_fs (KERNEL_DS);
	ret = sys_setrlimit(resource, &r);
	set_fs (old_fs);
	return ret;
}

struct statfs32 {
	int	f_type;
	int	f_bsize;
	int	f_frsize;
	int	f_blocks;
	int	f_bfree;
	int	f_files;
	int	f_ffree;
	int	f_bavail;
	__kernel_fsid_t32	f_fsid;
	int	f_namelen;
	int	f_spare[6];
};

static inline int
put_statfs (struct statfs32 *ubuf, struct statfs *kbuf)
{
	int err;

	err = put_user (kbuf->f_type, &ubuf->f_type);
	err |= __put_user (kbuf->f_bsize, &ubuf->f_bsize);
	err |= __put_user (kbuf->f_blocks, &ubuf->f_blocks);
	err |= __put_user (kbuf->f_bfree, &ubuf->f_bfree);
	err |= __put_user (kbuf->f_bavail, &ubuf->f_bavail);
	err |= __put_user (kbuf->f_files, &ubuf->f_files);
	err |= __put_user (kbuf->f_ffree, &ubuf->f_ffree);
	err |= __put_user (kbuf->f_namelen, &ubuf->f_namelen);
	err |= __put_user (kbuf->f_fsid.val[0], &ubuf->f_fsid.val[0]);
	err |= __put_user (kbuf->f_fsid.val[1], &ubuf->f_fsid.val[1]);
	return err;
}

extern asmlinkage int sys_statfs(const char * path, struct statfs * buf);

asmlinkage int
sys32_statfs(const char * path, struct statfs32 *buf)
{
	int ret;
	struct statfs s;
	mm_segment_t old_fs = get_fs();
	char *pth;
	
	pth = getname (path);
	ret = PTR_ERR(pth);
	if (!IS_ERR(pth)) {
		set_fs (KERNEL_DS);
		ret = sys_statfs((const char *)path, &s);
		set_fs (old_fs);
		if (!ret && put_statfs(buf, &s))
			return -EFAULT;
	}
	return ret;
}

extern asmlinkage int sys_fstatfs(unsigned int fd, struct statfs * buf);

asmlinkage int
sys32_fstatfs(unsigned int fd, struct statfs32 *buf)
{
	int ret;
	struct statfs s;
	mm_segment_t old_fs = get_fs();

	set_fs (KERNEL_DS);
	ret = sys_fstatfs(fd, &s);
	set_fs (old_fs);
	if (put_statfs(buf, &s))
		return -EFAULT;
	return ret;
}

#ifdef __MIPSEB__
asmlinkage long sys32_truncate64(const char * path, unsigned long __dummy,
	int length_hi, int length_lo)
#endif
#ifdef __MIPSEL__
asmlinkage long sys32_truncate64(const char * path, unsigned long __dummy,
	int length_lo, int length_hi)
#endif
{
	loff_t length;

	length = ((unsigned long) length_hi << 32) | (unsigned int) length_lo;

	return sys_truncate(path, length);
}

#ifdef __MIPSEB__
asmlinkage long sys32_ftruncate64(unsigned int fd, unsigned long __dummy,
	int length_hi, int length_lo)
#endif
#ifdef __MIPSEL__
asmlinkage long sys32_ftruncate64(unsigned int fd, unsigned long __dummy,
	int length_lo, int length_hi)
#endif
{
	loff_t length;

	length = ((unsigned long) length_hi << 32) | (unsigned int) length_lo;

	return sys_ftruncate(fd, length);
}

extern asmlinkage int
sys_getrusage(int who, struct rusage *ru);

asmlinkage int
sys32_getrusage(int who, struct rusage32 *ru)
{
	struct rusage r;
	int ret;
	mm_segment_t old_fs = get_fs();

	set_fs (KERNEL_DS);
	ret = sys_getrusage(who, &r);
	set_fs (old_fs);
	if (put_rusage (ru, &r))
		return -EFAULT;

	return ret;
}

static inline long
get_tv32(struct timeval *o, struct timeval32 *i)
{
	return (!access_ok(VERIFY_READ, i, sizeof(*i)) ||
		(__get_user(o->tv_sec, &i->tv_sec) |
		 __get_user(o->tv_usec, &i->tv_usec)));
}

static inline long
get_it32(struct itimerval *o, struct itimerval32 *i)
{
	return (!access_ok(VERIFY_READ, i, sizeof(*i)) ||
		(__get_user(o->it_interval.tv_sec, &i->it_interval.tv_sec) |
		 __get_user(o->it_interval.tv_usec, &i->it_interval.tv_usec) |
		 __get_user(o->it_value.tv_sec, &i->it_value.tv_sec) |
		 __get_user(o->it_value.tv_usec, &i->it_value.tv_usec)));
}

static inline long
put_tv32(struct timeval32 *o, struct timeval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->tv_sec, &o->tv_sec) |
		 __put_user(i->tv_usec, &o->tv_usec)));
}

static inline long
put_it32(struct itimerval32 *o, struct itimerval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->it_interval.tv_sec, &o->it_interval.tv_sec) |
		 __put_user(i->it_interval.tv_usec, &o->it_interval.tv_usec) |
		 __put_user(i->it_value.tv_sec, &o->it_value.tv_sec) |
		 __put_user(i->it_value.tv_usec, &o->it_value.tv_usec)));
}

extern int do_getitimer(int which, struct itimerval *value);

asmlinkage int
sys32_getitimer(int which, struct itimerval32 *it)
{
	struct itimerval kit;
	int error;

	error = do_getitimer(which, &kit);
	if (!error && put_it32(it, &kit))
		error = -EFAULT;

	return error;
}

extern int do_setitimer(int which, struct itimerval *, struct itimerval *);


asmlinkage int
sys32_setitimer(int which, struct itimerval32 *in, struct itimerval32 *out)
{
	struct itimerval kin, kout;
	int error;

	if (in) {
		if (get_it32(&kin, in))
			return -EFAULT;
	} else
		memset(&kin, 0, sizeof(kin));

	error = do_setitimer(which, &kin, out ? &kout : NULL);
	if (error || !out)
		return error;
	if (put_it32(out, &kout))
		return -EFAULT;

	return 0;
}

/* Translations due to time_t size differences.  Which affects all
   sorts of things, like timeval and itimerval.  */


extern struct timezone sys_tz;
extern int do_sys_settimeofday(struct timeval *tv, struct timezone *tz);

asmlinkage int
sys32_gettimeofday(struct timeval32 *tv, struct timezone *tz)
{
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

asmlinkage int
sys32_settimeofday(struct timeval32 *tv, struct timezone *tz)
{
	struct timeval ktv;
	struct timezone ktz;

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

extern asmlinkage long sys_llseek(unsigned int fd, unsigned long offset_high,
			          unsigned long offset_low, loff_t * result,
			          unsigned int origin);

asmlinkage int sys32_llseek(unsigned int fd, unsigned int offset_high,
			    unsigned int offset_low, loff_t * result,
			    unsigned int origin)
{
	return sys_llseek(fd, offset_high, offset_low, result, origin);
}

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

/* From the Single Unix Spec: pread & pwrite act like lseek to pos + op +
   lseek back to original location.  They fail just like lseek does on
   non-seekable files.  */

asmlinkage ssize_t sys32_pread(unsigned int fd, char * buf,
			       size_t count, u32 unused, u64 a4, u64 a5)
{
	ssize_t ret;
	struct file * file;
	ssize_t (*read)(struct file *, char *, size_t, loff_t *);
	loff_t pos;

	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (!(file->f_mode & FMODE_READ))
		goto out;
	pos = merge_64(a4, a5);
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

asmlinkage ssize_t sys32_pwrite(unsigned int fd, const char * buf,
			        size_t count, u32 unused, u64 a4, u64 a5)
{
	ssize_t ret;
	struct file * file;
	ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
	loff_t pos;

	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad_file;
	if (!(file->f_mode & FMODE_WRITE))
		goto out;
	pos = merge_64(a4, a5);
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

asmlinkage int sys32_select(int n, u32 *inp, u32 *outp, u32 *exp, struct timeval32 *tvp)
{
	fd_set_bits fds;
	char *bits;
	unsigned long nn;
	long timeout;
	int ret, size;

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
	return ret;
}



struct timespec32 {
	int 	tv_sec;
	int	tv_nsec;
};

extern asmlinkage int sys_sched_rr_get_interval(pid_t pid,
						struct timespec *interval);

asmlinkage int
sys32_sched_rr_get_interval(__kernel_pid_t32 pid, struct timespec32 *interval)
{
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs ();

	set_fs (KERNEL_DS);
	ret = sys_sched_rr_get_interval(pid, &t);
	set_fs (old_fs);
	if (put_user (t.tv_sec, &interval->tv_sec) ||
	    __put_user (t.tv_nsec, &interval->tv_nsec))
		return -EFAULT;
	return ret;
}


extern asmlinkage int sys_nanosleep(struct timespec *rqtp,
				    struct timespec *rmtp);

asmlinkage int
sys32_nanosleep(struct timespec32 *rqtp, struct timespec32 *rmtp)
{
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs ();

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

struct tms32 {
	int tms_utime;
	int tms_stime;
	int tms_cutime;
	int tms_cstime;
};

extern asmlinkage long sys_times(struct tms * tbuf);
asmlinkage long sys32_times(struct tms32 *tbuf)
{
	struct tms t;
	long ret;
	mm_segment_t old_fs = get_fs();
	int err;

	set_fs(KERNEL_DS);
	ret = sys_times(tbuf ? &t : NULL);
	set_fs(old_fs);
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

asmlinkage int sys32_setsockopt(int fd, int level, int optname,
				char *optval, int optlen)
{
	if (optname == SO_ATTACH_FILTER)
		return do_set_attach_filter(fd, level, optname,
					    optval, optlen);
	if (level == SOL_ICMPV6 && optname == ICMPV6_FILTER)
		return do_set_icmpv6_filter(fd, level, optname,
					    optval, optlen);

	return sys_setsockopt(fd, level, optname, optval, optlen);
}

static inline int get_flock(struct flock *kfl, struct flock32 *ufl)
{
	int err;

	if (!access_ok(VERIFY_READ, ufl, sizeof(*ufl)))
		return -EFAULT;

	err = __get_user(kfl->l_type, &ufl->l_type);
	err |= __get_user(kfl->l_whence, &ufl->l_whence);
	err |= __get_user(kfl->l_start, &ufl->l_start);
	err |= __get_user(kfl->l_len, &ufl->l_len);
	err |= __get_user(kfl->l_pid, &ufl->l_pid);

	return err;
}

static inline int put_flock(struct flock *kfl, struct flock32 *ufl)
{
	int err;

	if (!access_ok(VERIFY_WRITE, ufl, sizeof(*ufl)))
		return -EFAULT;

	err = __put_user(kfl->l_type, &ufl->l_type);
	err |= __put_user(kfl->l_whence, &ufl->l_whence);
	err |= __put_user(kfl->l_start, &ufl->l_start);
	err |= __put_user(kfl->l_len, &ufl->l_len);
	err |= __put_user(0, &ufl->l_sysid);
	err |= __put_user(kfl->l_pid, &ufl->l_pid);

	return err;
}

extern asmlinkage long
sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);

asmlinkage long
sys32_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		{
			struct flock f;
			mm_segment_t old_fs;
			long ret;

			if (get_flock(&f, (struct flock32 *)arg))
				return -EFAULT;
			old_fs = get_fs(); set_fs (KERNEL_DS);
			ret = sys_fcntl(fd, cmd, (unsigned long)&f);
			set_fs (old_fs);
			if (put_flock(&f, (struct flock32 *)arg))
				return -EFAULT;
			return ret;
		}
	default:
		return sys_fcntl(fd, cmd, (unsigned long)arg);
	}
}

asmlinkage long
sys32_fcntl64(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case F_GETLK64:
		return sys_fcntl(fd, F_GETLK, arg);
	case F_SETLK64:
		return sys_fcntl(fd, F_SETLK, arg);
	case F_SETLKW64:
		return sys_fcntl(fd, F_SETLKW, arg);
	}

	return sys32_fcntl(fd, cmd, arg);
}

struct msgbuf32 { s32 mtype; char mtext[1]; };

struct ipc_perm32
{
	key_t    	  key;
        __kernel_uid_t32  uid;
        __kernel_gid_t32  gid;
        __kernel_uid_t32  cuid;
        __kernel_gid_t32  cgid;
        __kernel_mode_t32 mode;
        unsigned short  seq;
};

struct ipc64_perm32 {
	key_t key;
	__kernel_uid_t32 uid;
	__kernel_gid_t32 gid;
	__kernel_uid_t32 cuid;
	__kernel_gid_t32 cgid;
	__kernel_mode_t32 mode; 
	unsigned short seq;
	unsigned short __pad1;
	unsigned int __unused1;
	unsigned int __unused2;
};

struct semid_ds32 {
        struct ipc_perm32 sem_perm;               /* permissions .. see ipc.h */
        __kernel_time_t32 sem_otime;              /* last semop time */
        __kernel_time_t32 sem_ctime;              /* last change time */
        u32 sem_base;              /* ptr to first semaphore in array */
        u32 sem_pending;          /* pending operations to be processed */
        u32 sem_pending_last;    /* last pending operation */
        u32 undo;                  /* undo requests on this array */
        unsigned short  sem_nsems;              /* no. of semaphores in array */
};

struct semid64_ds32 {
	struct ipc64_perm32 sem_perm;
	__kernel_time_t32 sem_otime;
	__kernel_time_t32 sem_ctime;
	unsigned int sem_nsems;
	unsigned int __unused1;
	unsigned int __unused2;
};

struct msqid_ds32
{
        struct ipc_perm32 msg_perm;
        u32 msg_first;
        u32 msg_last;
        __kernel_time_t32 msg_stime;
        __kernel_time_t32 msg_rtime;
        __kernel_time_t32 msg_ctime;
        u32 wwait;
        u32 rwait;
        unsigned short msg_cbytes;
        unsigned short msg_qnum;
        unsigned short msg_qbytes;
        __kernel_ipc_pid_t32 msg_lspid;
        __kernel_ipc_pid_t32 msg_lrpid;
};

struct msqid64_ds32 {
	struct ipc64_perm32 msg_perm;
	__kernel_time_t32 msg_stime;
	unsigned int __unused1;
	__kernel_time_t32 msg_rtime;
	unsigned int __unused2;
	__kernel_time_t32 msg_ctime;
	unsigned int __unused3;
	unsigned int msg_cbytes;
	unsigned int msg_qnum;
	unsigned int msg_qbytes;
	__kernel_pid_t32 msg_lspid;
	__kernel_pid_t32 msg_lrpid;
	unsigned int __unused4;
	unsigned int __unused5;
};

struct shmid_ds32 {
        struct ipc_perm32       shm_perm;
        int                     shm_segsz;
        __kernel_time_t32       shm_atime;
        __kernel_time_t32       shm_dtime;
        __kernel_time_t32       shm_ctime;
        __kernel_ipc_pid_t32    shm_cpid;
        __kernel_ipc_pid_t32    shm_lpid;
        unsigned short          shm_nattch;
};

struct shmid64_ds32 {
	struct ipc64_perm32 shm_perm;
	__kernel_size_t32 shm_segsz;
	__kernel_time_t32 shm_atime;
	__kernel_time_t32 shm_dtime;
	__kernel_time_t32 shm_ctime;
	__kernel_pid_t32 shm_cpid;
	__kernel_pid_t32 shm_lpid;
	unsigned int shm_nattch;
	unsigned int __unused1;
	unsigned int __unused2;
};

struct ipc_kludge32 {
	u32 msgp;
	s32 msgtyp;
};

static int
do_sys32_semctl(int first, int second, int third, void *uptr)
{
	union semun fourth;
	u32 pad;
	int err, err2;
	struct semid64_ds s;
	mm_segment_t old_fs;

	if (!uptr)
		return -EINVAL;
	err = -EFAULT;
	if (get_user (pad, (u32 *)uptr))
		return err;
	if ((third & ~IPC_64) == SETVAL)
		fourth.val = (int)pad;
	else
		fourth.__pad = (void *)A(pad);
	switch (third & ~IPC_64) {
	case IPC_INFO:
	case IPC_RMID:
	case IPC_SET:
	case SEM_INFO:
	case GETVAL:
	case GETPID:
	case GETNCNT:
	case GETZCNT:
	case GETALL:
	case SETVAL:
	case SETALL:
		err = sys_semctl (first, second, third, fourth);
		break;

	case IPC_STAT:
	case SEM_STAT:
		fourth.__pad = &s;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_semctl(first, second, third | IPC_64, fourth);
		set_fs(old_fs);

		if (third & IPC_64) {
			struct semid64_ds32 *usp64 = (struct semid64_ds32 *) A(pad);

			if (!access_ok(VERIFY_WRITE, usp64, sizeof(*usp64))) {
				err = -EFAULT;
				break;
			}
			err2 = __put_user(s.sem_perm.key, &usp64->sem_perm.key);
			err2 |= __put_user(s.sem_perm.uid, &usp64->sem_perm.uid);
			err2 |= __put_user(s.sem_perm.gid, &usp64->sem_perm.gid);
			err2 |= __put_user(s.sem_perm.cuid, &usp64->sem_perm.cuid);
			err2 |= __put_user(s.sem_perm.cgid, &usp64->sem_perm.cgid);
			err2 |= __put_user(s.sem_perm.mode, &usp64->sem_perm.mode);
			err2 |= __put_user(s.sem_perm.seq, &usp64->sem_perm.seq);
			err2 |= __put_user(s.sem_otime, &usp64->sem_otime);
			err2 |= __put_user(s.sem_ctime, &usp64->sem_ctime);
			err2 |= __put_user(s.sem_nsems, &usp64->sem_nsems);
		} else {
			struct semid_ds32 *usp32 = (struct semid_ds32 *) A(pad);

			if (!access_ok(VERIFY_WRITE, usp32, sizeof(*usp32))) {
				err = -EFAULT;
				break;
			}
			err2 = __put_user(s.sem_perm.key, &usp32->sem_perm.key);
			err2 |= __put_user(s.sem_perm.uid, &usp32->sem_perm.uid);
			err2 |= __put_user(s.sem_perm.gid, &usp32->sem_perm.gid);
			err2 |= __put_user(s.sem_perm.cuid, &usp32->sem_perm.cuid);
			err2 |= __put_user(s.sem_perm.cgid, &usp32->sem_perm.cgid);
			err2 |= __put_user(s.sem_perm.mode, &usp32->sem_perm.mode);
			err2 |= __put_user(s.sem_perm.seq, &usp32->sem_perm.seq);
			err2 |= __put_user(s.sem_otime, &usp32->sem_otime);
			err2 |= __put_user(s.sem_ctime, &usp32->sem_ctime);
			err2 |= __put_user(s.sem_nsems, &usp32->sem_nsems);
		}
		if (err2)
			err = -EFAULT;
		break;

	default:
		err = - EINVAL;
		break;
	}

	return err;
}

static int
do_sys32_msgsnd (int first, int second, int third, void *uptr)
{
	struct msgbuf32 *up = (struct msgbuf32 *)uptr;
	struct msgbuf *p;
	mm_segment_t old_fs;
	int err;

	if (second < 0)
		return -EINVAL;
	p = kmalloc (second + sizeof (struct msgbuf)
				    + 4, GFP_USER);
	if (!p)
		return -ENOMEM;
	err = get_user (p->mtype, &up->mtype);
	if (err)
		goto out;
	err |= __copy_from_user (p->mtext, &up->mtext, second);
	if (err)
		goto out;
	old_fs = get_fs ();
	set_fs (KERNEL_DS);
	err = sys_msgsnd (first, p, second, third);
	set_fs (old_fs);
out:
	kfree (p);

	return err;
}

static int
do_sys32_msgrcv (int first, int second, int msgtyp, int third,
		 int version, void *uptr)
{
	struct msgbuf32 *up;
	struct msgbuf *p;
	mm_segment_t old_fs;
	int err;

	if (!version) {
		struct ipc_kludge32 *uipck = (struct ipc_kludge32 *)uptr;
		struct ipc_kludge32 ipck;

		err = -EINVAL;
		if (!uptr)
			goto out;
		err = -EFAULT;
		if (copy_from_user (&ipck, uipck, sizeof (struct ipc_kludge32)))
			goto out;
		uptr = (void *)AA(ipck.msgp);
		msgtyp = ipck.msgtyp;
	}

	if (second < 0)
		return -EINVAL;
	err = -ENOMEM;
	p = kmalloc (second + sizeof (struct msgbuf) + 4, GFP_USER);
	if (!p)
		goto out;
	old_fs = get_fs ();
	set_fs (KERNEL_DS);
	err = sys_msgrcv (first, p, second + 4, msgtyp, third);
	set_fs (old_fs);
	if (err < 0)
		goto free_then_out;
	up = (struct msgbuf32 *)uptr;
	if (put_user (p->mtype, &up->mtype) ||
	    __copy_to_user (&up->mtext, p->mtext, err))
		err = -EFAULT;
free_then_out:
	kfree (p);
out:
	return err;
}

static int
do_sys32_msgctl (int first, int second, void *uptr)
{
	int err = -EINVAL, err2;
	struct msqid64_ds m;
	struct msqid_ds32 *up32 = (struct msqid_ds32 *)uptr;
	struct msqid64_ds32 *up64 = (struct msqid64_ds32 *)uptr;
	mm_segment_t old_fs;

	switch (second & ~IPC_64) {
	case IPC_INFO:
	case IPC_RMID:
	case MSG_INFO:
		err = sys_msgctl (first, second, (struct msqid_ds *)uptr);
		break;

	case IPC_SET:
		if (second & IPC_64) {
			if (!access_ok(VERIFY_READ, up64, sizeof(*up64))) {
				err = -EFAULT;
				break;
			}
			err = __get_user(m.msg_perm.uid, &up64->msg_perm.uid);
			err |= __get_user(m.msg_perm.gid, &up64->msg_perm.gid);
			err |= __get_user(m.msg_perm.mode, &up64->msg_perm.mode);
			err |= __get_user(m.msg_qbytes, &up64->msg_qbytes);
		} else {
			if (!access_ok(VERIFY_READ, up32, sizeof(*up32))) {
				err = -EFAULT;
				break;
			}
			err = __get_user(m.msg_perm.uid, &up32->msg_perm.uid);
			err |= __get_user(m.msg_perm.gid, &up32->msg_perm.gid);
			err |= __get_user(m.msg_perm.mode, &up32->msg_perm.mode);
			err |= __get_user(m.msg_qbytes, &up32->msg_qbytes);
		}
		if (err)
			break;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_msgctl(first, second | IPC_64, (struct msqid_ds *)&m);
		set_fs(old_fs);
		break;

	case IPC_STAT:
	case MSG_STAT:
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_msgctl(first, second | IPC_64, (struct msqid_ds *)&m);
		set_fs(old_fs);
		if (second & IPC_64) {
			if (!access_ok(VERIFY_WRITE, up64, sizeof(*up64))) {
				err = -EFAULT;
				break;
			}
			err2 = __put_user(m.msg_perm.key, &up64->msg_perm.key);
			err2 |= __put_user(m.msg_perm.uid, &up64->msg_perm.uid);
			err2 |= __put_user(m.msg_perm.gid, &up64->msg_perm.gid);
			err2 |= __put_user(m.msg_perm.cuid, &up64->msg_perm.cuid);
			err2 |= __put_user(m.msg_perm.cgid, &up64->msg_perm.cgid);
			err2 |= __put_user(m.msg_perm.mode, &up64->msg_perm.mode);
			err2 |= __put_user(m.msg_perm.seq, &up64->msg_perm.seq);
			err2 |= __put_user(m.msg_stime, &up64->msg_stime);
			err2 |= __put_user(m.msg_rtime, &up64->msg_rtime);
			err2 |= __put_user(m.msg_ctime, &up64->msg_ctime);
			err2 |= __put_user(m.msg_cbytes, &up64->msg_cbytes);
			err2 |= __put_user(m.msg_qnum, &up64->msg_qnum);
			err2 |= __put_user(m.msg_qbytes, &up64->msg_qbytes);
			err2 |= __put_user(m.msg_lspid, &up64->msg_lspid);
			err2 |= __put_user(m.msg_lrpid, &up64->msg_lrpid);
			if (err2)
				err = -EFAULT;
		} else {
			if (!access_ok(VERIFY_WRITE, up32, sizeof(*up32))) {
				err = -EFAULT;
				break;
			}
			err2 = __put_user(m.msg_perm.key, &up32->msg_perm.key);
			err2 |= __put_user(m.msg_perm.uid, &up32->msg_perm.uid);
			err2 |= __put_user(m.msg_perm.gid, &up32->msg_perm.gid);
			err2 |= __put_user(m.msg_perm.cuid, &up32->msg_perm.cuid);
			err2 |= __put_user(m.msg_perm.cgid, &up32->msg_perm.cgid);
			err2 |= __put_user(m.msg_perm.mode, &up32->msg_perm.mode);
			err2 |= __put_user(m.msg_perm.seq, &up32->msg_perm.seq);
			err2 |= __put_user(m.msg_stime, &up32->msg_stime);
			err2 |= __put_user(m.msg_rtime, &up32->msg_rtime);
			err2 |= __put_user(m.msg_ctime, &up32->msg_ctime);
			err2 |= __put_user(m.msg_cbytes, &up32->msg_cbytes);
			err2 |= __put_user(m.msg_qnum, &up32->msg_qnum);
			err2 |= __put_user(m.msg_qbytes, &up32->msg_qbytes);
			err2 |= __put_user(m.msg_lspid, &up32->msg_lspid);
			err2 |= __put_user(m.msg_lrpid, &up32->msg_lrpid);
			if (err2)
				err = -EFAULT;
		}
		break;
	}

	return err;
}

static int
do_sys32_shmat (int first, int second, int third, int version, void *uptr)
{
	unsigned long raddr;
	u32 *uaddr = (u32 *)A((u32)third);
	int err = -EINVAL;

	if (version == 1)
		return err;
	err = sys_shmat (first, uptr, second, &raddr);
	if (err)
		return err;
	err = put_user (raddr, uaddr);
	return err;
}

struct shm_info32 {
	int used_ids;
	u32 shm_tot, shm_rss, shm_swp;
	u32 swap_attempts, swap_successes;
};

static int
do_sys32_shmctl (int first, int second, void *uptr)
{
	struct shmid64_ds32 *up64 = (struct shmid64_ds32 *)uptr;
	struct shmid_ds32 *up32 = (struct shmid_ds32 *)uptr;
	struct shm_info32 *uip = (struct shm_info32 *)uptr;
	int err = -EFAULT, err2;
	struct shmid64_ds s64;
	mm_segment_t old_fs;
	struct shm_info si;
	struct shmid_ds s;

	switch (second & ~IPC_64) {
	case IPC_INFO:
		second = IPC_INFO; /* So that we don't have to translate it */
	case IPC_RMID:
	case SHM_LOCK:
	case SHM_UNLOCK:
		err = sys_shmctl(first, second, (struct shmid_ds *)uptr);
		break;
	case IPC_SET:
		if (second & IPC_64) {
			err = get_user(s.shm_perm.uid, &up64->shm_perm.uid);
			err |= get_user(s.shm_perm.gid, &up64->shm_perm.gid);
			err |= get_user(s.shm_perm.mode, &up64->shm_perm.mode);
		} else {
			err = get_user(s.shm_perm.uid, &up32->shm_perm.uid);
			err |= get_user(s.shm_perm.gid, &up32->shm_perm.gid);
			err |= get_user(s.shm_perm.mode, &up32->shm_perm.mode);
		}
		if (err)
			break;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_shmctl(first, second & ~IPC_64, &s);
		set_fs(old_fs);
		break;

	case IPC_STAT:
	case SHM_STAT:
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_shmctl(first, second | IPC_64, (void *) &s64);
		set_fs(old_fs);
		if (err < 0)
			break;
		if (second & IPC_64) {
			if (!access_ok(VERIFY_WRITE, up64, sizeof(*up64))) {
				err = -EFAULT;
				break;
			}
			err2 = __put_user(s64.shm_perm.key, &up64->shm_perm.key);
			err2 |= __put_user(s64.shm_perm.uid, &up64->shm_perm.uid);
			err2 |= __put_user(s64.shm_perm.gid, &up64->shm_perm.gid);
			err2 |= __put_user(s64.shm_perm.cuid, &up64->shm_perm.cuid);
			err2 |= __put_user(s64.shm_perm.cgid, &up64->shm_perm.cgid);
			err2 |= __put_user(s64.shm_perm.mode, &up64->shm_perm.mode);
			err2 |= __put_user(s64.shm_perm.seq, &up64->shm_perm.seq);
			err2 |= __put_user(s64.shm_atime, &up64->shm_atime);
			err2 |= __put_user(s64.shm_dtime, &up64->shm_dtime);
			err2 |= __put_user(s64.shm_ctime, &up64->shm_ctime);
			err2 |= __put_user(s64.shm_segsz, &up64->shm_segsz);
			err2 |= __put_user(s64.shm_nattch, &up64->shm_nattch);
			err2 |= __put_user(s64.shm_cpid, &up64->shm_cpid);
			err2 |= __put_user(s64.shm_lpid, &up64->shm_lpid);
		} else {
			if (!access_ok(VERIFY_WRITE, up32, sizeof(*up32))) {
				err = -EFAULT;
				break;
			}
			err2 = __put_user(s64.shm_perm.key, &up32->shm_perm.key);
			err2 |= __put_user(s64.shm_perm.uid, &up32->shm_perm.uid);
			err2 |= __put_user(s64.shm_perm.gid, &up32->shm_perm.gid);
			err2 |= __put_user(s64.shm_perm.cuid, &up32->shm_perm.cuid);
			err2 |= __put_user(s64.shm_perm.cgid, &up32->shm_perm.cgid);
			err2 |= __put_user(s64.shm_perm.mode, &up32->shm_perm.mode);
			err2 |= __put_user(s64.shm_perm.seq, &up32->shm_perm.seq);
			err2 |= __put_user(s64.shm_atime, &up32->shm_atime);
			err2 |= __put_user(s64.shm_dtime, &up32->shm_dtime);
			err2 |= __put_user(s64.shm_ctime, &up32->shm_ctime);
			err2 |= __put_user(s64.shm_segsz, &up32->shm_segsz);
			err2 |= __put_user(s64.shm_nattch, &up32->shm_nattch);
			err2 |= __put_user(s64.shm_cpid, &up32->shm_cpid);
			err2 |= __put_user(s64.shm_lpid, &up32->shm_lpid);
		}
		if (err2)
			err = -EFAULT;
		break;

	case SHM_INFO:
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = sys_shmctl(first, second, (void *)&si);
		set_fs(old_fs);
		if (err < 0)
			break;
		err2 = put_user(si.used_ids, &uip->used_ids);
		err2 |= __put_user(si.shm_tot, &uip->shm_tot);
		err2 |= __put_user(si.shm_rss, &uip->shm_rss);
		err2 |= __put_user(si.shm_swp, &uip->shm_swp);
		err2 |= __put_user(si.swap_attempts, &uip->swap_attempts);
		err2 |= __put_user (si.swap_successes, &uip->swap_successes);
		if (err2)
			err = -EFAULT;
		break;

	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static inline void *alloc_user_space(long len)
{
	unsigned long sp = (unsigned long) current + THREAD_SIZE - 32;
 
	return (void *) (sp - len);
}

static int sys32_semtimedop(int semid, struct sembuf *tsems, int nsems,
                            const struct timespec32 *timeout32)
{
	struct timespec32 t32;
	struct timespec *t64 = alloc_user_space(sizeof(*t64));

	if (copy_from_user(&t32, timeout32, sizeof(t32)))
		return -EFAULT;
                                                                                
	if (put_user(t32.tv_sec, &t64->tv_sec) ||
	    put_user(t32.tv_nsec, &t64->tv_nsec))
		return -EFAULT;

	return sys_semtimedop(semid, tsems, nsems, t64);
}

asmlinkage long
sys32_ipc (u32 call, int first, int second, int third, u32 ptr, u32 fifth)
{
	int version, err;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	switch (call) {

	case SEMOP:
		/* struct sembuf is the same on 32 and 64bit :)) */
		err = sys_semtimedop (first, (struct sembuf *)AA(ptr),
		                      second, NULL);
		break;
	case SEMTIMEDOP:
		err = sys32_semtimedop(first, (struct sembuf *)AA(ptr), second,
		                       (const struct timespec32 *) AA(fifth));
		break;
	case SEMGET:
		err = sys_semget (first, second, third);
		break;
	case SEMCTL:
		err = do_sys32_semctl (first, second, third,
				       (void *)AA(ptr));
		break;

	case MSGSND:
		err = do_sys32_msgsnd (first, second, third,
				       (void *)AA(ptr));
		break;
	case MSGRCV:
		err = do_sys32_msgrcv (first, second, fifth, third,
				       version, (void *)AA(ptr));
		break;
	case MSGGET:
		err = sys_msgget ((key_t) first, second);
		break;
	case MSGCTL:
		err = do_sys32_msgctl (first, second, (void *)AA(ptr));
		break;

	case SHMAT:
		err = do_sys32_shmat (first, second, third,
				      version, (void *)AA(ptr));
		break;
	case SHMDT:
		err = sys_shmdt ((char *)A(ptr));
		break;
	case SHMGET:
		err = sys_shmget (first, second, third);
		break;
	case SHMCTL:
		err = do_sys32_shmctl (first, second, (void *)AA(ptr));
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

struct sysctl_args32
{
	__kernel_caddr_t32 name;
	int nlen;
	__kernel_caddr_t32 oldval;
	__kernel_caddr_t32 oldlenp;
	__kernel_caddr_t32 newval;
	__kernel_size_t32 newlen;
	unsigned int __unused[4];
};

#ifdef CONFIG_SYSCTL

asmlinkage long sys32_sysctl(struct sysctl_args32 *args)
{
	struct sysctl_args32 tmp;
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

#else /* CONFIG_SYSCTL */

asmlinkage long sys32_sysctl(struct sysctl_args32 *args)
{
	return -ENOSYS;
}

#endif /* CONFIG_SYSCTL */

asmlinkage long sys32_newuname(struct new_utsname * name)
{
	int ret = 0;

	down_read(&uts_sem);
	if (copy_to_user(name,&system_utsname,sizeof *name))
		ret = -EFAULT;
	up_read(&uts_sem);

	if (current->personality == PER_LINUX32 && !ret)
		if (copy_to_user(name->machine, "mips\0\0\0", 8))
			ret = -EFAULT;

	return ret;
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

/* ustat compatibility */
struct ustat32 {
	__kernel_daddr_t32	f_tfree;
	__kernel_ino_t32	f_tinode;
	char			f_fname[6];
	char			f_fpack[6];
};

extern asmlinkage long sys_ustat(dev_t dev, struct ustat * ubuf);

asmlinkage int sys32_ustat(dev_t dev, struct ustat32 * ubuf32)
{
	int err;
	struct ustat tmp;
	struct ustat32 tmp32;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	err = sys_ustat(dev, &tmp);
	set_fs (old_fs);

	if (err)
		goto out;

	memset(&tmp32,0,sizeof(struct ustat32));
	tmp32.f_tfree = tmp.f_tfree;
	tmp32.f_tinode = tmp.f_tinode;

	err = copy_to_user(ubuf32,&tmp32,sizeof(struct ustat32)) ? -EFAULT : 0;

out:
	return err;
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

asmlinkage int sys32_adjtimex(struct timex32 *utp)
{
	struct timex txc;
	int ret;

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

/*
 *  Declare the 32-bit version of the msghdr
 */
 
struct msghdr32 {
	unsigned int    msg_name;	/* Socket name			*/
	int		msg_namelen;	/* Length of name		*/
	unsigned int    msg_iov;	/* Data blocks			*/
	unsigned int	msg_iovlen;	/* Number of blocks		*/
	unsigned int    msg_control;	/* Per protocol magic (eg BSD file descriptor passing) */
	unsigned int	msg_controllen;	/* Length of cmsg list */
	unsigned	msg_flags;
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
		kiov->iov_base = (void *)AA(buf);
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

	kmsg->msg_name = (void *)AA(tmp1);
	kmsg->msg_iov = (struct iovec *)AA(tmp2);
	kmsg->msg_control = (void *)AA(tmp3);

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

static __inline__ void
sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

/* XXX This really belongs in some header file... -DaveM */
#define MAX_SOCK_ADDR	128		/* 108 for Unix domain - 
					   16 for IP, 16 for IPX,
					   24 for IPv6,
					   about 80 for AX.25 */

extern struct socket *sockfd_lookup(int fd, int *err);

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
			return -ENOBUFS;
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

	if(uaddr != NULL && kern_msg.msg_namelen && err >= 0)
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

extern asmlinkage ssize_t sys_sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

asmlinkage int sys32_sendfile(int out_fd, int in_fd, __kernel_off_t32 *offset, s32 count)
{
	mm_segment_t old_fs = get_fs();
	int ret;
	off_t of;
	
	if (offset && get_user(of, offset))
		return -EFAULT;
		
	set_fs(KERNEL_DS);
	ret = sys_sendfile(out_fd, in_fd, offset ? &of : NULL, count);
	set_fs(old_fs);
	
	if (offset && put_user(of, offset))
		return -EFAULT;
		
	return ret;
}

asmlinkage ssize_t sys_readahead(int fd, loff_t offset, size_t count);

asmlinkage ssize_t sys32_readahead(int fd, u32 pad0, u64 a2, u64 a3,
                                   size_t count)
{
	return sys_readahead(fd, merge_64(a2, a3), count);
}

/* Argument list sizes for sys_socketcall */
#define AL(x) ((x) * sizeof(unsigned int))
static unsigned char socketcall_nargs[18]={AL(0),AL(3),AL(3),AL(3),AL(2),AL(3),
				AL(3),AL(3),AL(4),AL(4),AL(4),AL(6),
				AL(6),AL(2),AL(5),AL(5),AL(3),AL(3)};
#undef AL

/*
 *	System call vectors. 
 *
 *	Argument checking cleaned up. Saved 20% in size.
 *  This function doesn't need to set the kernel lock because
 *  it is set by the callees. 
 */

asmlinkage long sys32_socketcall(int call, unsigned int *args32)
{
	unsigned int a[6];
	unsigned int a0,a1;
	int err;

	if(call<1||call>SYS_RECVMSG)
		return -EINVAL;

	/* copy_from_user should be SMP safe. */
	if (copy_from_user(a, args32, socketcall_nargs[call]))
		return -EFAULT;
		
	a0=a[0];
	a1=a[1];
	
	switch (call) {
	case SYS_SOCKET:
		err = sys_socket(a0,a1,a[2]);
		break;
	case SYS_BIND:
		err = sys_bind(a0,(struct sockaddr *)A(a1), a[2]);
		break;
	case SYS_CONNECT:
		err = sys_connect(a0, (struct sockaddr *)A(a1), a[2]);
		break;
	case SYS_LISTEN:
		err = sys_listen(a0,a1);
		break;
	case SYS_ACCEPT:
		err = sys_accept(a0,(struct sockaddr *)A(a1), (int *)A(a[2]));
		break;
	case SYS_GETSOCKNAME:
		err = sys_getsockname(a0,(struct sockaddr *)A(a1), (int *)A(a[2]));
		break;
	case SYS_GETPEERNAME:
		err = sys_getpeername(a0, (struct sockaddr *)A(a1), (int *)A(a[2]));
		break;
	case SYS_SOCKETPAIR:
		err = sys_socketpair(a0,a1, a[2], (int *)A(a[3]));
		break;
	case SYS_SEND:
		err = sys_send(a0, (void *)A(a1), a[2], a[3]);
		break;
	case SYS_SENDTO:
		err = sys_sendto(a0,(void *)A(a1), a[2], a[3],
				 (struct sockaddr *)A(a[4]), a[5]);
		break;
	case SYS_RECV:
		err = sys_recv(a0, (void *)A(a1), a[2], a[3]);
		break;
	case SYS_RECVFROM:
		err = sys_recvfrom(a0, (void *)A(a1), a[2], a[3],
				   (struct sockaddr *)A(a[4]), (int *)A(a[5]));
		break;
	case SYS_SHUTDOWN:
		err = sys_shutdown(a0,a1);
		break;
	case SYS_SETSOCKOPT:
		err = sys_setsockopt(a0, a1, a[2], (char *)A(a[3]), a[4]);
		break;
	case SYS_GETSOCKOPT:
		err = sys_getsockopt(a0, a1, a[2], (char *)A(a[3]), (int *)A(a[4]));
		break;
	case SYS_SENDMSG:
		err = sys_sendmsg(a0, (struct msghdr *) A(a1), a[2]);
		break;
	case SYS_RECVMSG:
		err = sys_recvmsg(a0, (struct msghdr *) A(a1), a[2]);
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

#ifdef CONFIG_MODULES

/* From sparc64 */

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

asmlinkage unsigned long
sys32_create_module(const char *name_user, size_t size)
{
	return -ENOSYS;
}

asmlinkage int
sys32_init_module(const char *name_user, struct module *mod_user)
{
	return -ENOSYS;
}

asmlinkage int
sys32_delete_module(const char *name_user)
{
	return -ENOSYS;
}

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

asmlinkage long
sys32_get_kernel_syms(struct kernel_sym *table)
{
	return -ENOSYS;
}

#endif /* CONFIG_MODULES */
