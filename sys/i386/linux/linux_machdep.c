/*-
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
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>

#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/sysarch.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <compat/linux/linux_ipc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

struct l_descriptor {
	l_uint		entry_number;
	l_ulong		base_addr;
	l_uint		limit;
	l_uint		seg_32bit:1;
	l_uint		contents:2;
	l_uint		read_exec_only:1;
	l_uint		limit_in_pages:1;
	l_uint		seg_not_present:1;
	l_uint		useable:1;
};

struct l_old_select_argv {
	l_int		nfds;
	l_fd_set	*readfds;
	l_fd_set	*writefds;
	l_fd_set	*exceptfds;
	struct l_timeval	*timeout;
};

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

int
linux_execve(struct thread *td, struct linux_execve_args *args)
{
	int error;
	char *newpath;
	struct image_args eargs;

	LCONVPATHEXIST(td, args->path, &newpath);

#ifdef DEBUG
	if (ldebug(execve))
		printf(ARGS(execve, "%s"), newpath);
#endif

	error = exec_copyin_args(&eargs, newpath, UIO_SYSSPACE,
	    args->argp, args->envp);
	free(newpath, M_TEMP);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL);
	exec_free_args(&eargs);
	return (error);
}

struct l_ipc_kludge {
	struct l_msgbuf *msgp;
	l_long msgtyp;
};

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

			if (args->ptr == NULL)
				return (EINVAL);
			error = copyin(args->ptr, &tmp, sizeof(tmp));
			if (error)
				return (error);
			a.msgp = tmp.msgp;
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
		a.raddr = (l_ulong *)args->arg3;
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
	newsel.readfds = linux_args.readfds;
	newsel.writefds = linux_args.writefds;
	newsel.exceptfds = linux_args.exceptfds;
	newsel.timeout = linux_args.timeout;
	return (linux_select(td, &newsel));
}

int
linux_fork(struct thread *td, struct linux_fork_args *args)
{
	int error;

#ifdef DEBUG
	if (ldebug(fork))
		printf(ARGS(fork, ""));
#endif

	if ((error = fork(td, (struct fork_args *)args)) != 0)
		return (error);

	if (td->td_retval[1] == 1)
		td->td_retval[0] = 0;
	return (0);
}

int
linux_vfork(struct thread *td, struct linux_vfork_args *args)
{
	int error;

#ifdef DEBUG
	if (ldebug(vfork))
		printf(ARGS(vfork, ""));
#endif

	if ((error = vfork(td, (struct vfork_args *)args)) != 0)
		return (error);
	/* Are we the child? */
	if (td->td_retval[1] == 1)
		td->td_retval[0] = 0;
	return (0);
}

#define CLONE_VM	0x100
#define CLONE_FS	0x200
#define CLONE_FILES	0x400
#define CLONE_SIGHAND	0x800
#define CLONE_PID	0x1000
#define CLONE_THREAD	0x10000

#define THREADING_FLAGS	(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)

