/*	$NetBSD: osf1_misc.c,v 1.14 1998/05/20 16:34:29 chs Exp $	*/
/*-
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * Additional Copyright (c) 1999 by Andrew Gallatin
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/selinfo.h>
#include <sys/pipe.h>		/* Must come after sys/selinfo.h */
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/utsname.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/cpuconf.h>
#include <machine/fpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/rpb.h>

#include <alpha/osf1/exec_ecoff.h>
#include <alpha/osf1/osf1_signal.h>
#include <alpha/osf1/osf1_proto.h>
#include <alpha/osf1/osf1_syscall.h>
#include <alpha/osf1/osf1_util.h>
#include <alpha/osf1/osf1.h>

static void cvtstat2osf1(struct stat *, struct osf1_stat *);
static int  osf2bsd_pathconf(int *);

static const char osf1_emul_path[] = "/compat/osf1";

/*
 * [ taken from the linux emulator ]
 * Search an alternate path before passing pathname arguments on
 * to system calls. Useful for keeping a separate 'emulation tree'.
 *
 * If cflag is set, we check if an attempt can be made to create
 * the named file, i.e. we check if the directory it should
 * be in exists.
 */
int
osf1_emul_find(struct thread *td, char *path, enum uio_seg pathseg,
    char **pbuf, int create)
{

	return (kern_alternate_path(td, osf1_emul_path, path, pathseg, pbuf,
	    create));
}


int
osf1_open(td, uap)
	struct thread *td;
	struct osf1_open_args *uap;
{
	char *path;
	int error;

	CHECKALTEXIST(td, uap->path, &path);

	/* XXX: translate flags */
	error = kern_open(td, path, UIO_SYSSPACE, uap->flags, uap->mode);
	free(path, M_TEMP);
	return (error);
}

extern long totalphysmem;

int
osf1_getsysinfo(td, uap)
	struct thread *td;
	struct osf1_getsysinfo_args *uap;
{
	int error, retval;
	int ncpus = 1; 	       /* XXX until SMP */
	int ophysmem;
	int unit;
	long percpu;
	long proctype;
	struct osf1_cpu_info cpuinfo;

	error = retval = 0;

	switch(uap->op) {
	case OSF_GET_MAX_UPROCS:
		error = copyout(&maxprocperuid, uap->buffer,
		    sizeof(maxprocperuid));
		retval = 1;
		break;
	case OSF_GET_PHYSMEM:
		ophysmem = totalphysmem * (PAGE_SIZE >> 10);	
		error = copyout(&ophysmem, uap->buffer,
		    sizeof(ophysmem));
		retval = 1;
		break;
	case OSF_GET_MAX_CPU:
	case OSF_GET_CPUS_IN_BOX:
		error = copyout(&ncpus, uap->buffer,
		    sizeof(ncpus));
		retval = 1;
		break;
	case OSF_GET_IEEE_FP_CONTROL:
		error = copyout(&td->td_pcb->pcb_fp_control,uap->buffer,
		    sizeof(td->td_pcb->pcb_fp_control));
		retval = 1;
		break;
	case OSF_GET_CPU_INFO:

		if (uap->nbytes < sizeof(cpuinfo))
			error = EINVAL;
		else {
			bzero(&cpuinfo, sizeof(cpuinfo));
			unit = alpha_pal_whami();
			cpuinfo.current_cpu = unit;
			cpuinfo.cpus_in_box = ncpus;
			cpuinfo.cpu_type = 
			    LOCATE_PCS(hwrpb, unit)->pcs_proc_type;
			cpuinfo.ncpus = ncpus;
			cpuinfo.cpus_present = ncpus;
			cpuinfo.cpus_running = ncpus;
			cpuinfo.cpu_binding = 1;
			cpuinfo.cpu_ex_binding = 0;
			cpuinfo.mhz = hwrpb->rpb_cc_freq / 1000000;
			error = copyout(&cpuinfo, uap->buffer,
			    sizeof(cpuinfo));
			retval = 1;
		}
		break;
	case OSF_GET_PROC_TYPE:
		if(uap->nbytes < sizeof(proctype))
			error = EINVAL;
		else {
			unit = alpha_pal_whami();
			proctype = LOCATE_PCS(hwrpb, unit)->pcs_proc_type;
			error = copyout (&proctype, uap->buffer,
			    sizeof(percpu));
			retval = 1;
		}
	break;
	case OSF_GET_HWRPB: {  /* note -- osf/1 doesn't have rpb_tbhint[8] */
		unsigned long rpb_size;
		rpb_size = (unsigned long)&hwrpb->rpb_tbhint -
		    (unsigned long)hwrpb;
		if(uap->nbytes < rpb_size){
			uprintf("nbytes = %ld, sizeof(struct rpb) = %ld\n",
			    uap->nbytes, rpb_size);
			error = EINVAL;
		}
		else {
			error = copyout(hwrpb, uap->buffer, rpb_size);
			retval = 1;
		}
	}
		break;
	case OSF_GET_PLATFORM_NAME:
		error = copyout(platform.model, uap->buffer, 
		    strlen(platform.model));
		retval = 1;
		break;
	default:
		printf("osf1_getsysinfo called with unknown op=%ld\n", uap->op);
		return EINVAL;
	}
	td->td_retval[0] = retval;
	return(error);
}


