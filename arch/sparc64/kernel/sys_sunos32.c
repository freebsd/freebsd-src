/* $Id: sys_sunos32.c,v 1.61 2001/08/13 14:40:07 davem Exp $
 * sys_sunos32.c: SunOS binary compatability layer on sparc64.
 *
 * Copyright (C) 1995, 1996, 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1995 Miguel de Icaza (miguel@nuclecu.unam.mx)
 *
 * Based upon preliminary work which is:
 *
 * Copyright (C) 1995 Adrian M. Rodriguez (adrian@remus.rutgers.edu)
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/resource.h>
#include <linux/ipc.h>
#include <linux/shm.h>
#include <linux/msg.h>
#include <linux/sem.h>
#include <linux/signal.h>
#include <linux/uio.h>
#include <linux/utsname.h>
#include <linux/major.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pconf.h>
#include <asm/idprom.h> /* for gethostid() */
#include <asm/unistd.h>
#include <asm/system.h>

/* For the nfs mount emulation */
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs_mount.h>

/* for sunos_select */
#include <linux/time.h>
#include <linux/personality.h>

/* Use this to get at 32-bit user passed pointers. */
#define A(__x)				\
({	unsigned long __ret;		\
	__asm__ ("srl	%0, 0, %0"	\
		 : "=r" (__ret)		\
		 : "0" (__x));		\
	__ret;				\
})

#define SUNOS_NR_OPEN	256

asmlinkage u32 sunos_mmap(u32 addr, u32 len, u32 prot, u32 flags, u32 fd, u32 off)
{
	struct file *file = NULL;
	unsigned long retval, ret_type;

	if(flags & MAP_NORESERVE) {
		static int cnt;
		if (cnt++ < 10)
			printk("%s:  unimplemented SunOS MAP_NORESERVE mmap() flag\n",
			       current->comm);
		flags &= ~MAP_NORESERVE;
	}
	retval = -EBADF;
	if(!(flags & MAP_ANONYMOUS)) {
		struct inode * inode;
		if(fd >= SUNOS_NR_OPEN)
			goto out;
 		file = fget(fd);
		if (!file)
			goto out;
		inode = file->f_dentry->d_inode;
		if(MAJOR(inode->i_rdev)==MEM_MAJOR && MINOR(inode->i_rdev)==5) {
			flags |= MAP_ANONYMOUS;
			fput(file);
			file = NULL;
		}
	}

	retval = -EINVAL;
	if(!(flags & MAP_FIXED))
		addr = 0;
	else if (len > 0xf0000000 || addr > 0xf0000000 - len)
		goto out_putf;
	ret_type = flags & _MAP_NEW;
	flags &= ~_MAP_NEW;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	down_write(&current->mm->mmap_sem);
	retval = do_mmap(file,
			 (unsigned long) addr, (unsigned long) len,
			 (unsigned long) prot, (unsigned long) flags,
			 (unsigned long) off);
	up_write(&current->mm->mmap_sem);
	if(!ret_type)
		retval = ((retval < 0xf0000000) ? 0 : retval);
out_putf:
	if (file)
		fput(file);
out:
	return (u32) retval;
}

asmlinkage int sunos_mctl(u32 addr, u32 len, int function, u32 arg)
{
	return 0;
}

asmlinkage int sunos_brk(u32 baddr)
{
	int freepages, retval = -ENOMEM;
	unsigned long rlim;
	unsigned long newbrk, oldbrk, brk = (unsigned long) baddr;

	down_write(&current->mm->mmap_sem);
	if (brk < current->mm->end_code)
		goto out;
	newbrk = PAGE_ALIGN(brk);
	oldbrk = PAGE_ALIGN(current->mm->brk);
	retval = 0;
	if (oldbrk == newbrk) {
		current->mm->brk = brk;
		goto out;
	}
	/* Always allow shrinking brk. */
	if (brk <= current->mm->brk) {
		current->mm->brk = brk;
		do_munmap(current->mm, newbrk, oldbrk-newbrk);
		goto out;
	}
	/* Check against rlimit and stack.. */
	retval = -ENOMEM;
	rlim = current->rlim[RLIMIT_DATA].rlim_cur;
	if (rlim >= RLIM_INFINITY)
		rlim = ~0;
	if (brk - current->mm->end_code > rlim)
		goto out;
	/* Check against existing mmap mappings. */
	if (find_vma_intersection(current->mm, oldbrk, newbrk+PAGE_SIZE))
		goto out;
	/* stupid algorithm to decide if we have enough memory: while
	 * simple, it hopefully works in most obvious cases.. Easy to
	 * fool it, but this should catch most mistakes.
	 */
	freepages = atomic_read(&buffermem_pages) >> PAGE_SHIFT;
	freepages += page_cache_size;
	freepages >>= 1;
	freepages += nr_free_pages();
	freepages += nr_swap_pages;
	freepages -= num_physpages >> 4;
	freepages -= (newbrk-oldbrk) >> PAGE_SHIFT;
	if (freepages < 0)
		goto out;
	/* Ok, we have probably got enough memory - let it rip. */
	current->mm->brk = brk;
	do_brk(oldbrk, newbrk-oldbrk);
	retval = 0;
out:
	up_write(&current->mm->mmap_sem);
	return retval;
}