int
linux_clone(struct thread *td, struct linux_clone_args *args)
{
	int error, ff = RFPROC | RFSTOPPED;
	struct proc *p2;
	struct thread *td2;
	int exit_signal;

#ifdef DEBUG
	if (ldebug(clone)) {
		printf(ARGS(clone, "flags %x, stack %x"),
		    (unsigned int)args->flags, (unsigned int)args->stack);
		if (args->flags & CLONE_PID)
			printf(LMSG("CLONE_PID not yet supported"));
	}
#endif

	if (!args->stack)
		return (EINVAL);

	exit_signal = args->flags & 0x000000ff;
	if (exit_signal >= LINUX_NSIG)
		return (EINVAL);

	if (exit_signal <= LINUX_SIGTBLSZ)
		exit_signal = linux_to_bsd_signal[_SIG_IDX(exit_signal)];

	if (args->flags & CLONE_VM)
		ff |= RFMEM;
	if (args->flags & CLONE_SIGHAND)
		ff |= RFSIGSHARE;
	if (!(args->flags & CLONE_FILES))
		ff |= RFFDG;

	/*
	 * Attempt to detect when linux_clone(2) is used for creating
	 * kernel threads. Unfortunately despite the existence of the
	 * CLONE_THREAD flag, version of linuxthreads package used in
	 * most popular distros as of beginning of 2005 doesn't make
	 * any use of it. Therefore, this detection relies on
	 * empirical observation that linuxthreads sets certain
	 * combination of flags, so that we can make more or less
	 * precise detection and notify the FreeBSD kernel that several
	 * processes are in fact part of the same threading group, so
	 * that special treatment is necessary for signal delivery
	 * between those processes and fd locking.
	 */
	if ((args->flags & 0xffffff00) == THREADING_FLAGS)
		ff |= RFTHREAD;

	error = fork1(td, ff, 0, &p2);
	if (error)
		return (error);
	

	PROC_LOCK(p2);
	p2->p_sigparent = exit_signal;
	PROC_UNLOCK(p2);
	td2 = FIRST_THREAD_IN_PROC(p2);
	td2->td_frame->tf_esp = (unsigned int)args->stack;

#ifdef DEBUG
	if (ldebug(clone))
		printf(LMSG("clone: successful rfork to %ld, stack %p sig = %d"),
		    (long)p2->p_pid, args->stack, exit_signal);
#endif

	/*
	 * Make this runnable after we are finished with it.
	 */
	mtx_lock_spin(&sched_lock);
	TD_SET_CAN_RUN(td2);
	setrunqueue(td2, SRQ_BORING);
	mtx_unlock_spin(&sched_lock);

	td->td_retval[0] = p2->p_pid;
	td->td_retval[1] = 0;
	return (0);
}

/* XXX move */
struct l_mmap_argv {
	l_caddr_t	addr;
	l_int		len;
	l_int		prot;
	l_int		flags;
	l_int		fd;
	l_int		pos;
};

#define STACK_SIZE  (2 * 1024 * 1024)
#define GUARD_SIZE  (4 * PAGE_SIZE)

static int linux_mmap_common(struct thread *, struct l_mmap_argv *);

int
linux_mmap2(struct thread *td, struct linux_mmap2_args *args)
{
	struct l_mmap_argv linux_args;

#ifdef DEBUG
	if (ldebug(mmap2))
		printf(ARGS(mmap2, "%p, %d, %d, 0x%08x, %d, %d"),
		    (void *)args->addr, args->len, args->prot,
		    args->flags, args->fd, args->pgoff);
#endif

	linux_args.addr = (l_caddr_t)args->addr;
	linux_args.len = args->len;
	linux_args.prot = args->prot;
	linux_args.flags = args->flags;
	linux_args.fd = args->fd;
	linux_args.pos = args->pgoff * PAGE_SIZE;

	return (linux_mmap_common(td, &linux_args));
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
		printf(ARGS(mmap, "%p, %d, %d, 0x%08x, %d, %d"),
		    (void *)linux_args.addr, linux_args.len, linux_args.prot,
		    linux_args.flags, linux_args.fd, linux_args.pos);
#endif

	return (linux_mmap_common(td, &linux_args));
}