int
osf1_setsysinfo(td, uap)
	struct thread *td;
	struct osf1_setsysinfo_args *uap;
{
	int error;

	error = 0;

	switch(uap->op) {
	case OSF_SET_IEEE_FP_CONTROL:
	{
		u_int64_t temp, *fp_control;

		if ((error = copyin(uap->buffer, &temp, sizeof(temp))))
			break;
		fp_control = &td->td_pcb->pcb_fp_control;
		*fp_control = temp & IEEE_TRAP_ENABLE_MASK;
		break;
	}
	default:
		uprintf("osf1_setsysinfo called with op=%ld\n", uap->op);
		/*error = EINVAL;*/
	}
	return (error);
}


int
osf1_getrlimit(td, uap)
	struct thread *td;
	struct osf1_getrlimit_args *uap;
{
	struct rlimit bsd_rlim;
	struct proc *p;
	int which;

	if (uap->which >= OSF1_RLIMIT_NLIMITS)
		return (EINVAL);

	if (uap->which <= OSF1_RLIMIT_LASTCOMMON)
		which = uap->which;
	else if (uap->which == OSF1_RLIMIT_NOFILE)
		which = RLIMIT_NOFILE;
	else
		return (0);

	p = td->td_proc;
	PROC_LOCK(p);
	lim_rlimit(p, which, &bsd_rlim);
	PROC_UNLOCK(p);
	return (copyout(&bsd_rlim, uap->rlp, sizeof(bsd_rlim)));
}


int
osf1_setrlimit(td, uap)
	struct thread *td;
	struct osf1_setrlimit_args  *uap;
{
	struct rlimit bsd_rlim;
	int error, which;

	if (uap->which >= OSF1_RLIMIT_NLIMITS)
		return (EINVAL);

	if (uap->which <= OSF1_RLIMIT_LASTCOMMON)
		which = uap->which;
	else if (uap->which == OSF1_RLIMIT_NOFILE)
		which = RLIMIT_NOFILE;
	else
		return (0);

	error = copyin(uap->rlp, &bsd_rlim, sizeof(bsd_rlim));
	if (error)
		return (error);

	return (kern_setrlimit(td, which, &bsd_rlim));
}


/*
 *  As linux says, this is a total guess.
 */

int
osf1_set_program_attributes(td, uap)
	struct thread *td;
	struct osf1_set_program_attributes_args *uap;
{
	struct vmspace *vm = td->td_proc->p_vmspace;

	vm->vm_taddr = (caddr_t)uap->text_start;
	vm->vm_tsize = btoc(round_page(uap->text_len));
	vm->vm_daddr = (caddr_t)uap->bss_start;
	vm->vm_dsize = btoc(round_page(uap->bss_len));

	return(KERN_SUCCESS);
}


int
osf1_mmap(td, uap)
	struct thread *td;
	struct osf1_mmap_args *uap;
{
	struct mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ a;
	int retval;
	vm_map_t map;
	vm_offset_t addr, len, newaddr;

	GIANT_REQUIRED;

	a.addr = uap->addr;
	a.len = uap->len;
	a.prot = uap->prot;
	a.fd = uap->fd;
	a.pad = 0;
	a.pos = uap->pos;

	a.flags = 0;

	/*
	 *  OSF/1's mmap, unlike FreeBSD's, does its best to map memory at the
	 *  user's requested address, even if MAP_FIXED is not set.  Here we
	 *  try to replicate this behaviour as much as we can because some
	 *  applications (like /sbin/loader) depend on having things put as
	 *  close to where they've requested as possible.
	 */

	if (uap->addr != NULL)
		addr = round_page((vm_offset_t)a.addr);
	else
	/*
	 *  Try to use the apparent OSF/1 default placement of 0x10000 for
	 *  NULL addrs, this helps to prevent non-64 bit clean binaries from
	 *  SEGV'ing.
	 */
		addr = round_page((vm_offset_t)0x10000UL);
	len = (vm_offset_t)a.len;
	map = &td->td_proc->p_vmspace->vm_map;
	if (!vm_map_findspace(map, addr, len, &newaddr)) {
		a.addr = (caddr_t) newaddr;
		a.flags |= (MAP_FIXED);
	}
#ifdef DEBUG
	else
		uprintf("osf1_mmap:vm_map_findspace failed for: %p 0x%lx\n",
		    (caddr_t)addr, len);
#endif
	if (uap->flags & OSF1_MAP_SHARED)
		a.flags |= MAP_SHARED;
	if (uap->flags & OSF1_MAP_PRIVATE)
		a.flags |= MAP_PRIVATE;

	switch (uap->flags & OSF1_MAP_TYPE) {
	case OSF1_MAP_ANONYMOUS:
		a.flags |= MAP_ANON;
		break;
	case OSF1_MAP_FILE:
		a.flags |= MAP_FILE;
		break;
	default:
		return (EINVAL);
	}
	if (uap->flags & OSF1_MAP_FIXED)
		a.flags |= MAP_FIXED;
	if (uap->flags & OSF1_MAP_HASSEMAPHORE)
		a.flags |= MAP_HASSEMAPHORE;
	if (uap->flags & OSF1_MAP_INHERIT)
		return (EINVAL);
	if (uap->flags & OSF1_MAP_UNALIGNED)
		return (EINVAL);
	/*
	 *  Emulate an osf/1 bug:  Apparently, mmap'ed segments are always
	 *  readable even if the user doesn't or in PROT_READ.  This causes
	 *  some buggy programs to segv.
	 */
	a.prot |= PROT_READ;