asmlinkage u32 sunos_sbrk(int increment)
{
	int error, oldbrk;

	/* This should do it hopefully... */
	oldbrk = (int)current->mm->brk;
	error = sunos_brk(((int) current->mm->brk) + increment);
	if(!error)
		error = oldbrk;
	return error;
}

asmlinkage u32 sunos_sstk(int increment)
{
	printk("%s: Call to sunos_sstk(increment<%d>) is unsupported\n",
	       current->comm, increment);

	return (u32)-1;
}

/* Give hints to the kernel as to what paging strategy to use...
 * Completely bogus, don't remind me.
 */
#define VA_NORMAL     0 /* Normal vm usage expected */
#define VA_ABNORMAL   1 /* Abnormal/random vm usage probable */
#define VA_SEQUENTIAL 2 /* Accesses will be of a sequential nature */
#define VA_INVALIDATE 3 /* Page table entries should be flushed ??? */
static char *vstrings[] = {
	"VA_NORMAL",
	"VA_ABNORMAL",
	"VA_SEQUENTIAL",
	"VA_INVALIDATE",
};

asmlinkage void sunos_vadvise(u32 strategy)
{
	static int count;

	/* I wanna see who uses this... */
	if (count++ < 5)
		printk("%s: Advises us to use %s paging strategy\n",
		       current->comm,
		       strategy <= 3 ? vstrings[strategy] : "BOGUS");
}

/* This just wants the soft limit (ie. rlim_cur element) of the RLIMIT_NOFILE
 * resource limit and is for backwards compatibility with older sunos
 * revs.
 */
asmlinkage int sunos_getdtablesize(void)
{
	return SUNOS_NR_OPEN;
}


#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

asmlinkage u32 sunos_sigblock(u32 blk_mask)
{
	u32 old;

	spin_lock_irq(&current->sigmask_lock);
	old = (u32) current->blocked.sig[0];
	current->blocked.sig[0] |= (blk_mask & _BLOCKABLE);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	return old;
}

asmlinkage u32 sunos_sigsetmask(u32 newmask)
{
	u32 retval;

	spin_lock_irq(&current->sigmask_lock);
	retval = (u32) current->blocked.sig[0];
	current->blocked.sig[0] = (newmask & _BLOCKABLE);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	return retval;
}

/* SunOS getdents is very similar to the newer Linux (iBCS2 compliant)    */
/* getdents system call, the format of the structure just has a different */
/* layout (d_off+d_ino instead of d_ino+d_off) */
struct sunos_dirent {
    s32		d_off;
    u32		d_ino;
    u16		d_reclen;
    u16		d_namlen;
    char	d_name[1];
};

struct sunos_dirent_callback {
    struct sunos_dirent *curr;
    struct sunos_dirent *previous;
    int count;
    int error;
};

#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP(x) (((x)+sizeof(s32)-1) & ~(sizeof(s32)-1))