static int
linux_mmap_common(struct thread *td, struct l_mmap_argv *linux_args)
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
	if (! ((linux_args->flags & LINUX_MAP_SHARED) ^
	    (linux_args->flags & LINUX_MAP_PRIVATE)))
		return (EINVAL);

	if (linux_args->flags & LINUX_MAP_SHARED)
		bsd_args.flags |= MAP_SHARED;
	if (linux_args->flags & LINUX_MAP_PRIVATE)
		bsd_args.flags |= MAP_PRIVATE;
	if (linux_args->flags & LINUX_MAP_FIXED)
		bsd_args.flags |= MAP_FIXED;
	if (linux_args->flags & LINUX_MAP_ANON)
		bsd_args.flags |= MAP_ANON;
	else
		bsd_args.flags |= MAP_NOSYNC;
	if (linux_args->flags & LINUX_MAP_GROWSDOWN) {
		bsd_args.flags |= MAP_STACK;

		/* The linux MAP_GROWSDOWN option does not limit auto
		 * growth of the region.  Linux mmap with this option
		 * takes as addr the inital BOS, and as len, the initial
		 * region size.  It can then grow down from addr without
		 * limit.  However, linux threads has an implicit internal
		 * limit to stack size of STACK_SIZE.  Its just not
		 * enforced explicitly in linux.  But, here we impose
		 * a limit of (STACK_SIZE - GUARD_SIZE) on the stack
		 * region, since we can do this with our mmap.
		 *
		 * Our mmap with MAP_STACK takes addr as the maximum
		 * downsize limit on BOS, and as len the max size of
		 * the region.  It them maps the top SGROWSIZ bytes,
		 * and auto grows the region down, up to the limit
		 * in addr.
		 *
		 * If we don't use the MAP_STACK option, the effect
		 * of this code is to allocate a stack region of a
		 * fixed size of (STACK_SIZE - GUARD_SIZE).
		 */

		/* This gives us TOS */
		bsd_args.addr = linux_args->addr + linux_args->len;

		if (bsd_args.addr > p->p_vmspace->vm_maxsaddr) {
			/* Some linux apps will attempt to mmap
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
			p->p_vmspace->vm_maxsaddr = (char *)USRSTACK -
			    lim_cur(p, RLIMIT_STACK);
			PROC_UNLOCK(p);
		}

		/* This gives us our maximum stack size */
		if (linux_args->len > STACK_SIZE - GUARD_SIZE)
			bsd_args.len = linux_args->len;
		else
			bsd_args.len  = STACK_SIZE - GUARD_SIZE;

		/* This gives us a new BOS.  If we're using VM_STACK, then
		 * mmap will just map the top SGROWSIZ bytes, and let
		 * the stack grow down to the limit at BOS.  If we're
		 * not using VM_STACK we map the full stack, since we
		 * don't have a way to autogrow it.
		 */
		bsd_args.addr -= bsd_args.len;
	} else {
		bsd_args.addr = linux_args->addr;
		bsd_args.len  = linux_args->len;
	}

	bsd_args.prot = linux_args->prot;
	if (linux_args->flags & LINUX_MAP_ANON)
		bsd_args.fd = -1;
	else {
		/*
		 * Linux follows Solaris mmap(2) description:
		 * The file descriptor fildes is opened with
		 * read permission, regardless of the
		 * protection options specified.
		 * If PROT_WRITE is specified, the application
		 * must have opened the file descriptor
		 * fildes with write permission unless
		 * MAP_PRIVATE is specified in the flag
		 * argument as described below.
		 */

		if ((error = fget(td, linux_args->fd, &fp)) != 0)
			return (error);
		if (fp->f_type != DTYPE_VNODE) {
			fdrop(fp, td);
			return (EINVAL);
		}

		/* Linux mmap() just fails for O_WRONLY files */
		if (! (fp->f_flag & FREAD)) {
			fdrop(fp, td);
			return (EACCES);
		}

		bsd_args.fd = linux_args->fd;
		fdrop(fp, td);
	}
	bsd_args.pos = linux_args->pos;
	bsd_args.pad = 0;

#ifdef DEBUG
	if (ldebug(mmap))
		printf("-> %s(%p, %d, %d, 0x%08x, %d, 0x%x)\n",
		    __func__,
		    (void *)bsd_args.addr, bsd_args.len, bsd_args.prot,
		    bsd_args.flags, bsd_args.fd, (int)bsd_args.pos);
#endif
	error = mmap(td, &bsd_args);
#ifdef DEBUG
	if (ldebug(mmap))
		printf("-> %s() return: 0x%x (0x%08x)\n",
			__func__, error, (u_int)td->td_retval[0]);
#endif
	return (error);
}

int
linux_pipe(struct thread *td, struct linux_pipe_args *args)
{
	int error;
	int reg_edx;

#ifdef DEBUG
	if (ldebug(pipe))
		printf(ARGS(pipe, "*"));
#endif

	reg_edx = td->td_retval[1];
	error = pipe(td, 0);
	if (error) {
		td->td_retval[1] = reg_edx;
		return (error);
	}

	error = copyout(td->td_retval, args->pipefds, 2*sizeof(int));
	if (error) {
		td->td_retval[1] = reg_edx;
		return (error);
	}

	td->td_retval[1] = reg_edx;
	td->td_retval[0] = 0;
	return (0);
}