	retval = mmap(td, &a);
#ifdef DEBUG
	uprintf(
	    "\nosf1_mmap: addr=%p (%p), len = 0x%lx, prot=0x%x, fd=%d, pad=0, pos=0x%lx",
	    uap->addr, a.addr,uap->len, uap->prot,
	    uap->fd, uap->pos);
	printf(" flags = 0x%x\n",uap->flags);
#endif
	return (retval);
}

int
osf1_msync(td, uap)
	struct thread *td;
	struct osf1_msync_args *uap;
{
	struct msync_args a;

	a.addr = uap->addr;
	a.len  = uap->len;
	a.flags = 0;
	if(uap->flags & OSF1_MS_ASYNC)
		a.flags |= MS_ASYNC;
	if(uap->flags & OSF1_MS_SYNC)
		a.flags |= MS_SYNC;
	if(uap->flags & OSF1_MS_INVALIDATE)
		a.flags |= MS_INVALIDATE;
	return(msync(td, &a));
}

struct osf1_stat {
	int32_t		st_dev;
	u_int32_t	st_ino;
	u_int32_t	st_mode;
	u_int16_t	st_nlink;
	u_int32_t	st_uid;
	u_int32_t	st_gid;
	int32_t		st_rdev;
	u_int64_t	st_size;
	int32_t		st_atime_sec;
	int32_t		st_spare1;
	int32_t		st_mtime_sec;
	int32_t		st_spare2;
	int32_t		st_ctime_sec;
	int32_t		st_spare3;
	u_int32_t	st_blksize;
	int32_t		st_blocks;
	u_int32_t	st_flags;
	u_int32_t	st_gen;
};

/*
 *  Get file status; this version follows links.
 */
/* ARGSUSED */
int
osf1_stat(td, uap)
	struct thread *td;
	struct osf1_stat_args *uap;
{
	struct stat sb;
	struct osf1_stat osb;
	char *path;
	int error;

	CHECKALTEXIST(td, uap->path, &path);

	error = kern_stat(td, path, UIO_SYSSPACE, &sb);
	free(path, M_TEMP);
	if (error)
		return (error);
	cvtstat2osf1(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)uap->ub, sizeof (osb));
	return (error);
}


/*
 *  Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
osf1_lstat(td, uap)
	struct thread *td;
	register struct osf1_lstat_args *uap;
{
	struct stat sb;
	struct osf1_stat osb;
	char *path;
	int error;

	CHECKALTEXIST(td, uap->path, &path);

	error = kern_lstat(td, path, UIO_SYSSPACE, &sb);
	free(path, M_TEMP);
	if (error)
		return (error);
	cvtstat2osf1(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)uap->ub, sizeof (osb));
	return (error);
}


/*
 *  Return status information about a file descriptor.
 */
int
osf1_fstat(td, uap)
	struct thread *td;
	register struct osf1_fstat_args *uap;
{
	struct osf1_stat oub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error)
		return (error);
	cvtstat2osf1(&ub, &oub);
	if (error == 0)
		error = copyout((caddr_t)&oub, (caddr_t)uap->sb,
		    sizeof (oub));
	return (error);
}


#define	bsd2osf_dev(dev)	(umajor(dev) << 20 | uminor(dev))
/*
 * Convert from a stat structure to an osf1 stat structure.
 */
static void
cvtstat2osf1(st, ost)
	struct stat *st;
	struct osf1_stat *ost;
{

	ost->st_dev = bsd2osf_dev(st->st_dev);
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode;
	ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid == -2 ? (u_int16_t) -2 : st->st_uid;
	ost->st_gid = st->st_gid == -2 ? (u_int16_t) -2 : st->st_gid;
	ost->st_rdev = bsd2osf_dev(st->st_rdev);
	ost->st_size = st->st_size;
	ost->st_atime_sec = st->st_atime;
	ost->st_spare1 = 0;
	ost->st_mtime_sec = st->st_mtime;
	ost->st_spare2 = 0;
	ost->st_ctime_sec = st->st_ctime;
	ost->st_spare3 = 0;
	ost->st_blksize = st->st_blksize;
	ost->st_blocks = st->st_blocks;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}


int
osf1_mknod(td, uap)
	struct thread *td;
	struct osf1_mknod_args *uap;
{

	printf("osf1_mknod no longer implemented\n");
	return ENOSYS;
}


int
osf1_access(td, uap)
	struct thread *td;
	struct osf1_access_args *uap;
{
	char *path;
	int error;

	CHECKALTEXIST(td, uap->path, &path);
	error = kern_access(td, path, UIO_SYSSPACE, uap->flags);
	free(path, M_TEMP);
	return (error);
}


struct osf1_flock   {
	short	l_type;
	short	l_whence;
	off_t	l_start;
	off_t	l_len;
	pid_t	l_pid;
	};

int
osf1_fcntl(td, uap)
	struct thread *td;
	struct osf1_fcntl_args *uap;
{
	int error;
	long tmp;
	struct osf1_flock osf_flock;
	struct flock bsd_flock;

	error = 0;