static int sunos_filldir(void * __buf, const char * name, int namlen,
			 loff_t offset, ino_t ino, unsigned int d_type)
{
	struct sunos_dirent * dirent;
	struct sunos_dirent_callback * buf = (struct sunos_dirent_callback *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent)
		put_user(offset, &dirent->d_off);
	dirent = buf->curr;
	buf->previous = dirent;
	put_user(ino, &dirent->d_ino);
	put_user(namlen, &dirent->d_namlen);
	put_user(reclen, &dirent->d_reclen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	((char *) dirent) += reclen;
	buf->curr = dirent;
	buf->count -= reclen;
	return 0;
}

asmlinkage int sunos_getdents(unsigned int fd, u32 u_dirent, int cnt)
{
	struct file * file;
	struct sunos_dirent * lastdirent;
	struct sunos_dirent_callback buf;
	int error = -EBADF;
	void *dirent = (void *)A(u_dirent);

	if(fd >= SUNOS_NR_OPEN)
		goto out;

	file = fget(fd);
	if(!file)
		goto out;

	error = -EINVAL;
	if(cnt < (sizeof(struct sunos_dirent) + 255))
		goto out_putf;

	buf.curr = (struct sunos_dirent *) dirent;
	buf.previous = NULL;
	buf.count = cnt;
	buf.error = 0;

	error = vfs_readdir(file, sunos_filldir, &buf);
	if (error < 0)
		goto out_putf;

	lastdirent = buf.previous;
	error = buf.error;
	if (lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = cnt - buf.count;
	}

out_putf:
	fput(file);
out:
	return error;
}

/* Old sunos getdirentries, severely broken compatibility stuff here. */
struct sunos_direntry {
    u32		d_ino;
    u16		d_reclen;
    u16		d_namlen;
    char	d_name[1];
};

struct sunos_direntry_callback {
    struct sunos_direntry *curr;
    struct sunos_direntry *previous;
    int count;
    int error;
};

static int sunos_filldirentry(void * __buf, const char * name, int namlen,
			      loff_t offset, ino_t ino, unsigned int d_type)
{
	struct sunos_direntry * dirent;
	struct sunos_direntry_callback * buf = (struct sunos_direntry_callback *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	dirent = buf->curr;
	buf->previous = dirent;
	put_user(ino, &dirent->d_ino);
	put_user(namlen, &dirent->d_namlen);
	put_user(reclen, &dirent->d_reclen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	((char *) dirent) += reclen;
	buf->curr = dirent;
	buf->count -= reclen;
	return 0;
}

asmlinkage int sunos_getdirentries(unsigned int fd, u32 u_dirent,
				   int cnt, u32 u_basep)
{
	void *dirent = (void *) A(u_dirent);
	unsigned int *basep = (unsigned int *)A(u_basep);
	struct file * file;
	struct sunos_direntry * lastdirent;
	int error = -EBADF;
	struct sunos_direntry_callback buf;

	if(fd >= SUNOS_NR_OPEN)
		goto out;

	file = fget(fd);
	if(!file)
		goto out;

	error = -EINVAL;
	if(cnt < (sizeof(struct sunos_direntry) + 255))
		goto out_putf;

	buf.curr = (struct sunos_direntry *) dirent;
	buf.previous = NULL;
	buf.count = cnt;
	buf.error = 0;

	error = vfs_readdir(file, sunos_filldirentry, &buf);
	if (error < 0)
		goto out_putf;

	lastdirent = buf.previous;
	error = buf.error;
	if (lastdirent) {
		put_user(file->f_pos, basep);
		error = cnt - buf.count;
	}

out_putf:
	fput(file);
out:
	return error;
}

struct sunos_utsname {
	char sname[9];
	char nname[9];
	char nnext[56];
	char rel[9];
	char ver[9];
	char mach[9];
};

asmlinkage int sunos_uname(struct sunos_utsname *name)
{
	int ret;

	down_read(&uts_sem);
	ret = copy_to_user(&name->sname[0], &system_utsname.sysname[0], sizeof(name->sname) - 1);
	ret |= copy_to_user(&name->nname[0], &system_utsname.nodename[0], sizeof(name->nname) - 1);
	ret |= put_user('\0', &name->nname[8]);
	ret |= copy_to_user(&name->rel[0], &system_utsname.release[0], sizeof(name->rel) - 1);
	ret |= copy_to_user(&name->ver[0], &system_utsname.version[0], sizeof(name->ver) - 1);
	ret |= copy_to_user(&name->mach[0], &system_utsname.machine[0], sizeof(name->mach) - 1);
	up_read(&uts_sem);
	return (ret ? -EFAULT : 0);
}

asmlinkage int sunos_nosys(void)
{
	struct pt_regs *regs;
	siginfo_t info;
	static int cnt;

	regs = current->thread.kregs;
	if ((current->thread.flags & SPARC_FLAG_32BIT) != 0) {
		regs->tpc &= 0xffffffff;
		regs->tnpc &= 0xffffffff;
	}
	info.si_signo = SIGSYS;
	info.si_errno = 0;
	info.si_code = __SI_FAULT|0x100;
	info.si_addr = (void *)regs->tpc;
	info.si_trapno = regs->u_regs[UREG_G1];
	send_sig_info(SIGSYS, &info, current);
	if (cnt++ < 4) {
		printk("Process makes ni_syscall number %d, register dump:\n",
		       (int) regs->u_regs[UREG_G1]);
		show_regs(regs);
	}
	return -ENOSYS;
}

/* This is not a real and complete implementation yet, just to keep
 * the easy SunOS binaries happy.
 */
asmlinkage int sunos_fpathconf(int fd, int name)
{
	int ret;

	switch(name) {
	case _PCONF_LINK:
		ret = LINK_MAX;
		break;
	case _PCONF_CANON:
		ret = MAX_CANON;
		break;
	case _PCONF_INPUT:
		ret = MAX_INPUT;
		break;
	case _PCONF_NAME:
		ret = NAME_MAX;
		break;
	case _PCONF_PATH:
		ret = PATH_MAX;
		break;
	case _PCONF_PIPE:
		ret = PIPE_BUF;
		break;
	case _PCONF_CHRESTRICT:		/* XXX Investigate XXX */
		ret = 1;
		break;
	case _PCONF_NOTRUNC:		/* XXX Investigate XXX */
	case _PCONF_VDISABLE:
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

asmlinkage int sunos_pathconf(u32 u_path, int name)
{
	int ret;

	ret = sunos_fpathconf(0, name); /* XXX cheese XXX */
	return ret;
}

/* SunOS mount system call emulation */
extern asmlinkage int
sys32_select(int n, u32 inp, u32 outp, u32 exp, u32 tvp);

struct timeval32
{
	int tv_sec, tv_usec;
};

asmlinkage int sunos_select(int width, u32 inp, u32 outp, u32 exp, u32 tvp_x)
{
	int ret;

	/* SunOS binaries expect that select won't change the tvp contents */
	ret = sys32_select (width, inp, outp, exp, tvp_x);
	if (ret == -EINTR && tvp_x) {
		struct timeval32 *tvp = (struct timeval32 *)A(tvp_x);
		time_t sec, usec;

		__get_user(sec, &tvp->tv_sec);
		__get_user(usec, &tvp->tv_usec);
		if (sec == 0 && usec == 0)
			ret = 0;
	}
	return ret;
}

asmlinkage void sunos_nop(void)
{
	return;
}

/* XXXXXXXXXX SunOS mount/umount. XXXXXXXXXXX */
#define SMNT_RDONLY       1
#define SMNT_NOSUID       2
#define SMNT_NEWTYPE      4
#define SMNT_GRPID        8
#define SMNT_REMOUNT      16
#define SMNT_NOSUB        32
#define SMNT_MULTI        64
#define SMNT_SYS5         128

struct sunos_fh_t {
	char fh_data [NFS_FHSIZE];
};

struct sunos_nfs_mount_args {
	struct sockaddr_in  *addr; /* file server address */
	struct nfs_fh *fh;     /* File handle to be mounted */
	int        flags;      /* flags */
	int        wsize;      /* write size in bytes */
	int        rsize;      /* read size in bytes */
	int        timeo;      /* initial timeout in .1 secs */
	int        retrans;    /* times to retry send */
	char       *hostname;  /* server's hostname */
	int        acregmin;   /* attr cache file min secs */
	int        acregmax;   /* attr cache file max secs */
	int        acdirmin;   /* attr cache dir min secs */
	int        acdirmax;   /* attr cache dir max secs */
	char       *netname;   /* server's netname */
};

extern asmlinkage int sys_mount(char *, char *, char *, unsigned long, void *);
extern asmlinkage int sys_connect(int fd, struct sockaddr *uservaddr, int addrlen);
extern asmlinkage int sys_socket(int family, int type, int protocol);
extern asmlinkage int sys_bind(int fd, struct sockaddr *umyaddr, int addrlen);


/* Bind the socket on a local reserved port and connect it to the
 * remote server.  This on Linux/i386 is done by the mount program,
 * not by the kernel. 
 */
/* XXXXXXXXXXXXXXXXXXXX */
static int
sunos_nfs_get_server_fd (int fd, struct sockaddr_in *addr)
{
	struct sockaddr_in local;
	struct sockaddr_in server;
	int    try_port;
	int    ret;
	struct socket *socket;
	struct inode  *inode;
	struct file   *file;

	file = fget(fd);
	if(!file)
		return 0;

	inode = file->f_dentry->d_inode;

	socket = &inode->u.socket_i;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;

	/* IPPORT_RESERVED = 1024, can't find the definition in the kernel */
	try_port = 1024;
	do {
		local.sin_port = htons (--try_port);
		ret = socket->ops->bind(socket, (struct sockaddr*)&local,
					sizeof(local));
	} while (ret && try_port > (1024 / 2));

	if (ret) {
		fput(file);
		return 0;
	}

	server.sin_family = AF_INET;
	server.sin_addr = addr->sin_addr;
	server.sin_port = NFS_PORT;

	/* Call sys_connect */
	ret = socket->ops->connect (socket, (struct sockaddr *) &server,
				    sizeof (server), file->f_flags);
	fput(file);
	if (ret < 0)
		return 0;
	return 1;
}

/* XXXXXXXXXXXXXXXXXXXX */
static int get_default (int value, int def_value)
{
    if (value)
	return value;
    else
	return def_value;
}

/* XXXXXXXXXXXXXXXXXXXX */
static int sunos_nfs_mount(char *dir_name, int linux_flags, void *data)
{
	int  server_fd, err;
	char *the_name, *mount_page;
	struct nfs_mount_data linux_nfs_mount;
	struct sunos_nfs_mount_args sunos_mount;

	/* Ok, here comes the fun part: Linux's nfs mount needs a
	 * socket connection to the server, but SunOS mount does not
	 * require this, so we use the information on the destination
	 * address to create a socket and bind it to a reserved
	 * port on this system
	 */
	if (copy_from_user(&sunos_mount, data, sizeof(sunos_mount)))
		return -EFAULT;

	server_fd = sys_socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (server_fd < 0)
		return -ENXIO;

	if (copy_from_user(&linux_nfs_mount.addr,sunos_mount.addr,
				sizeof(*sunos_mount.addr)) ||
	    copy_from_user(&linux_nfs_mount.root,sunos_mount.fh,
				sizeof(*sunos_mount.fh))) {
		sys_close (server_fd);
		return -EFAULT;
	}

	if (!sunos_nfs_get_server_fd (server_fd, &linux_nfs_mount.addr)){
		sys_close (server_fd);
		return -ENXIO;
	}

	/* Now, bind it to a locally reserved port */
	linux_nfs_mount.version  = NFS_MOUNT_VERSION;
	linux_nfs_mount.flags    = sunos_mount.flags;
	linux_nfs_mount.fd       = server_fd;
	
	linux_nfs_mount.rsize    = get_default (sunos_mount.rsize, 8192);
	linux_nfs_mount.wsize    = get_default (sunos_mount.wsize, 8192);
	linux_nfs_mount.timeo    = get_default (sunos_mount.timeo, 10);
	linux_nfs_mount.retrans  = sunos_mount.retrans;
	
	linux_nfs_mount.acregmin = sunos_mount.acregmin;
	linux_nfs_mount.acregmax = sunos_mount.acregmax;
	linux_nfs_mount.acdirmin = sunos_mount.acdirmin;
	linux_nfs_mount.acdirmax = sunos_mount.acdirmax;

	the_name = getname(sunos_mount.hostname);
	if(IS_ERR(the_name))
		return PTR_ERR(the_name);

	strncpy (linux_nfs_mount.hostname, the_name, 254);
	linux_nfs_mount.hostname [255] = 0;
	putname (the_name);
	
	mount_page = (char *) get_zeroed_page(GFP_KERNEL);
	if (!mount_page)
		return -ENOMEM;

	memcpy(mount_page, &linux_nfs_mount, sizeof(linux_nfs_mount));

	err = do_mount("", dir_name, "nfs", linux_flags, mount_page);

	free_page((unsigned long) mount_page);
	return err;
}

/* XXXXXXXXXXXXXXXXXXXX */
asmlinkage int
sunos_mount(char *type, char *dir, int flags, void *data)
{
	int linux_flags = 0;
	int ret = -EINVAL;
	char *dev_fname = 0;
	char *dir_page, *type_page;

	if (!capable (CAP_SYS_ADMIN))
		return -EPERM;

	/* We don't handle the integer fs type */
	if ((flags & SMNT_NEWTYPE) == 0)
		goto out;

	/* Do not allow for those flags we don't support */
	if (flags & (SMNT_GRPID|SMNT_NOSUB|SMNT_MULTI|SMNT_SYS5))
		goto out;

	if(flags & SMNT_REMOUNT)
		linux_flags |= MS_REMOUNT;
	if(flags & SMNT_RDONLY)
		linux_flags |= MS_RDONLY;
	if(flags & SMNT_NOSUID)
		linux_flags |= MS_NOSUID;

	dir_page = getname(dir);
	ret = PTR_ERR(dir_page);
	if (IS_ERR(dir_page))
		goto out;

	type_page = getname(type);
	ret = PTR_ERR(type_page);
	if (IS_ERR(type_page))
		goto out1;

	if(strcmp(type_page, "ext2") == 0) {
		dev_fname = getname(data);
	} else if(strcmp(type_page, "iso9660") == 0) {
		dev_fname = getname(data);
	} else if(strcmp(type_page, "minix") == 0) {
		dev_fname = getname(data);
	} else if(strcmp(type_page, "nfs") == 0) {
		ret = sunos_nfs_mount (dir_page, flags, data);
		goto out2;
        } else if(strcmp(type_page, "ufs") == 0) {
		printk("Warning: UFS filesystem mounts unsupported.\n");
		ret = -ENODEV;
		goto out2;
	} else if(strcmp(type_page, "proc")) {
		ret = -ENODEV;
		goto out2;
	}
	ret = PTR_ERR(dev_fname);
	if (IS_ERR(dev_fname))
		goto out2;
	lock_kernel();
	ret = do_mount(dev_fname, dir_page, type_page, linux_flags, NULL);
	unlock_kernel();
	if (dev_fname)
		putname(dev_fname);
out2:
	putname(type_page);
out1:
	putname(dir_page);
out:
	return ret;
}

extern asmlinkage int sys_setsid(void);
extern asmlinkage int sys_setpgid(pid_t, pid_t);

asmlinkage int sunos_setpgrp(pid_t pid, pid_t pgid)
{
	int ret;

	/* So stupid... */
	if((!pid || pid == current->pid) &&
	   !pgid) {
		sys_setsid();
		ret = 0;
	} else {
		ret = sys_setpgid(pid, pgid);
	}
	return ret;
}

/* So stupid... */
extern asmlinkage int sys32_wait4(__kernel_pid_t32 pid,
				  u32 stat_addr, int options, u32 ru);

asmlinkage int sunos_wait4(__kernel_pid_t32 pid, u32 stat_addr, int options, u32 ru)
{
	int ret;

	ret = sys32_wait4((pid ? pid : ((__kernel_pid_t32)-1)),
			  stat_addr, options, ru);
	return ret;
}

extern int kill_pg(int, int, int);
asmlinkage int sunos_killpg(int pgrp, int sig)
{
	return kill_pg(pgrp, sig, 0);
}

asmlinkage int sunos_audit(void)
{
	printk ("sys_audit\n");
	return -1;
}

extern asmlinkage u32 sunos_gethostid(void)
{
	u32 ret;

	ret = (((u32)idprom->id_machtype << 24) | ((u32)idprom->id_sernum));

	return ret;
}

/* sysconf options, for SunOS compatibility */
#define   _SC_ARG_MAX             1
#define   _SC_CHILD_MAX           2
#define   _SC_CLK_TCK             3
#define   _SC_NGROUPS_MAX         4
#define   _SC_OPEN_MAX            5
#define   _SC_JOB_CONTROL         6
#define   _SC_SAVED_IDS           7
#define   _SC_VERSION             8

extern asmlinkage s32 sunos_sysconf (int name)
{
	s32 ret;

	switch (name){
	case _SC_ARG_MAX:
		ret = ARG_MAX;
		break;
	case _SC_CHILD_MAX:
		ret = CHILD_MAX;
		break;
	case _SC_CLK_TCK:
		ret = HZ;
		break;
	case _SC_NGROUPS_MAX:
		ret = NGROUPS_MAX;
		break;
	case _SC_OPEN_MAX:
		ret = OPEN_MAX;
		break;
	case _SC_JOB_CONTROL:
		ret = 1;	/* yes, we do support job control */
		break;
	case _SC_SAVED_IDS:
		ret = 1;	/* yes, we do support saved uids  */
		break;
	case _SC_VERSION:
		/* mhm, POSIX_VERSION is in /usr/include/unistd.h
		 * should it go on /usr/include/linux?
		 */
		ret = 199009;
		break;
	default:
		ret = -1;
		break;
	};
	return ret;
}

asmlinkage int sunos_semsys(int op, u32 arg1, u32 arg2, u32 arg3, u32 ptr)
{
	union semun arg4;
	int ret;

	switch (op) {
	case 0:
		/* Most arguments match on a 1:1 basis but cmd doesn't */
		switch(arg3) {
		case 4:
			arg3=GETPID; break;
		case 5:
			arg3=GETVAL; break;
		case 6:
			arg3=GETALL; break;
		case 3:
			arg3=GETNCNT; break;
		case 7:
			arg3=GETZCNT; break;
		case 8:
			arg3=SETVAL; break;
		case 9:
			arg3=SETALL; break;
		}
		/* sys_semctl(): */
		arg4.__pad=(void *)A(ptr); /* value to modify semaphore to */
		ret = sys_semctl((int)arg1, (int)arg2, (int)arg3, arg4);
		break;
	case 1:
		/* sys_semget(): */
		ret = sys_semget((key_t)arg1, (int)arg2, (int)arg3);
		break;
	case 2:
		/* sys_semop(): */
		ret = sys_semop((int)arg1, (struct sembuf *)A(arg2), (unsigned)arg3);
		break;
	default:
		ret = -EINVAL;
		break;
	};
	return ret;
}

struct msgbuf32 {
	s32 mtype;
	char mtext[1];
};

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

static inline int sunos_msqid_get(struct msqid_ds32 *user,
				  struct msqid_ds *kern)
{
	if(get_user(kern->msg_perm.key, &user->msg_perm.key)		||
	   __get_user(kern->msg_perm.uid, &user->msg_perm.uid)		||
	   __get_user(kern->msg_perm.gid, &user->msg_perm.gid)		||
	   __get_user(kern->msg_perm.cuid, &user->msg_perm.cuid)	||
	   __get_user(kern->msg_perm.cgid, &user->msg_perm.cgid)	||
	   __get_user(kern->msg_stime, &user->msg_stime)		||
	   __get_user(kern->msg_rtime, &user->msg_rtime)		||
	   __get_user(kern->msg_ctime, &user->msg_ctime)		||
	   __get_user(kern->msg_ctime, &user->msg_cbytes)		||
	   __get_user(kern->msg_ctime, &user->msg_qnum)			||
	   __get_user(kern->msg_ctime, &user->msg_qbytes)		||
	   __get_user(kern->msg_ctime, &user->msg_lspid)		||
	   __get_user(kern->msg_ctime, &user->msg_lrpid))
		return -EFAULT;
	return 0;
}

static inline int sunos_msqid_put(struct msqid_ds32 *user,
				  struct msqid_ds *kern)
{
	if(put_user(kern->msg_perm.key, &user->msg_perm.key)		||
	   __put_user(kern->msg_perm.uid, &user->msg_perm.uid)		||
	   __put_user(kern->msg_perm.gid, &user->msg_perm.gid)		||
	   __put_user(kern->msg_perm.cuid, &user->msg_perm.cuid)	||
	   __put_user(kern->msg_perm.cgid, &user->msg_perm.cgid)	||
	   __put_user(kern->msg_stime, &user->msg_stime)		||
	   __put_user(kern->msg_rtime, &user->msg_rtime)		||
	   __put_user(kern->msg_ctime, &user->msg_ctime)		||
	   __put_user(kern->msg_ctime, &user->msg_cbytes)		||
	   __put_user(kern->msg_ctime, &user->msg_qnum)			||
	   __put_user(kern->msg_ctime, &user->msg_qbytes)		||
	   __put_user(kern->msg_ctime, &user->msg_lspid)		||
	   __put_user(kern->msg_ctime, &user->msg_lrpid))
		return -EFAULT;
	return 0;
}

static inline int sunos_msgbuf_get(struct msgbuf32 *user, struct msgbuf *kern, int len)
{
	if(get_user(kern->mtype, &user->mtype)	||
	   __copy_from_user(kern->mtext, &user->mtext, len))
		return -EFAULT;
	return 0;
}

static inline int sunos_msgbuf_put(struct msgbuf32 *user, struct msgbuf *kern, int len)
{
	if(put_user(kern->mtype, &user->mtype)	||
	   __copy_to_user(user->mtext, kern->mtext, len))
		return -EFAULT;
	return 0;
}

asmlinkage int sunos_msgsys(int op, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
	struct sparc_stackf32 *sp;
	struct msqid_ds kds;
	struct msgbuf *kmbuf;
	mm_segment_t old_fs = get_fs();
	u32 arg5;
	int rval;

	switch(op) {
	case 0:
		rval = sys_msgget((key_t)arg1, (int)arg2);
		break;
	case 1:
		if(!sunos_msqid_get((struct msqid_ds32 *)A(arg3), &kds)) {
			set_fs(KERNEL_DS);
			rval = sys_msgctl((int)arg1, (int)arg2,
					  (struct msqid_ds *)A(arg3));
			set_fs(old_fs);
			if(!rval)
				rval = sunos_msqid_put((struct msqid_ds32 *)A(arg3),
						       &kds);
		} else
			rval = -EFAULT;
		break;
	case 2:
		rval = -EFAULT;
		kmbuf = (struct msgbuf *)kmalloc(sizeof(struct msgbuf) + arg3,
						 GFP_KERNEL);
		if(!kmbuf)
			break;
		sp = (struct sparc_stackf32 *)
			(current->thread.kregs->u_regs[UREG_FP] & 0xffffffffUL);
		if(get_user(arg5, &sp->xxargs[0])) {
			rval = -EFAULT;
			kfree(kmbuf);
			break;
		}
		set_fs(KERNEL_DS);
		rval = sys_msgrcv((int)arg1, kmbuf, (size_t)arg3,
				  (long)arg4, (int)arg5);
		set_fs(old_fs);
		if(!rval)
			rval = sunos_msgbuf_put((struct msgbuf32 *)A(arg2),
						kmbuf, arg3);
		kfree(kmbuf);
		break;
	case 3:
		rval = -EFAULT;
		kmbuf = (struct msgbuf *)kmalloc(sizeof(struct msgbuf) + arg3,
						 GFP_KERNEL);
		if(!kmbuf || sunos_msgbuf_get((struct msgbuf32 *)A(arg2),
					      kmbuf, arg3))
			break;
		set_fs(KERNEL_DS);
		rval = sys_msgsnd((int)arg1, kmbuf, (size_t)arg3, (int)arg4);
		set_fs(old_fs);
		kfree(kmbuf);
		break;
	default:
		rval = -EINVAL;
		break;
	}
	return rval;
}

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
                                                        
static inline int sunos_shmid_get(struct shmid_ds32 *user,
				  struct shmid_ds *kern)
{
	if(get_user(kern->shm_perm.key, &user->shm_perm.key)		||
	   __get_user(kern->shm_perm.uid, &user->shm_perm.uid)		||
	   __get_user(kern->shm_perm.gid, &user->shm_perm.gid)		||
	   __get_user(kern->shm_perm.cuid, &user->shm_perm.cuid)	||
	   __get_user(kern->shm_perm.cgid, &user->shm_perm.cgid)	||
	   __get_user(kern->shm_segsz, &user->shm_segsz)		||
	   __get_user(kern->shm_atime, &user->shm_atime)		||
	   __get_user(kern->shm_dtime, &user->shm_dtime)		||
	   __get_user(kern->shm_ctime, &user->shm_ctime)		||
	   __get_user(kern->shm_cpid, &user->shm_cpid)			||
	   __get_user(kern->shm_lpid, &user->shm_lpid)			||
	   __get_user(kern->shm_nattch, &user->shm_nattch))
		return -EFAULT;
	return 0;
}

static inline int sunos_shmid_put(struct shmid_ds32 *user,
				  struct shmid_ds *kern)
{
	if(put_user(kern->shm_perm.key, &user->shm_perm.key)		||
	   __put_user(kern->shm_perm.uid, &user->shm_perm.uid)		||
	   __put_user(kern->shm_perm.gid, &user->shm_perm.gid)		||
	   __put_user(kern->shm_perm.cuid, &user->shm_perm.cuid)	||
	   __put_user(kern->shm_perm.cgid, &user->shm_perm.cgid)	||
	   __put_user(kern->shm_segsz, &user->shm_segsz)		||
	   __put_user(kern->shm_atime, &user->shm_atime)		||
	   __put_user(kern->shm_dtime, &user->shm_dtime)		||
	   __put_user(kern->shm_ctime, &user->shm_ctime)		||
	   __put_user(kern->shm_cpid, &user->shm_cpid)			||
	   __put_user(kern->shm_lpid, &user->shm_lpid)			||
	   __put_user(kern->shm_nattch, &user->shm_nattch))
		return -EFAULT;
	return 0;
}

asmlinkage int sunos_shmsys(int op, u32 arg1, u32 arg2, u32 arg3)
{
	struct shmid_ds ksds;
	unsigned long raddr;
	mm_segment_t old_fs = get_fs();
	int rval;

	switch(op) {
	case 0:
		/* sys_shmat(): attach a shared memory area */
		rval = sys_shmat((int)arg1,(char *)A(arg2),(int)arg3,&raddr);
		if(!rval)
			rval = (int) raddr;
		break;
	case 1:
		/* sys_shmctl(): modify shared memory area attr. */
		if(!sunos_shmid_get((struct shmid_ds32 *)A(arg3), &ksds)) {
			set_fs(KERNEL_DS);
			rval = sys_shmctl((int)arg1,(int)arg2, &ksds);
			set_fs(old_fs);
			if(!rval)
				rval = sunos_shmid_put((struct shmid_ds32 *)A(arg3),
						       &ksds);
		} else
			rval = -EFAULT;
		break;
	case 2:
		/* sys_shmdt(): detach a shared memory area */
		rval = sys_shmdt((char *)A(arg1));
		break;
	case 3:
		/* sys_shmget(): get a shared memory area */
		rval = sys_shmget((key_t)arg1,(int)arg2,(int)arg3);
		break;
	default:
		rval = -EINVAL;
		break;
	};
	return rval;
}

extern asmlinkage long sparc32_open(const char * filename, int flags, int mode);

asmlinkage int sunos_open(u32 fname, int flags, int mode)
{
	const char *filename = (const char *)(long)fname;

	return sparc32_open(filename, flags, mode);
}

#define SUNOS_EWOULDBLOCK 35

/* see the sunos man page read(2v) for an explanation
   of this garbage. We use O_NDELAY to mark
   file descriptors that have been set non-blocking 
   using 4.2BSD style calls. (tridge) */

static inline int check_nonblock(int ret, int fd)
{
	if (ret == -EAGAIN) {
		struct file * file = fget(fd);
		if (file) {
			if (file->f_flags & O_NDELAY)
				ret = -SUNOS_EWOULDBLOCK;
			fput(file);
		}
	}
	return ret;
}

extern asmlinkage ssize_t sys_read(unsigned int fd, char *buf, unsigned long count);
extern asmlinkage ssize_t sys_write(unsigned int fd, char *buf, unsigned long count);
extern asmlinkage int sys_recv(int fd, void *ubuf, size_t size, unsigned flags);
extern asmlinkage int sys_send(int fd, void *buff, size_t len, unsigned flags);
extern asmlinkage int sys_accept(int fd, struct sockaddr *sa, int *addrlen);
extern asmlinkage int sys32_readv(u32 fd, u32 vector, s32 count);
extern asmlinkage int sys32_writev(u32 fd, u32 vector, s32 count);

asmlinkage int sunos_read(unsigned int fd, u32 buf, u32 count)
{
	int ret;

	ret = check_nonblock(sys_read(fd, (char *)A(buf), count), fd);
	return ret;
}

asmlinkage int sunos_readv(u32 fd, u32 vector, s32 count)
{
	int ret;

	ret = check_nonblock(sys32_readv(fd, vector, count), fd);
	return ret;
}

asmlinkage int sunos_write(unsigned int fd, u32 buf, u32 count)
{
	int ret;

	ret = check_nonblock(sys_write(fd, (char *)A(buf), count), fd);
	return ret;
}

asmlinkage int sunos_writev(u32 fd, u32 vector, s32 count)
{
	int ret;

	ret = check_nonblock(sys32_writev(fd, vector, count), fd);
	return ret;
}

asmlinkage int sunos_recv(int fd, u32 ubuf, int size, unsigned flags)
{
	int ret;

	ret = check_nonblock(sys_recv(fd, (void *)A(ubuf), size, flags), fd);
	return ret;
}

asmlinkage int sunos_send(int fd, u32 buff, int len, unsigned flags)
{
	int ret;

	ret = check_nonblock(sys_send(fd, (void *)A(buff), len, flags), fd);
	return ret;
}

extern asmlinkage int sys_setsockopt(int fd, int level, int optname,
				     char *optval, int optlen);

asmlinkage int sunos_socket(int family, int type, int protocol)
{
	int ret, one = 1;

	ret = sys_socket(family, type, protocol);
	if (ret < 0)
		goto out;

	sys_setsockopt(ret, SOL_SOCKET, SO_BSDCOMPAT,
		       (char *)&one, sizeof(one));
out:
	return ret;
}

asmlinkage int sunos_accept(int fd, u32 sa, u32 addrlen)
{
	int ret, one = 1;

	while (1) {
		ret = check_nonblock(sys_accept(fd, (struct sockaddr *)A(sa),
						(int *)A(addrlen)), fd);
		if (ret != -ENETUNREACH && ret != -EHOSTUNREACH)
			break;
	}
	if (ret < 0)
		goto out;

	sys_setsockopt(ret, SOL_SOCKET, SO_BSDCOMPAT,
		       (char *)&one, sizeof(one));
out:
	return ret;
}

#define SUNOS_SV_INTERRUPT 2

asmlinkage int sunos_sigaction (int sig, u32 act, u32 oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t32 mask;

		if (get_user((long)new_ka.sa.sa_handler, &((struct old_sigaction32 *)A(act))->sa_handler) ||
		    __get_user(new_ka.sa.sa_flags, &((struct old_sigaction32 *)A(act))->sa_flags))
			return -EFAULT;
		__get_user(mask, &((struct old_sigaction32 *)A(act))->sa_mask);
		new_ka.sa.sa_restorer = NULL;
		new_ka.ka_restorer = NULL;
		siginitset(&new_ka.sa.sa_mask, mask);
		new_ka.sa.sa_flags ^= SUNOS_SV_INTERRUPT;
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		old_ka.sa.sa_flags ^= SUNOS_SV_INTERRUPT;
		if (put_user((long)old_ka.sa.sa_handler, &((struct old_sigaction32 *)A(oact))->sa_handler) ||
		    __put_user(old_ka.sa.sa_flags, &((struct old_sigaction32 *)A(oact))->sa_flags))
			return -EFAULT;
		__put_user(old_ka.sa.sa_mask.sig[0], &((struct old_sigaction32 *)A(oact))->sa_mask);
	}

	return ret;
}

extern asmlinkage int sys_setsockopt(int fd, int level, int optname,
				     char *optval, int optlen);
extern asmlinkage int sys32_getsockopt(int fd, int level, int optname,
				     u32 optval, u32 optlen);

asmlinkage int sunos_setsockopt(int fd, int level, int optname, u32 optval,
				int optlen)
{
	int tr_opt = optname;
	int ret;

	if (level == SOL_IP) {
		/* Multicast socketopts (ttl, membership) */
		if (tr_opt >=2 && tr_opt <= 6)
			tr_opt += 30;
	}
	ret = sys_setsockopt(fd, level, tr_opt, (char *)A(optval), optlen);
	return ret;
}

asmlinkage int sunos_getsockopt(int fd, int level, int optname,
				u32 optval, u32 optlen)
{
	int tr_opt = optname;
	int ret;

	if (level == SOL_IP) {
		/* Multicast socketopts (ttl, membership) */
		if (tr_opt >=2 && tr_opt <= 6)
			tr_opt += 30;
	}
	ret = sys32_getsockopt(fd, level, tr_opt, optval, optlen);
	return ret;
}