int
linux_ioperm(struct thread *td, struct linux_ioperm_args *args)
{
	int error;
	struct i386_ioperm_args iia;

	iia.start = args->start;
	iia.length = args->length;
	iia.enable = args->enable;
	mtx_lock(&Giant);
	error = i386_set_ioperm(td, &iia);
	mtx_unlock(&Giant);
	return (error);
}

int
linux_iopl(struct thread *td, struct linux_iopl_args *args)
{
	int error;

	if (args->level < 0 || args->level > 3)
		return (EINVAL);
	if ((error = suser(td)) != 0)
		return (error);
	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);
	td->td_frame->tf_eflags = (td->td_frame->tf_eflags & ~PSL_IOPL) |
	    (args->level * (PSL_IOPL / 3));
	return (0);
}

int
linux_modify_ldt(struct thread *td, struct linux_modify_ldt_args *uap)
{
	int error;
	struct i386_ldt_args ldt;
	struct l_descriptor ld;
	union descriptor desc;

	if (uap->ptr == NULL)
		return (EINVAL);

	switch (uap->func) {
	case 0x00: /* read_ldt */
		ldt.start = 0;
		ldt.descs = uap->ptr;
		ldt.num = uap->bytecount / sizeof(union descriptor);
		mtx_lock(&Giant);
		error = i386_get_ldt(td, &ldt);
		td->td_retval[0] *= sizeof(union descriptor);
		mtx_unlock(&Giant);
		break;
	case 0x01: /* write_ldt */
	case 0x11: /* write_ldt */
		if (uap->bytecount != sizeof(ld))
			return (EINVAL);

		error = copyin(uap->ptr, &ld, sizeof(ld));
		if (error)
			return (error);

		ldt.start = ld.entry_number;
		ldt.descs = &desc;
		ldt.num = 1;
		desc.sd.sd_lolimit = (ld.limit & 0x0000ffff);
		desc.sd.sd_hilimit = (ld.limit & 0x000f0000) >> 16;
		desc.sd.sd_lobase = (ld.base_addr & 0x00ffffff);
		desc.sd.sd_hibase = (ld.base_addr & 0xff000000) >> 24;
		desc.sd.sd_type = SDT_MEMRO | ((ld.read_exec_only ^ 1) << 1) |
			(ld.contents << 2);
		desc.sd.sd_dpl = 3;
		desc.sd.sd_p = (ld.seg_not_present ^ 1);
		desc.sd.sd_xx = 0;
		desc.sd.sd_def32 = ld.seg_32bit;
		desc.sd.sd_gran = ld.limit_in_pages;
		mtx_lock(&Giant);
		error = i386_set_ldt(td, &ldt, &desc);
		mtx_unlock(&Giant);
		break;
	default:
		error = EINVAL;
		break;
	}

	if (error == EOPNOTSUPP) {
		printf("linux: modify_ldt needs kernel option USER_LDT\n");
		error = ENOSYS;
	}

	return (error);
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
 * Linux has two extra args, restart and oldmask.  We dont use these,
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

		ss.ss_sp = lss.ss_sp;
		ss.ss_size = lss.ss_size;
		ss.ss_flags = linux_to_bsd_sigaltstack(lss.ss_flags);
	}
	error = kern_sigaltstack(td, (uap->uss != NULL) ? &ss : NULL,
	    (uap->uoss != NULL) ? &oss : NULL);
	if (!error && uap->uoss != NULL) {
		lss.ss_sp = oss.ss_sp;
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
	sa.pad = 0;
	sa.length = args->length;
	return ftruncate(td, &sa);
}

int
linux_set_thread_area(struct thread *td, struct linux_set_thread_area_args *args)
{
	/*
	 * Return an error code instead of raising a SIGSYS so that
	 * the caller will fall back to simpler LDT methods.
	 */
	return (ENOSYS);
}

int
linux_gettid(struct thread *td, struct linux_gettid_args *args)
{

	td->td_retval[0] = td->td_proc->p_pid;
	return (0);
}

int
linux_tkill(struct thread *td, struct linux_tkill_args *args)
{

	return (linux_kill(td, (struct linux_kill_args *) args));
}