	switch (uap->cmd) {

	case F_SETFL:
		/* need to translate flags here */
		tmp = 0;
		if ((long)uap->arg & OSF1_FNONBLOCK)
			tmp |= FNONBLOCK;
		if ((long)uap->arg & OSF1_FAPPEND)
			tmp |= FAPPEND;
		if ((long)uap->arg & OSF1_FDEFER)
			tmp |= FDEFER;
		if ((long)uap->arg & OSF1_FASYNC)
			tmp |= FASYNC;
		if ((long)uap->arg & OSF1_FCREAT)
			tmp |= O_CREAT;
		if ((long)uap->arg & OSF1_FTRUNC)
			tmp |= O_TRUNC;
		if ((long)uap->arg & OSF1_FEXCL)
			tmp |= O_EXCL;
		if ((long)uap->arg & OSF1_FNDELAY)
			tmp |= FNDELAY;
		if ((long)uap->arg & OSF1_FSYNC)
			tmp |= FFSYNC;
		error = kern_fcntl(td, uap->fd, F_SETFL, tmp);
		break;

	case F_SETLK:
	case F_SETLKW:
	case F_GETLK:
		/*
		 *  The OSF/1 flock stucture has a different order than
		 *  the BSD one, but all else is the same.  We must
		 *  reorder the one we've gotten so that flock() groks it.
		 */
		error = copyin(uap->arg, &osf_flock, sizeof(osf_flock));
		if (error)
			return (error);
		bsd_flock.l_type = osf_flock.l_type;
		bsd_flock.l_whence = osf_flock.l_whence;
		bsd_flock.l_start = osf_flock.l_start;
		bsd_flock.l_len = osf_flock.l_len;
		bsd_flock.l_pid = osf_flock.l_pid;
		error = kern_fcntl(td, uap->fd, uap->cmd, (intptr_t)&bsd_flock);
/*		if (error) {
			printf("fcntl called with cmd=%d, args=0x%lx\n returns %d\n",uap->cmd,(long)uap->arg,error);
			printf("bsd_flock.l_type = 0x%x\n", bsd_flock.l_type);
			printf("bsd_flock.l_whence = 0x%x\n", bsd_flock.l_whence);
			printf("bsd_flock.l_start = 0x%lx\n", bsd_flock.l_start);
			printf("bsd_flock.l_len = 0x%lx\n", bsd_flock.l_len);
			printf("bsd_flock.l_pid = 0x%x\n", bsd_flock.l_pid);
		}
*/
		if ((uap->cmd == F_GETLK) && !error) {
			/*
			 * XXX: Why are we hardcoding F_UNLCK here instead of
			 * copying the structure members from bsd_flock?
			 */
			osf_flock.l_type = F_UNLCK;
			error = copyout(&osf_flock, uap->arg,
			    sizeof(osf_flock));
		}
		break;
	default:
		error = kern_fcntl(td, uap->fd, uap->cmd, (intptr_t)uap->arg);

		if ((uap->cmd == OSF1_F_GETFL) && !error ) {
			tmp = td->td_retval[0] & O_ACCMODE;
			if (td->td_retval[0] & FNONBLOCK)
				tmp |= OSF1_FNONBLOCK;
			if (td->td_retval[0] & FAPPEND)
				tmp |= OSF1_FAPPEND;
			if (td->td_retval[0] & FDEFER)
				tmp |= OSF1_FDEFER;
			if (td->td_retval[0] & FASYNC)
				tmp |= OSF1_FASYNC;
			if (td->td_retval[0] & O_CREAT)
				tmp |= OSF1_FCREAT;
			if (td->td_retval[0] & O_TRUNC)
				tmp |= OSF1_FTRUNC;
			if (td->td_retval[0] & O_EXCL)
				tmp |= OSF1_FEXCL;
			if (td->td_retval[0] & FNDELAY)
				tmp |= OSF1_FNDELAY;
			if (td->td_retval[0] & FFSYNC)
				tmp |= OSF1_FSYNC;
			td->td_retval[0] = tmp;
		}
	}

	return (error);
}


#if 0
int
osf1_fcntl(td, uap)
	struct thread *td;
	struct osf1_fcntl_args *uap;
{
	struct fcntl_args a;
	long tmp;
	int error;

	a.fd = uap->fd;

	switch (uap->cmd) {

	case OSF1_F_DUPFD:
		a.cmd = F_DUPFD;
		a.arg = (long)uap->arg;
		break;

	case OSF1_F_GETFD:
		a.cmd = F_GETFD;
		a.arg = (long)uap->arg;
		break;

	case OSF1_F_SETFD:
		a.cmd = F_SETFD;
		a.arg = (long)uap->arg;
		break;

	case OSF1_F_GETFL:
		a.cmd = F_GETFL;
		a.arg = (long)uap->arg;		/* ignored */
		break;

	case OSF1_F_SETFL:
		a.cmd = F_SETFL;
		tmp = 0;
		if ((long)uap->arg & OSF1_FAPPEND)
			tmp |= FAPPEND;
		if ((long)uap->arg & OSF1_FNONBLOCK)
			tmp |= FNONBLOCK;
		if ((long)uap->arg & OSF1_FASYNC)
			tmp |= FASYNC;
		if ((long)uap->arg & OSF1_FSYNC)
			tmp |= FFSYNC;
		a.arg = tmp;
		break;

	default:					/* XXX other cases */
		return (EINVAL);
	}

