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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <alpha/linux/linux.h>
#include <alpha/linux/linux_proto.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

struct linux_select_argv {
	int nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *exceptfds;
	struct timeval *timeout;
};

int
linux_execve(struct thread *td, struct linux_execve_args *args)
{
	struct execve_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(execve))
		printf(ARGS(execve, "%s"), args->path);
#endif
	bsd.fname = args->path;
	bsd.argv = args->argp;
	bsd.envv = args->envp;
	return (execve(td, &bsd));
}

/*
 * MPSAFE
 */
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

/*
 * MPSAFE
 */
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

#define	CLONE_VM	0x100
#define	CLONE_FS	0x200
#define	CLONE_FILES	0x400
#define	CLONE_SIGHAND	0x800
#define	CLONE_PID	0x1000

int
linux_clone(struct thread *td, struct linux_clone_args *args)
{
	int error, ff = RFPROC | RFSTOPPED;
	struct proc *p2;
	int exit_signal;
	vm_offset_t start;

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

/*	if (exit_signal <= LINUX_SIGTBLSZ)
		exit_signal = linux_to_bsd_signal[_SIG_IDX(exit_signal)];
*/
	/* RFTHREAD probably not necessary here, but it shouldn't hurt */
	ff |= RFTHREAD;

	if (args->flags & CLONE_VM)
		ff |= RFMEM;
	if (args->flags & CLONE_SIGHAND)
		ff |= RFSIGSHARE;
	if (!(args->flags & CLONE_FILES))
		ff |= RFFDG;

	error = 0;
	start = 0;

	if ((error = fork1(td, ff, &p2)) != 0)
		return (error);

	PROC_LOCK(p2);
	p2->p_sigparent = exit_signal;
	PROC_UNLOCK(p2);
	p2->p_thread.td_pcb->pcb_hw.apcb_usp = (unsigned long)args->stack;

#ifdef DEBUG
	if (ldebug(clone))
		printf(LMSG("clone: successful rfork to %ld, stack %p sig = %d"),
		    (long)p2->p_pid, args->stack, exit_signal);
#endif

	/*
	 * Make this runnable after we are finished with it.
	 */
	mtx_lock_spin(&sched_lock);
	p2->p_stat = SRUN;
	setrunqueue(&p2->p_thread);
	mtx_unlock_spin(&sched_lock);

	td->td_retval[0] = p2->p_pid;
	td->td_retval[1] = 0;
	return (0);
}

#define	STACK_SIZE  (2 * 1024 * 1024)
#define	GUARD_SIZE  (4 * PAGE_SIZE)

int
linux_mmap(struct thread *td, struct linux_mmap_args *linux_args)
{
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

#ifdef DEBUG
	if (ldebug(mmap))
		printf(ARGS(mmap, "%p, 0x%lx, 0x%x, 0x%x, 0x%x, 0x%lx"),
		    (void *)linux_args->addr, linux_args->len,
		    linux_args->prot, linux_args->flags, linux_args->fd,
		    linux_args->pos);
#endif
	bsd_args.prot = linux_args->prot | PROT_READ;	/* always required */

	bsd_args.flags = 0;
	if (linux_args->flags & LINUX_MAP_SHARED)
		bsd_args.flags |= MAP_SHARED;
	if (linux_args->flags & LINUX_MAP_PRIVATE)
		bsd_args.flags |= MAP_PRIVATE;
	if (linux_args->flags & LINUX_MAP_FIXED){
		bsd_args.flags |= MAP_FIXED;
		bsd_args.pos = trunc_page(linux_args->pos);
	} else {
		bsd_args.pos = linux_args->pos;
	}
	if (linux_args->flags & LINUX_MAP_ANON)
		bsd_args.flags |= MAP_ANON;
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
		 * and autgrows the region down, up to the limit
		 * in addr.
		 *
		 * If we don't use the MAP_STACK option, the effect
		 * of this code is to allocate a stack region of a
		 * fixed size of (STACK_SIZE - GUARD_SIZE).
		 */

		/* This gives us TOS */
		bsd_args.addr = (caddr_t)(linux_args->addr + linux_args->len);

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
		bsd_args.addr = (caddr_t)round_page(bsd_args.addr); /* XXXX */
	} else {
		bsd_args.addr = (caddr_t)linux_args->addr;
		bsd_args.len  = linux_args->len;
	}

	bsd_args.fd = linux_args->fd;
	if(linux_args->fd == 0)
		bsd_args.fd = -1;

	bsd_args.pad = 0;
