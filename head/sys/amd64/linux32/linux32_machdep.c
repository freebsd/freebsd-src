/*-
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2000 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/clock.h>
#include <sys/imgact.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>
#include <sys/wait.h>

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <compat/freebsd32/freebsd32_util.h>
#include <amd64/linux32/linux.h>
#include <amd64/linux32/linux32_proto.h>
#include <compat/linux/linux_ipc.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_emul.h>

struct l_old_select_argv {
	l_int		nfds;
	l_uintptr_t	readfds;
	l_uintptr_t	writefds;
	l_uintptr_t	exceptfds;
	l_uintptr_t	timeout;
} __packed;

int
linux_to_bsd_sigaltstack(int lsa)
{
	int bsa = 0;

	if (lsa & LINUX_SS_DISABLE)
		bsa |= SS_DISABLE;
	if (lsa & LINUX_SS_ONSTACK)
		bsa |= SS_ONSTACK;
	return (bsa);
}

static int	linux_mmap_common(struct thread *td, l_uintptr_t addr,
		    l_size_t len, l_int prot, l_int flags, l_int fd,
		    l_loff_t pos);

int
bsd_to_linux_sigaltstack(int bsa)
{
	int lsa = 0;

	if (bsa & SS_DISABLE)
		lsa |= LINUX_SS_DISABLE;
	if (bsa & SS_ONSTACK)
		lsa |= LINUX_SS_ONSTACK;
	return (lsa);
}

static void
bsd_to_linux_rusage(struct rusage *ru, struct l_rusage *lru)
{

	lru->ru_utime.tv_sec = ru->ru_utime.tv_sec;
	lru->ru_utime.tv_usec = ru->ru_utime.tv_usec;
	lru->ru_stime.tv_sec = ru->ru_stime.tv_sec;
	lru->ru_stime.tv_usec = ru->ru_stime.tv_usec;
	lru->ru_maxrss = ru->ru_maxrss;
	lru->ru_ixrss = ru->ru_ixrss;
	lru->ru_idrss = ru->ru_idrss;
	lru->ru_isrss = ru->ru_isrss;
	lru->ru_minflt = ru->ru_minflt;
	lru->ru_majflt = ru->ru_majflt;
	lru->ru_nswap = ru->ru_nswap;
	lru->ru_inblock = ru->ru_inblock;
	lru->ru_oublock = ru->ru_oublock;
	lru->ru_msgsnd = ru->ru_msgsnd;
	lru->ru_msgrcv = ru->ru_msgrcv;
	lru->ru_nsignals = ru->ru_nsignals;
	lru->ru_nvcsw = ru->ru_nvcsw;
	lru->ru_nivcsw = ru->ru_nivcsw;
}

int
linux_execve(struct thread *td, struct linux_execve_args *args)
{
	struct image_args eargs;
	char *path;
	int error;

	LCONVPATHEXIST(td, args->path, &path);

#ifdef DEBUG
	if (ldebug(execve))
		printf(ARGS(execve, "%s"), path);
#endif

	error = freebsd32_exec_copyin_args(&eargs, path, UIO_SYSSPACE,
	    args->argp, args->envp);
	free(path, M_TEMP);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL);
	if (error == 0)
		/* Linux process can execute FreeBSD one, do not attempt
		 * to create emuldata for such process using
		 * linux_proc_init, this leads to a panic on KASSERT
		 * because such process has p->p_emuldata == NULL.
		 */
		if (SV_PROC_ABI(td->td_proc) == SV_ABI_LINUX)
			error = linux_proc_init(td, 0, 0);
	return (error);
}

CTASSERT(sizeof(struct l_iovec32) == 8);