	error = fcntl(td, &a);

	if (error)
		return error;

	switch (uap->cmd) {
	case OSF1_F_GETFL:
		/* XXX */
		break;
	}

	return error;
}
#endif

int
osf1_socket(td, uap)
	struct thread *td;
	struct osf1_socket_args *uap;
{
	struct socket_args a;

	if (uap->type > AF_LINK)
		return (EINVAL);	/* XXX After AF_LINK, divergence. */

	a.domain = uap->domain;
	a.type = uap->type;
	a.protocol = uap->protocol;

	return socket(td, &a);
}


int
osf1_sendto(td, uap)
	struct thread *td;
	register struct osf1_sendto_args *uap;
{
	struct sendto_args a;

	if (uap->flags & ~0x7f)		/* unsupported flags */
		return (EINVAL);

	a.s = uap->s;
	a.buf = uap->buf;
	a.len = uap->len;
	a.flags = uap->flags;
	a.to = (caddr_t)uap->to;
	a.tolen = uap->tolen;

	return sendto(td, &a);
}


int
osf1_reboot(td, uap)
	struct thread *td;
	struct osf1_reboot_args *uap;
{
	struct reboot_args a;

	if (uap->opt & ~OSF1_RB_ALLFLAGS &&
	    uap->opt & (OSF1_RB_ALTBOOT|OSF1_RB_UNIPROC))
		return (EINVAL);

	a.opt = 0;

	if (uap->opt & OSF1_RB_ASKNAME)
		a.opt |= RB_ASKNAME;
	if (uap->opt & OSF1_RB_SINGLE)
		a.opt |= RB_SINGLE;
	if (uap->opt & OSF1_RB_NOSYNC)
		a.opt |= RB_NOSYNC;
	if (uap->opt & OSF1_RB_HALT)
		a.opt |= RB_HALT;
	if (uap->opt & OSF1_RB_INITNAME)
		a.opt |= RB_INITNAME;
	if (uap->opt & OSF1_RB_DFLTROOT)
		a.opt |= RB_DFLTROOT;

	return reboot(td, &a);
}


int
osf1_lseek(td, uap)
	struct thread *td;
	struct osf1_lseek_args *uap;
{
	struct lseek_args a;

	a.fd = uap->fd;
	a.pad = 0;
	a.offset = uap->offset;
	a.whence = uap->whence;

	return lseek(td, &a);
}


/*
 *  OSF/1 defines _POSIX_SAVED_IDS, which means that our normal
 *  setuid() won't work.
 *
 *  Instead, by P1003.1b-1993, setuid() is supposed to work like:
 *	If the process has appropriate [super-user] privileges, the
 *	    setuid() function sets the real user ID, effective user
 *	    ID, and the saved set-user-ID to uid.
 *	If the process does not have appropriate privileges, but uid
 *	    is equal to the real user ID or the saved set-user-ID, the
 *	    setuid() function sets the effective user ID to uid; the
 *	    real user ID and saved set-user-ID remain unchanged by
 *	    this function call.
 */
int
osf1_setuid(td, uap)
	struct thread *td;
	struct osf1_setuid_args *uap;
{
	struct proc *p;
	int error;
	uid_t uid;
	struct uidinfo *uip;
	struct ucred *newcred, *oldcred;

	p = td->td_proc;
	uid = uap->uid;
	newcred = crget();
	uip = uifind(uid);
	PROC_LOCK(p);
	oldcred = p->p_ucred;

	if ((error = suser_cred(p->p_ucred, SUSER_ALLOWJAIL)) != 0 &&
	    uid != oldcred->cr_ruid && uid != oldcred->cr_svuid) {
		PROC_UNLOCK(p);
		uifree(uip);
		crfree(newcred);
		return (error);
	}

	crcopy(newcred, oldcred);
	if (error == 0) {
		if (uid != oldcred->cr_ruid) {
			change_ruid(newcred, uip);
			setsugid(p);
		}
		if (oldcred->cr_svuid != uid) {
			change_svuid(newcred, uid);
			setsugid(p);
		}
	}
	if (newcred->cr_uid != uid) {
		change_euid(newcred, uip);
		setsugid(p);
	}
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	uifree(uip);
	crfree(oldcred);
	return (0);
}


/*
 *  OSF/1 defines _POSIX_SAVED_IDS, which means that our normal
 *  setgid() won't work.
 *
 *  If you change "uid" to "gid" in the discussion, above, about
 *  setuid(), you'll get a correct description of setgid().
 */
int
osf1_setgid(td, uap)
	struct thread *td;
	struct osf1_setgid_args *uap;
{
	struct proc *p;
	int error;
	gid_t gid;
	struct ucred *newcred, *oldcred;

	p = td->td_proc;
	gid = uap->gid;
	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;

	if (((error = suser_cred(p->p_ucred, SUSER_ALLOWJAIL)) != 0 ) &&
	    gid != oldcred->cr_rgid && gid != oldcred->cr_svgid) {
		PROC_UNLOCK(p);
		crfree(newcred);
		return (error);
	}