#ifdef DEBUG
	if (ldebug(mmap))
		printf(ARGS(mmap, "%p, 0x%lx, 0x%x, 0x%x, 0x%x, 0x%lx)",
		    (void *)bsd_args.addr,
		    bsd_args.len,
		    bsd_args.prot,
		    bsd_args.flags,
		    bsd_args.fd,
		    bsd_args.pos);
#endif
	if (bsd_args.addr == 0)
		bsd_args.addr = (caddr_t)0x40000000UL;
	error = mmap(td, &bsd_args);
#ifdef DEBUG
	if (ldebug(mmap))
		printf(LMSG("mmap returns %d, 0x%lx", error, td->td_retval[0]);
#endif
	return (error);
}

int
linux_rt_sigsuspend(td, uap)
	struct thread *td;
	struct linux_rt_sigsuspend_args *uap;
{
	int error;
	l_sigset_t lmask;
	sigset_t *bmask;
	struct sigsuspend_args bsd;
	caddr_t sg;

	sg = stackgap_init();

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

	bmask = stackgap_alloc(&sg, sizeof(sigset_t));
	linux_to_bsd_sigset(&lmask, bmask);
	bsd.sigmask = bmask;
	return (sigsuspend(td, &bsd));
}

int
linux_mprotect(td, uap)
	struct thread *td;
	struct linux_mprotect_args *uap;
{

#ifdef DEBUG
	if (ldebug(mprotect))
		printf(ARGS(mprotect, "%p, 0x%lx, 0x%x)",
		    (void *)uap->addr, uap->len, uap->prot);
#endif
	return (mprotect(td, (void *)uap));
}

int
linux_munmap(td, uap)
	struct thread *td;
	struct linux_munmap_args *uap;
{

#ifdef DEBUG
	if (ldebug(munmap))
		printf(ARGS(munmap, "%p, 0x%lx",
		    (void *)uap->addr, uap->len);
#endif
	return (munmap(td, (void *)uap));
}

static unsigned int linux_to_bsd_resource[LINUX_RLIM_NLIMITS] = {
	RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK,
	RLIMIT_CORE, RLIMIT_RSS, RLIMIT_NOFILE, -1,
	RLIMIT_NPROC, RLIMIT_MEMLOCK
};

int
linux_setrlimit(td, uap)
	struct thread *td;
	struct linux_setrlimit_args *uap;
{
	struct rlimit rlim;
	u_int which;
	int error;

#ifdef DEBUG
	if (ldebug(setrlimit))
		printf(ARGS(setrlimit, "%d, %p"),
		    uap->resource, (void *)uap->rlim);
#endif
	if (uap->resource >= LINUX_RLIM_NLIMITS)
		return EINVAL;

	which = linux_to_bsd_resource[uap->resource];

	if (which == -1)
		return EINVAL;

	if ((error =
	   copyin((caddr_t)uap->rlim, (caddr_t)&rlim, sizeof (struct rlimit))))
		return (error);
	return dosetrlimit(td,  which, &rlim);
}

int
linux_getrlimit(td, uap)
	struct thread *td;
	struct linux_getrlimit_args *uap;
{
	u_int which;

#ifdef DEBUG
	if (ldebug(getrlimit))
		printf(ARGS(getrlimit, "%d, %p"),
		    uap->resource, (void *)uap->rlim);
#endif
	if (uap->resource >= LINUX_RLIM_NLIMITS)
		return EINVAL;

	which = linux_to_bsd_resource[uap->resource];

	if (which == -1)
		return EINVAL;

	return (copyout((caddr_t)&td->td_proc->p_rlimit[which],
	    (caddr_t)uap->rlim, sizeof (struct rlimit)));
}