static int
linux32_copyinuio(struct l_iovec32 *iovp, l_ulong iovcnt, struct uio **uiop)
{
	struct l_iovec32 iov32;
	struct iovec *iov;
	struct uio *uio;
	uint32_t iovlen;
	int error, i;

	*uiop = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (EINVAL);
	iovlen = iovcnt * sizeof(struct iovec);
	uio = malloc(iovlen + sizeof(*uio), M_IOV, M_WAITOK);
	iov = (struct iovec *)(uio + 1);
	for (i = 0; i < iovcnt; i++) {
		error = copyin(&iovp[i], &iov32, sizeof(struct l_iovec32));
		if (error) {
			free(uio, M_IOV);
			return (error);
		}
		iov[i].iov_base = PTRIN(iov32.iov_base);
		iov[i].iov_len = iov32.iov_len;
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
linux32_copyiniov(struct l_iovec32 *iovp32, l_ulong iovcnt, struct iovec **iovp,
    int error)
{
	struct l_iovec32 iov32;
	struct iovec *iov;
	uint32_t iovlen;
	int i;

	*iovp = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (error);
	iovlen = iovcnt * sizeof(struct iovec);
	iov = malloc(iovlen, M_IOV, M_WAITOK);
	for (i = 0; i < iovcnt; i++) {
		error = copyin(&iovp32[i], &iov32, sizeof(struct l_iovec32));
		if (error) {
			free(iov, M_IOV);
			return (error);
		}
		iov[i].iov_base = PTRIN(iov32.iov_base);
		iov[i].iov_len = iov32.iov_len;
	}
	*iovp = iov;
	return(0);

}

int
linux_readv(struct thread *td, struct linux_readv_args *uap)
{
	struct uio *auio;
	int error;

	error = linux32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_readv(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

int
linux_writev(struct thread *td, struct linux_writev_args *uap)
{
	struct uio *auio;
	int error;

	error = linux32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_writev(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

struct l_ipc_kludge {
	l_uintptr_t msgp;
	l_long msgtyp;
} __packed;

int
linux_ipc(struct thread *td, struct linux_ipc_args *args)
{

	switch (args->what & 0xFFFF) {
	case LINUX_SEMOP: {
		struct linux_semop_args a;

		a.semid = args->arg1;
		a.tsops = args->ptr;
		a.nsops = args->arg2;
		return (linux_semop(td, &a));
	}
	case LINUX_SEMGET: {
		struct linux_semget_args a;

		a.key = args->arg1;
		a.nsems = args->arg2;
		a.semflg = args->arg3;
		return (linux_semget(td, &a));
	}
	case LINUX_SEMCTL: {
		struct linux_semctl_args a;
		int error;

		a.semid = args->arg1;
		a.semnum = args->arg2;
		a.cmd = args->arg3;
		error = copyin(args->ptr, &a.arg, sizeof(a.arg));
		if (error)
			return (error);
		return (linux_semctl(td, &a));
	}
	case LINUX_MSGSND: {
		struct linux_msgsnd_args a;

		a.msqid = args->arg1;
		a.msgp = args->ptr;
		a.msgsz = args->arg2;
		a.msgflg = args->arg3;
		return (linux_msgsnd(td, &a));
	}
	case LINUX_MSGRCV: {
		struct linux_msgrcv_args a;

		a.msqid = args->arg1;
		a.msgsz = args->arg2;
		a.msgflg = args->arg3;
		if ((args->what >> 16) == 0) {
			struct l_ipc_kludge tmp;
			int error;

			if (args->ptr == 0)
				return (EINVAL);
			error = copyin(args->ptr, &tmp, sizeof(tmp));
			if (error)
				return (error);
			a.msgp = PTRIN(tmp.msgp);
			a.msgtyp = tmp.msgtyp;
		} else {
			a.msgp = args->ptr;
			a.msgtyp = args->arg5;
		}
		return (linux_msgrcv(td, &a));
	}
	case LINUX_MSGGET: {
		struct linux_msgget_args a;

		a.key = args->arg1;
		a.msgflg = args->arg2;
		return (linux_msgget(td, &a));
	}
	case LINUX_MSGCTL: {
		struct linux_msgctl_args a;

		a.msqid = args->arg1;
		a.cmd = args->arg2;
		a.buf = args->ptr;
		return (linux_msgctl(td, &a));
	}
	case LINUX_SHMAT: {
		struct linux_shmat_args a;

		a.shmid = args->arg1;
		a.shmaddr = args->ptr;
		a.shmflg = args->arg2;
		a.raddr = PTRIN((l_uint)args->arg3);
		return (linux_shmat(td, &a));
	}
	case LINUX_SHMDT: {
		struct linux_shmdt_args a;

		a.shmaddr = args->ptr;
		return (linux_shmdt(td, &a));
	}
	case LINUX_SHMGET: {
		struct linux_shmget_args a;

		a.key = args->arg1;
		a.size = args->arg2;
		a.shmflg = args->arg3;
		return (linux_shmget(td, &a));
	}
	case LINUX_SHMCTL: {
		struct linux_shmctl_args a;

		a.shmid = args->arg1;
		a.cmd = args->arg2;
		a.buf = args->ptr;
		return (linux_shmctl(td, &a));
	}
	default:
		break;
	}

	return (EINVAL);
}

int
linux_old_select(struct thread *td, struct linux_old_select_args *args)
{
	struct l_old_select_argv linux_args;
	struct linux_select_args newsel;
	int error;

#ifdef DEBUG
	if (ldebug(old_select))
		printf(ARGS(old_select, "%p"), args->ptr);
#endif

	error = copyin(args->ptr, &linux_args, sizeof(linux_args));
	if (error)
		return (error);

	newsel.nfds = linux_args.nfds;
	newsel.readfds = PTRIN(linux_args.readfds);
	newsel.writefds = PTRIN(linux_args.writefds);
	newsel.exceptfds = PTRIN(linux_args.exceptfds);
	newsel.timeout = PTRIN(linux_args.timeout);
	return (linux_select(td, &newsel));
}

int
linux_set_cloned_tls(struct thread *td, void *desc)
{
	struct user_segment_descriptor sd;
	struct l_user_desc info;
	struct pcb *pcb;
	int error;
	int a[2];

	error = copyin(desc, &info, sizeof(struct l_user_desc));
	if (error) {
		printf(LMSG("copyin failed!"));
	} else {
		/* We might copy out the entry_number as GUGS32_SEL. */
		info.entry_number = GUGS32_SEL;
		error = copyout(&info, desc, sizeof(struct l_user_desc));
		if (error)
			printf(LMSG("copyout failed!"));

		a[0] = LINUX_LDT_entry_a(&info);
		a[1] = LINUX_LDT_entry_b(&info);

		memcpy(&sd, &a, sizeof(a));
#ifdef DEBUG
		if (ldebug(clone))
			printf("Segment created in clone with "
			    "CLONE_SETTLS: lobase: %x, hibase: %x, "
			    "lolimit: %x, hilimit: %x, type: %i, "
			    "dpl: %i, p: %i, xx: %i, long: %i, "
			    "def32: %i, gran: %i\n", sd.sd_lobase,
			    sd.sd_hibase, sd.sd_lolimit, sd.sd_hilimit,
			    sd.sd_type, sd.sd_dpl, sd.sd_p, sd.sd_xx,
			    sd.sd_long, sd.sd_def32, sd.sd_gran);
#endif
		pcb = td->td_pcb;
		pcb->pcb_gsbase = (register_t)info.base_addr;
/* XXXKIB	pcb->pcb_gs32sd = sd; */
		td->td_frame->tf_gs = GSEL(GUGS32_SEL, SEL_UPL);
		set_pcb_flags(pcb, PCB_GS32BIT | PCB_32BIT);
	}

	return (error);
}

int
linux_set_upcall_kse(struct thread *td, register_t stack)
{

	td->td_frame->tf_rsp = stack;

	return (0);
}

#define STACK_SIZE  (2 * 1024 * 1024)
#define GUARD_SIZE  (4 * PAGE_SIZE)

int
linux_mmap2(struct thread *td, struct linux_mmap2_args *args)
{

#ifdef DEBUG
	if (ldebug(mmap2))
		printf(ARGS(mmap2, "0x%08x, %d, %d, 0x%08x, %d, %d"),
		    args->addr, args->len, args->prot,
		    args->flags, args->fd, args->pgoff);
#endif

	return (linux_mmap_common(td, PTROUT(args->addr), args->len, args->prot,
		args->flags, args->fd, (uint64_t)(uint32_t)args->pgoff *
		PAGE_SIZE));
}

int
linux_mmap(struct thread *td, struct linux_mmap_args *args)
{
	int error;
	struct l_mmap_argv linux_args;

	error = copyin(args->ptr, &linux_args, sizeof(linux_args));
	if (error)
		return (error);

#ifdef DEBUG
	if (ldebug(mmap))
		printf(ARGS(mmap, "0x%08x, %d, %d, 0x%08x, %d, %d"),
		    linux_args.addr, linux_args.len, linux_args.prot,
		    linux_args.flags, linux_args.fd, linux_args.pgoff);
#endif

	return (linux_mmap_common(td, linux_args.addr, linux_args.len,
	    linux_args.prot, linux_args.flags, linux_args.fd,
	    (uint32_t)linux_args.pgoff));
}

static int
linux_mmap_common(struct thread *td, l_uintptr_t addr, l_size_t len, l_int prot,
    l_int flags, l_int fd, l_loff_t pos)
{
	struct proc *p = td->td_proc;
	struct mmap_args /* {
		caddr_t addr;
		size_t len;
		int prot;
		int flags;
		int fd;
		long pad;
		off_t pos;
	} */ bsd_args;
	int error;
	struct file *fp;

	error = 0;
	bsd_args.flags = 0;
	fp = NULL;

	/*
	 * Linux mmap(2):
	 * You must specify exactly one of MAP_SHARED and MAP_PRIVATE
	 */
	if (!((flags & LINUX_MAP_SHARED) ^ (flags & LINUX_MAP_PRIVATE)))
		return (EINVAL);

	if (flags & LINUX_MAP_SHARED)
		bsd_args.flags |= MAP_SHARED;
	if (flags & LINUX_MAP_PRIVATE)
		bsd_args.flags |= MAP_PRIVATE;
	if (flags & LINUX_MAP_FIXED)
		bsd_args.flags |= MAP_FIXED;
	if (flags & LINUX_MAP_ANON) {
		/* Enforce pos to be on page boundary, then ignore. */
		if ((pos & PAGE_MASK) != 0)
			return (EINVAL);
		pos = 0;
		bsd_args.flags |= MAP_ANON;
	} else
		bsd_args.flags |= MAP_NOSYNC;
	if (flags & LINUX_MAP_GROWSDOWN)
		bsd_args.flags |= MAP_STACK;

	/*
	 * PROT_READ, PROT_WRITE, or PROT_EXEC implies PROT_READ and PROT_EXEC
	 * on Linux/i386. We do this to ensure maximum compatibility.
	 * Linux/ia64 does the same in i386 emulation mode.
	 */
	bsd_args.prot = prot;
	if (bsd_args.prot & (PROT_READ | PROT_WRITE | PROT_EXEC))
		bsd_args.prot |= PROT_READ | PROT_EXEC;

	/* Linux does not check file descriptor when MAP_ANONYMOUS is set. */
	bsd_args.fd = (bsd_args.flags & MAP_ANON) ? -1 : fd;
	if (bsd_args.fd != -1) {
		/*
		 * Linux follows Solaris mmap(2) description:
		 * The file descriptor fildes is opened with
		 * read permission, regardless of the
		 * protection options specified.
		 */

		if ((error = fget(td, bsd_args.fd, CAP_MMAP, &fp)) != 0)
			return (error);
		if (fp->f_type != DTYPE_VNODE) {
			fdrop(fp, td);
			return (EINVAL);
		}

		/* Linux mmap() just fails for O_WRONLY files */
		if (!(fp->f_flag & FREAD)) {
			fdrop(fp, td);
			return (EACCES);
		}

		fdrop(fp, td);
	}

	if (flags & LINUX_MAP_GROWSDOWN) {
		/*
		 * The Linux MAP_GROWSDOWN option does not limit auto
		 * growth of the region.  Linux mmap with this option
		 * takes as addr the inital BOS, and as len, the initial
		 * region size.  It can then grow down from addr without
		 * limit.  However, Linux threads has an implicit internal
		 * limit to stack size of STACK_SIZE.  Its just not
		 * enforced explicitly in Linux.  But, here we impose
		 * a limit of (STACK_SIZE - GUARD_SIZE) on the stack
		 * region, since we can do this with our mmap.
		 *
		 * Our mmap with MAP_STACK takes addr as the maximum
		 * downsize limit on BOS, and as len the max size of
		 * the region.  It then maps the top SGROWSIZ bytes,
		 * and auto grows the region down, up to the limit
		 * in addr.
		 *
		 * If we don't use the MAP_STACK option, the effect
		 * of this code is to allocate a stack region of a
		 * fixed size of (STACK_SIZE - GUARD_SIZE).
		 */

		if ((caddr_t)PTRIN(addr) + len > p->p_vmspace->vm_maxsaddr) {
			/*
			 * Some Linux apps will attempt to mmap
			 * thread stacks near the top of their
			 * address space.  If their TOS is greater
			 * than vm_maxsaddr, vm_map_growstack()
			 * will confuse the thread stack with the
			 * process stack and deliver a SEGV if they
			 * attempt to grow the thread stack past their
			 * current stacksize rlimit.  To avoid this,
			 * adjust vm_maxsaddr upwards to reflect
			 * the current stacksize rlimit rather
			 * than the maximum possible stacksize.
			 * It would be better to adjust the
			 * mmap'ed region, but some apps do not check
			 * mmap's return value.
			 */
			PROC_LOCK(p);
			p->p_vmspace->vm_maxsaddr = (char *)LINUX32_USRSTACK -
			    lim_cur(p, RLIMIT_STACK);
			PROC_UNLOCK(p);
		}

		/*
		 * This gives us our maximum stack size and a new BOS.
		 * If we're using VM_STACK, then mmap will just map
		 * the top SGROWSIZ bytes, and let the stack grow down
		 * to the limit at BOS.  If we're not using VM_STACK
		 * we map the full stack, since we don't have a way
		 * to autogrow it.
		 */
		if (len > STACK_SIZE - GUARD_SIZE) {
			bsd_args.addr = (caddr_t)PTRIN(addr);
			bsd_args.len = len;
		} else {
			bsd_args.addr = (caddr_t)PTRIN(addr) -
			    (STACK_SIZE - GUARD_SIZE - len);
			bsd_args.len = STACK_SIZE - GUARD_SIZE;
		}
	} else {
		bsd_args.addr = (caddr_t)PTRIN(addr);
		bsd_args.len  = len;
	}
	bsd_args.pos = pos;

#ifdef DEBUG
	if (ldebug(mmap))
		printf("-> %s(%p, %d, %d, 0x%08x, %d, 0x%x)\n",
		    __func__,
		    (void *)bsd_args.addr, (int)bsd_args.len, bsd_args.prot,
		    bsd_args.flags, bsd_args.fd, (int)bsd_args.pos);
#endif
	error = sys_mmap(td, &bsd_args);
#ifdef DEBUG
	if (ldebug(mmap))
		printf("-> %s() return: 0x%x (0x%08x)\n",
			__func__, error, (u_int)td->td_retval[0]);
#endif
	return (error);
}

int
linux_mprotect(struct thread *td, struct linux_mprotect_args *uap)
{
	struct mprotect_args bsd_args;

	bsd_args.addr = uap->addr;
	bsd_args.len = uap->len;
	bsd_args.prot = uap->prot;
	if (bsd_args.prot & (PROT_READ | PROT_WRITE | PROT_EXEC))
		bsd_args.prot |= PROT_READ | PROT_EXEC;
	return (sys_mprotect(td, &bsd_args));
}

int
linux_iopl(struct thread *td, struct linux_iopl_args *args)
{
	int error;

	if (args->level < 0 || args->level > 3)
		return (EINVAL);
	if ((error = priv_check(td, PRIV_IO)) != 0)
		return (error);
	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);
	td->td_frame->tf_rflags = (td->td_frame->tf_rflags & ~PSL_IOPL) |
	    (args->level * (PSL_IOPL / 3));

	return (0);
}

int
linux_pipe(struct thread *td, struct linux_pipe_args *args)
{
	int error;
	int fildes[2];

#ifdef DEBUG
	if (ldebug(pipe))
		printf(ARGS(pipe, "*"));
#endif

	error = kern_pipe(td, fildes);
	if (error)
		return (error);

	/* XXX: Close descriptors on error. */
	return (copyout(fildes, args->pipefds, sizeof fildes));
}

int
linux_sigaction(struct thread *td, struct linux_sigaction_args *args)
{
	l_osigaction_t osa;
	l_sigaction_t act, oact;
	int error;

#ifdef DEBUG
	if (ldebug(sigaction))
		printf(ARGS(sigaction, "%d, %p, %p"),
		    args->sig, (void *)args->nsa, (void *)args->osa);
#endif

	if (args->nsa != NULL) {
		error = copyin(args->nsa, &osa, sizeof(l_osigaction_t));
		if (error)
			return (error);
		act.lsa_handler = osa.lsa_handler;
		act.lsa_flags = osa.lsa_flags;
		act.lsa_restorer = osa.lsa_restorer;
		LINUX_SIGEMPTYSET(act.lsa_mask);
		act.lsa_mask.__bits[0] = osa.lsa_mask;
	}

	error = linux_do_sigaction(td, args->sig, args->nsa ? &act : NULL,
	    args->osa ? &oact : NULL);

	if (args->osa != NULL && !error) {
		osa.lsa_handler = oact.lsa_handler;
		osa.lsa_flags = oact.lsa_flags;
		osa.lsa_restorer = oact.lsa_restorer;
		osa.lsa_mask = oact.lsa_mask.__bits[0];
		error = copyout(&osa, args->osa, sizeof(l_osigaction_t));
	}

	return (error);
}

/*
 * Linux has two extra args, restart and oldmask.  We don't use these,
 * but it seems that "restart" is actually a context pointer that
 * enables the signal to happen with a different register set.
 */
int
linux_sigsuspend(struct thread *td, struct linux_sigsuspend_args *args)
{
	sigset_t sigmask;
	l_sigset_t mask;

#ifdef DEBUG
	if (ldebug(sigsuspend))
		printf(ARGS(sigsuspend, "%08lx"), (unsigned long)args->mask);
#endif

	LINUX_SIGEMPTYSET(mask);
	mask.__bits[0] = args->mask;
	linux_to_bsd_sigset(&mask, &sigmask);
	return (kern_sigsuspend(td, sigmask));
}

int
linux_rt_sigsuspend(struct thread *td, struct linux_rt_sigsuspend_args *uap)
{
	l_sigset_t lmask;
	sigset_t sigmask;
	int error;

#ifdef DEBUG
	if (ldebug(rt_sigsuspend))
		printf(ARGS(rt_sigsuspend, "%p, %d"),
		    (void *)uap->newset, uap->sigsetsize);
#endif

	if (uap->sigsetsize != sizeof(l_sigset_t))
		return (EINVAL);

	error = copyin(uap->newset, &lmask, sizeof(l_sigset_t));
	if (error)
		return (error);

	linux_to_bsd_sigset(&lmask, &sigmask);
	return (kern_sigsuspend(td, sigmask));
}

int
linux_pause(struct thread *td, struct linux_pause_args *args)
{
	struct proc *p = td->td_proc;
	sigset_t sigmask;

#ifdef DEBUG
	if (ldebug(pause))
		printf(ARGS(pause, ""));
#endif

	PROC_LOCK(p);
	sigmask = td->td_sigmask;
	PROC_UNLOCK(p);
	return (kern_sigsuspend(td, sigmask));
}

int
linux_sigaltstack(struct thread *td, struct linux_sigaltstack_args *uap)
{
	stack_t ss, oss;
	l_stack_t lss;
	int error;

#ifdef DEBUG
	if (ldebug(sigaltstack))
		printf(ARGS(sigaltstack, "%p, %p"), uap->uss, uap->uoss);
#endif

	if (uap->uss != NULL) {
		error = copyin(uap->uss, &lss, sizeof(l_stack_t));
		if (error)
			return (error);

		ss.ss_sp = PTRIN(lss.ss_sp);
		ss.ss_size = lss.ss_size;
		ss.ss_flags = linux_to_bsd_sigaltstack(lss.ss_flags);
	}
	error = kern_sigaltstack(td, (uap->uss != NULL) ? &ss : NULL,
	    (uap->uoss != NULL) ? &oss : NULL);
	if (!error && uap->uoss != NULL) {
		lss.ss_sp = PTROUT(oss.ss_sp);
		lss.ss_size = oss.ss_size;
		lss.ss_flags = bsd_to_linux_sigaltstack(oss.ss_flags);
		error = copyout(&lss, uap->uoss, sizeof(l_stack_t));
	}

	return (error);
}

int
linux_ftruncate64(struct thread *td, struct linux_ftruncate64_args *args)
{
	struct ftruncate_args sa;

#ifdef DEBUG
	if (ldebug(ftruncate64))
		printf(ARGS(ftruncate64, "%u, %jd"), args->fd,
		    (intmax_t)args->length);
#endif

	sa.fd = args->fd;
	sa.length = args->length;
	return sys_ftruncate(td, &sa);
}

int
linux_gettimeofday(struct thread *td, struct linux_gettimeofday_args *uap)
{
	struct timeval atv;
	l_timeval atv32;
	struct timezone rtz;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		atv32.tv_sec = atv.tv_sec;
		atv32.tv_usec = atv.tv_usec;
		error = copyout(&atv32, uap->tp, sizeof(atv32));
	}
	if (error == 0 && uap->tzp != NULL) {
		rtz.tz_minuteswest = tz_minuteswest;
		rtz.tz_dsttime = tz_dsttime;
		error = copyout(&rtz, uap->tzp, sizeof(rtz));
	}
	return (error);
}

int
linux_settimeofday(struct thread *td, struct linux_settimeofday_args *uap)
{
	l_timeval atv32;
	struct timeval atv, *tvp;
	struct timezone atz, *tzp;
	int error;

	if (uap->tp) {
		error = copyin(uap->tp, &atv32, sizeof(atv32));
		if (error)
			return (error);
		atv.tv_sec = atv32.tv_sec;
		atv.tv_usec = atv32.tv_usec;
		tvp = &atv;
	} else
		tvp = NULL;
	if (uap->tzp) {
		error = copyin(uap->tzp, &atz, sizeof(atz));
		if (error)
			return (error);
		tzp = &atz;
	} else
		tzp = NULL;
	return (kern_settimeofday(td, tvp, tzp));
}

int
linux_getrusage(struct thread *td, struct linux_getrusage_args *uap)
{
	struct l_rusage s32;
	struct rusage s;
	int error;

	error = kern_getrusage(td, uap->who, &s);
	if (error != 0)
		return (error);
	if (uap->rusage != NULL) {
		bsd_to_linux_rusage(&s, &s32);
		error = copyout(&s32, uap->rusage, sizeof(s32));
	}
	return (error);
}

int
linux_sched_rr_get_interval(struct thread *td,
    struct linux_sched_rr_get_interval_args *uap)
{
	struct timespec ts;
	struct l_timespec ts32;
	int error;

	error = kern_sched_rr_get_interval(td, uap->pid, &ts);
	if (error != 0)
		return (error);
	ts32.tv_sec = ts.tv_sec;
	ts32.tv_nsec = ts.tv_nsec;
	return (copyout(&ts32, uap->interval, sizeof(ts32)));
}

int
linux_set_thread_area(struct thread *td,
    struct linux_set_thread_area_args *args)
{
	struct l_user_desc info;
	struct user_segment_descriptor sd;
	struct pcb *pcb;
	int a[2];
	int error;

	error = copyin(args->desc, &info, sizeof(struct l_user_desc));
	if (error)
		return (error);

#ifdef DEBUG
	if (ldebug(set_thread_area))
		printf(ARGS(set_thread_area, "%i, %x, %x, %i, %i, %i, "
		    "%i, %i, %i"), info.entry_number, info.base_addr,
		    info.limit, info.seg_32bit, info.contents,
		    info.read_exec_only, info.limit_in_pages,
		    info.seg_not_present, info.useable);
#endif

	/*
	 * Semantics of Linux version: every thread in the system has array
	 * of three TLS descriptors. 1st is GLIBC TLS, 2nd is WINE, 3rd unknown.
	 * This syscall loads one of the selected TLS decriptors with a value
	 * and also loads GDT descriptors 6, 7 and 8 with the content of
	 * the per-thread descriptors.
	 *
	 * Semantics of FreeBSD version: I think we can ignore that Linux has
	 * three per-thread descriptors and use just the first one.
	 * The tls_array[] is used only in [gs]et_thread_area() syscalls and
	 * for loading the GDT descriptors. We use just one GDT descriptor
	 * for TLS, so we will load just one.
	 *
	 * XXX: This doesn't work when a user space process tries to use more
	 * than one TLS segment. Comment in the Linux source says wine might
	 * do this.
	 */

	/*
	 * GLIBC reads current %gs and call set_thread_area() with it.
	 * We should let GUDATA_SEL and GUGS32_SEL proceed as well because
	 * we use these segments.
	 */
	switch (info.entry_number) {
	case GUGS32_SEL:
	case GUDATA_SEL:
	case 6:
	case -1:
		info.entry_number = GUGS32_SEL;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * We have to copy out the GDT entry we use.
	 *
	 * XXX: What if a user space program does not check the return value
	 * and tries to use 6, 7 or 8?
	 */
	error = copyout(&info, args->desc, sizeof(struct l_user_desc));
	if (error)
		return (error);

	if (LINUX_LDT_empty(&info)) {
		a[0] = 0;
		a[1] = 0;
	} else {
		a[0] = LINUX_LDT_entry_a(&info);
		a[1] = LINUX_LDT_entry_b(&info);
	}

	memcpy(&sd, &a, sizeof(a));
#ifdef DEBUG
	if (ldebug(set_thread_area))
		printf("Segment created in set_thread_area: "
		    "lobase: %x, hibase: %x, lolimit: %x, hilimit: %x, "
		    "type: %i, dpl: %i, p: %i, xx: %i, long: %i, "
		    "def32: %i, gran: %i\n",
		    sd.sd_lobase,
		    sd.sd_hibase,
		    sd.sd_lolimit,
		    sd.sd_hilimit,
		    sd.sd_type,
		    sd.sd_dpl,
		    sd.sd_p,
		    sd.sd_xx,
		    sd.sd_long,
		    sd.sd_def32,
		    sd.sd_gran);
#endif

	pcb = td->td_pcb;
	pcb->pcb_gsbase = (register_t)info.base_addr;
	set_pcb_flags(pcb, PCB_32BIT | PCB_GS32BIT);
	update_gdt_gsbase(td, info.base_addr);

	return (0);
}

int
linux_wait4(struct thread *td, struct linux_wait4_args *args)
{
	int error, options;
	struct rusage ru, *rup;
	struct l_rusage lru;

#ifdef DEBUG
	if (ldebug(wait4))
		printf(ARGS(wait4, "%d, %p, %d, %p"),
		    args->pid, (void *)args->status, args->options,
		    (void *)args->rusage);
#endif

	options = (args->options & (WNOHANG | WUNTRACED));
	/* WLINUXCLONE should be equal to __WCLONE, but we make sure */
	if (args->options & __WCLONE)
		options |= WLINUXCLONE;

	if (args->rusage != NULL)
		rup = &ru;
	else
		rup = NULL;
	error = linux_common_wait(td, args->pid, args->status, options, rup);
	if (error)
		return (error);
	if (args->rusage != NULL) {
		bsd_to_linux_rusage(rup, &lru);
		error = copyout(&lru, args->rusage, sizeof(lru));
	}

	return (error);
}