	crcopy(newcred, oldcred);
	if (error == 0) {
		if (gid != oldcred->cr_rgid) {
			change_rgid(newcred, gid);
			setsugid(p);
		}
		if (oldcred->cr_svgid != gid) {
			change_svgid(newcred, gid);
			setsugid(p);
		}
	}
	if (newcred->cr_groups[0] != gid) {
		change_egid(newcred, gid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
}


/*
 *  The structures end up being the same... but we can't be sure that
 *  the other word of our iov_len is zero!
 */
struct osf1_iovec {
	char	*iov_base;
	int	iov_len;
};

static int
osf1_copyinuio(struct osf1_iovec *iovp, u_int iovcnt, struct uio **uiop)
{
	struct osf1_iovec oiov;
	struct iovec *iov;
	struct uio *uio;
	u_int iovlen;
	int error, i;

	*uiop = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (EINVAL);
	iovlen = iovcnt * sizeof(struct iovec);
	uio = malloc(iovlen + sizeof *uio, M_IOV, M_WAITOK);
	iov = (struct iovec *)(uio + 1);
	for (i = 0; i < iovcnt; i++) {
		error = copyin(&iovp[i], &oiov, sizeof(struct osf1_iovec));
		if (error) {
			free(uio, M_IOV);
			return (error);
		}
		iov[i].iov_base = oiov.iov_base;
		iov[i].iov_len = oiov.iov_len;
	}
	uio->uio_iov = iov;
	uio->uio_iovcnt = iovcnt;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_offset = -1;
	uio->uio_resid = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iov->iov_len > INT_MAX - uio->uio_resid) {
			free(uio, M_IOV);
			return (EINVAL);
		}
		uio->uio_resid += iov->iov_len;
		iov++;
	}
	*uiop = uio;
	return (0);
}

int
osf1_readv(td, uap)
	struct thread *td;
	struct osf1_readv_args *uap;
{
	struct uio *auio;
	int error;

	error = osf1_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_readv(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}


int
osf1_writev(td, uap)
	struct thread *td;
	struct osf1_writev_args *uap;
{
	struct uio *auio;
	int error;

	error = osf1_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_writev(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}


/*
 *  More of the stupid off_t padding!
 */
int
osf1_truncate(td, uap)
	struct thread *td;
	struct osf1_truncate_args *uap;
{
	char *path;
	int error;

        CHECKALTEXIST(td, uap->path, &path);
	error = kern_truncate(td, path, UIO_SYSSPACE, uap->length);
	free(path, M_TEMP);
	return (error);
}


int
osf1_ftruncate(td, uap)
	struct thread *td;
	struct osf1_ftruncate_args *uap;
{
	struct ftruncate_args a;

	a.fd = uap->fd;
	a.pad = 0;
	a.length = uap->length;

	return ftruncate(td, &a);
}


static int
osf2bsd_pathconf(name)
	int *name;
{

	switch (*name) {
	case _OSF1_PC_LINK_MAX:
	case _OSF1_PC_MAX_CANON:
	case _OSF1_PC_MAX_INPUT:
	case _OSF1_PC_NAME_MAX:
		*name -= 10;
		break;

	case _OSF1_PC_PATH_MAX:
	case _OSF1_PC_PIPE_BUF:
		*name -= 9;

	case _OSF1_PC_NO_TRUNC:
		*name = _PC_NO_TRUNC;
		break;

	case _OSF1_PC_CHOWN_RESTRICTED:
		*name = _PC_CHOWN_RESTRICTED;
		break;

	case _OSF1_PC_VDISABLE:
		*name = _PC_VDISABLE;
		break;

	default:
		return (EINVAL);
	}
	return 0;
}


int
osf1_pathconf(td, uap)
	struct thread *td;
	struct osf1_pathconf_args *uap;
{
	char *path;
	int error;

	if (osf2bsd_pathconf(&uap->name))
		return (EINVAL);
	CHECKALTEXIST(td, uap->path, &path);
	error = kern_pathconf(td, path, UIO_SYSSPACE, uap->name);
	free(path, M_TEMP);
	return (error);
}


int
osf1_fpathconf(td, uap)
	struct thread *td;
	struct osf1_fpathconf_args *uap;
{

	if (osf2bsd_pathconf(&uap->name))
		return (EINVAL);
	else
		return (fpathconf(td, (void *)uap));
}


int
osf1_getrusage(td, uap)
	struct thread *td;
	struct osf1_getrusage_args *uap;
{
	struct osf1_rusage oru;
	struct rusage ru;
	int error;

	error = kern_getrusage(td, uap->who, &ru);
	if (error)
		return (error);
	TV_CP(ru.ru_utime, oru.ru_utime);
	TV_CP(ru.ru_stime, oru.ru_stime);
	bcopy(&(ru.ru_first), &(oru.ru_first),
	    (&(oru.ru_last) - &(oru.ru_first)));

	return (copyout((caddr_t)&oru, (caddr_t)uap->rusage,
	    sizeof (struct osf1_rusage)));
}


int
osf1_wait4(td, uap)
	struct thread *td;
	struct osf1_wait4_args *uap;
{
	int error, status;
	struct osf1_rusage oru;
	struct rusage ru, *rup;

	if (uap->rusage != NULL)
		rup = &ru;
	else
		rup = NULL;
	error = kern_wait(td, uap->pid, &status, uap->options, rup);
	if (error)
		return (error);
	if (uap->status != NULL)
		error = copyout(&status, uap->status, sizeof(status));
	if (uap->rusage != NULL && error == 0) {
		TV_CP(ru.ru_utime, oru.ru_utime);
		TV_CP(ru.ru_stime, oru.ru_stime);
		bcopy(&ru.ru_first, &oru.ru_first,
		    (&(oru.ru_last) - &(oru.ru_first)));
		error = copyout(&oru, uap->rusage, sizeof (struct osf1_rusage));
	}
	return (error);
}


int
osf1_madvise(td, uap)
	struct thread *td;
	struct osf1_madvise_args *uap;
{

	/* XXX */
	return EINVAL;
}


int
osf1_execve(td, uap)
	struct thread *td;
	struct osf1_execve_args *uap;
{
	struct image_args eargs;
	char *path;
	int error;

	CHECKALTEXIST(td, uap->path, &path);

	error = exec_copyin_args(&eargs, path, UIO_SYSSPACE, uap->argp,
	    uap->envp);
	free(path, M_TEMP);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL);
	exec_free_args(&eargs);
	return (error);
}


int
osf1_usleep_thread(td, uap)
	struct thread *td;
	struct osf1_usleep_thread_args *uap;
{
	int error, s, timo;
	struct osf1_timeval time;
	struct timeval difftv, endtv, sleeptv, tv;

	if ((error = copyin(uap->sleep, &time, sizeof time)))
		return (error);

	sleeptv.tv_sec =  (u_long)time.tv_sec;
	sleeptv.tv_usec = (u_long)time.tv_usec;
	timo = tvtohz(&sleeptv);

	/*
	 *  Some callers use usleep(0) as a sort of thread-yield so make
	 *  sure that the timeout is non-zero.
	 */

	if (timo == 0)
		timo = 1;
	s = splclock();
	microtime(&tv);
	splx(s);

	tsleep(td, PUSER|PCATCH, "OSF/1", timo);

	if (uap->slept != NULL) {
		s = splclock();
		microtime(&endtv);
		timersub(&time, &endtv, &difftv);
		splx(s);
		if (tv.tv_sec < 0 || tv.tv_usec < 0)
			tv.tv_sec = tv.tv_usec = 0;
		TV_CP(difftv, time)
		error = copyout(&time, uap->slept, sizeof time);
	}
	return (error);
}


int
osf1_gettimeofday(td, uap)
	struct thread *td;
	register struct osf1_gettimeofday_args *uap;
{
	int error;
	struct timeval atv;
	struct timezone tz;
	struct osf1_timeval otv;

	error = 0;

	if (uap->tp) {
		microtime(&atv);
		otv.tv_sec = atv.tv_sec;
		otv.tv_usec = atv.tv_usec;
		if ((error = copyout((caddr_t)&otv, (caddr_t)uap->tp,
		    sizeof (otv))))
			return (error);
	}
	if (uap->tzp) {
		tz.tz_minuteswest = tz_minuteswest;
		tz.tz_dsttime = tz_dsttime;
		error = copyout((caddr_t)&tz, (caddr_t)uap->tzp, sizeof (tz));
	}
	return (error);
}


int
osf1_select(td, uap)
	struct thread *td;
	register struct osf1_select_args *uap;
{
	struct osf1_timeval otv;
	struct timeval tv, *tvp;
	int error;

	if (uap->tv != NULL) {
		error = copyin(uap->tv, &otv, sizeof(otv));
		if (error)
			return (error);
		TV_CP(otv, tv);
		tvp = &tv;
	} else
		tvp = NULL;
	return (kern_select(td, uap->nd, uap->in, uap->ou, uap->ex, tvp));
}


int
osf1_setitimer(td, uap)
	struct thread *td;
	struct osf1_setitimer_args *uap;
{
	struct itimerval itv, oitv;
	struct osf1_itimerval otv;
	int error;

	error = copyin(uap->itv, &otv, sizeof(otv));
	if (error) {
		printf("%s(%d): error = %d\n", __FILE__, __LINE__, error);
		return (error);
	}
	TV_CP(otv.it_interval, itv.it_interval);
	TV_CP(otv.it_value, itv.it_value);
	error = kern_setitimer(td, uap->which, &itv, &oitv);
	if (error || uap->oitv == NULL)
		return (error);

	TV_CP(oitv.it_interval, otv.it_interval);
	TV_CP(oitv.it_value, otv.it_value);
	error = copyout(&otv, uap->oitv, sizeof(otv));
	if (error)
		printf("%s(%d): error = %d\n", __FILE__, __LINE__, error);
	return (error);
}


int
osf1_getitimer(td, uap)
	struct thread *td;
	struct osf1_getitimer_args *uap;
{
	struct itimerval itv;
	struct osf1_itimerval otv;
	int error;

	error = kern_getitimer(td, uap->which, &itv);
	if (error)
		return (error);
	TV_CP(itv.it_interval, otv.it_interval);
	TV_CP(itv.it_value, otv.it_value);
	error = copyout(&otv, uap->itv, sizeof(otv));
	if (error)
		printf("%s(%d): error = %d\n", __FILE__, __LINE__, error);
	return (error);
}


int
osf1_proplist_syscall(td, uap)
	struct thread *td;
	struct osf1_proplist_syscall_args *uap;
{

	return(EOPNOTSUPP);
}


int
osf1_ntpgettime(td, uap)
	struct thread *td;
	struct  osf1_ntpgettime_args *uap;
{

	return(ENOSYS);
}


int
osf1_ntpadjtime(td, uap)
	struct thread *td;
	struct  osf1_ntpadjtime_args *uap;
{

	return(ENOSYS);
}


int
osf1_setpgrp(td, uap)
	struct thread *td;
	struct  osf1_setpgrp_args *uap;
{

	return(setpgid(td, (struct setpgid_args *)uap));
}


int
osf1_uswitch(td, uap)
	struct thread *td;
	struct osf1_uswitch_args *uap;
{
	struct proc *p;
	int rv;
	vm_map_entry_t entry;
	vm_offset_t zero;

	GIANT_REQUIRED;
	p = td->td_proc;
	zero = 0;

	if (uap->cmd == OSF1_USC_GET) {
		if (vm_map_lookup_entry(&(p->p_vmspace->vm_map), 0, &entry))
			td->td_retval[0] =  OSF1_USW_NULLP;
		else
			td->td_retval[0] =  0;
		return(KERN_SUCCESS);
	} else if (uap->cmd == OSF1_USC_SET)
		if (uap->mask & OSF1_USW_NULLP) {
			rv = vm_mmap(&(p->p_vmspace->vm_map), &zero, PAGE_SIZE,
			    VM_PROT_READ, VM_PROT_ALL,
			    MAP_PRIVATE | MAP_FIXED | MAP_ANON, NULL, 0);
			if (!rv)
				return(KERN_SUCCESS);
			else {
				printf(
				    "osf1_uswitch:vm_mmap of zero page failed with status %d\n",
				    rv);
				return(rv);
			}
		}
	return(EINVAL);
}


int
osf1_classcntl(td, uap)
	struct thread *td;
	struct  osf1_classcntl_args *uap;
{

	return(EACCES); /* class scheduling not enabled */
}


struct osf1_tbl_loadavg
{
	union {
		long   l[3];
		double d[3];
	} tl_avenrun;
	int  tl_lscale;
	long tl_mach_factor[3]; /* ???? */
};

struct osf1_tbl_sysinfo {
	long si_user;
	long si_nice;
	long si_sys;
	long si_idle;
	long si_hz;
	long si_phz;
	long si_boottime;
	long wait;
};

#define TBL_LOADAVG	3
#define TBL_SYSINFO    12

int
osf1_table(td, uap)
	struct thread *td;
	struct  osf1_table_args /*{
				long id;
				long index;
				void *addr;
				long nel;
				u_long lel;
				}*/ *uap;
{
	int retval;
	struct osf1_tbl_loadavg ld;
	struct osf1_tbl_sysinfo si;

	retval = 0;

	switch(uap->id) {
	case TBL_LOADAVG: /* xemacs wants this */
		if ((uap->index != 0) || (uap->nel != 1))
			retval = EINVAL;
		bcopy(&averunnable, &ld, sizeof(averunnable));
		ld.tl_lscale = (u_int)averunnable.fscale;
		retval = copyout(&ld, uap->addr, sizeof(ld));
		break;
	case TBL_SYSINFO:
		if ((uap->index != 0) || (uap->nel != 1))
			retval = EINVAL;
		bzero(&si, sizeof(si));
#if 0
		si.si_user = cp_time[CP_USER];
		si.si_nice = cp_time[CP_NICE];
		si.si_sys  = cp_time[CP_SYS];
		si.si_idle = cp_time[CP_IDLE];
		si.wait    = cp_time[CP_INTR];
#endif
		si.si_hz = hz;
		si.si_phz = profhz;
		si.si_boottime = boottime.tv_sec;
		retval = copyout(&si, uap->addr, sizeof(si));
		break;
	default:
		printf("osf1_table: %ld, %ld, %p, %ld %ld\n",
		    uap->id, uap->index, uap->addr, uap->nel, uap->lel);
		retval = EINVAL;
	}
	return retval;
}


/*
 * MPSAFE
 */
int
osf1_sysinfo(td, uap)
	struct thread *td;
	struct  osf1_sysinfo_args /*{
				int cmd;
				char *buf;
				long count;
				}*/ *uap;
{
	int name[2], retval;
	size_t bytes, len;
	char *string;

	string = NULL;

	switch(uap->cmd) {
	case 1: /* OS */
		string = "OSF1";
		break;
	case 2:	/* hostname, from ogethostname */
		len = uap->count;
		name[0] = CTL_KERN;
		name[1] = KERN_HOSTNAME;
		mtx_lock(&Giant);
		retval = userland_sysctl(td, name, 2, uap->buf, &len,
					1, 0, 0, &bytes, 0);
		mtx_unlock(&Giant);
		td->td_retval[0] =  bytes;
		return(retval);
		break;
	case 3: /* release of osf1 */
		string = "V4.0";
		break;
	case 4: /* minor version of osf1 */
		string = "878";
		break;
	case 5: /* machine or arch */
	case 6:
		string = "alpha";
		break;
	case 7: /* serial number, real osf1 returns 0! */
		string = "0";
		break;
	case 8: /* HW vendor */
		string = "Digital";
		break;
	case 9: /* dunno, this is what du does.. */
		return(ENOSYS);
		break;
	default:
		return(EINVAL);
	}
	bytes = min(uap->count, strlen(string)+1);
	copyout(string, uap->buf, bytes);
	td->td_retval[0] =  bytes;
	return(0);
}
