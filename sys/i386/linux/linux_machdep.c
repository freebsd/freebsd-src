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
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>

#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/sysarch.h>

#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <compat/linux/linux_ipc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

struct linux_descriptor {
	unsigned int  entry_number;
	unsigned long base_addr;
	unsigned int  limit;
	unsigned int  seg_32bit:1;
	unsigned int  contents:2;
	unsigned int  read_exec_only:1;
	unsigned int  limit_in_pages:1;
	unsigned int  seg_not_present:1;
	unsigned int  useable:1;
};

struct linux_select_argv {
	int nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *exceptfds;
	struct timeval *timeout;
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
linux_execve(struct proc *p, struct linux_execve_args *args)
{
	struct execve_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
        printf("Linux-emul(%d): execve(%s)\n", 
	    p->p_pid, args->path);
#endif

	bsd.fname = args->path;
	bsd.argv = args->argp;
	bsd.envv = args->envp;
	return (execve(p, &bsd));
}

int
linux_ipc(struct proc *p, struct linux_ipc_args *args)
{
	switch (args->what) {
	case LINUX_SEMOP:
		return (linux_semop(p, args));
	case LINUX_SEMGET:
		return (linux_semget(p, args));
	case LINUX_SEMCTL:
		return (linux_semctl(p, args));
	case LINUX_MSGSND:
		return (linux_msgsnd(p, args));
	case LINUX_MSGRCV:
		return (linux_msgrcv(p, args));
	case LINUX_MSGGET:
		return (linux_msgget(p, args));
	case LINUX_MSGCTL:
		return (linux_msgctl(p, args));
	case LINUX_SHMAT:
		return (linux_shmat(p, args));
	case LINUX_SHMDT:
		return (linux_shmdt(p, args));
	case LINUX_SHMGET:
		return (linux_shmget(p, args));
	case LINUX_SHMCTL:
		return (linux_shmctl(p, args));
	}

	uprintf("LINUX: 'ipc' typ=%d not implemented\n", args->what);
	return (ENOSYS);
}

int
linux_select(struct proc *p, struct linux_select_args *args)
{
	struct linux_select_argv linux_args;
	struct linux_newselect_args newsel;
	int error;

#ifdef SELECT_DEBUG
	printf("Linux-emul(%ld): select(%x)\n", (long)p->p_pid, args->ptr);
#endif

	error = copyin(args->ptr, &linux_args, sizeof(linux_args));
	if (error)
		return (error);

	newsel.nfds = linux_args.nfds;
	newsel.readfds = linux_args.readfds;
	newsel.writefds = linux_args.writefds;
	newsel.exceptfds = linux_args.exceptfds;
	newsel.timeout = linux_args.timeout;
	return (linux_newselect(p, &newsel));
}

int
linux_fork(struct proc *p, struct linux_fork_args *args)
{
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): fork()\n", (long)p->p_pid);
#endif

	if ((error = fork(p, (struct fork_args *)args)) != 0)
		return (error);

	if (p->p_retval[1] == 1)
		p->p_retval[0] = 0;
	return (0);
}

int
linux_vfork(struct proc *p, struct linux_vfork_args *args)
{
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): vfork()\n", (long)p->p_pid);
#endif

	if ((error = vfork(p, (struct vfork_args *)args)) != 0)
		return (error);
	/* Are we the child? */
	if (p->p_retval[1] == 1)
		p->p_retval[0] = 0;
	return (0);
}

#define CLONE_VM	0x100
#define CLONE_FS	0x200
#define CLONE_FILES	0x400
#define CLONE_SIGHAND	0x800
#define CLONE_PID	0x1000

int
linux_clone(struct proc *p, struct linux_clone_args *args)
{
	int error, ff = RFPROC;
	struct proc *p2;
	int exit_signal;
	vm_offset_t start;
	struct rfork_args rf_args;

#ifdef DEBUG
	if (args->flags & CLONE_PID)
		printf("linux_clone(%ld): CLONE_PID not yet supported\n",
		    (long)p->p_pid);
	printf("linux_clone(%ld): invoked with flags %x and stack %x\n",
	    (long)p->p_pid, (unsigned int)args->flags,
	    (unsigned int)args->stack);
#endif

	if (!args->stack)
		return (EINVAL);

	exit_signal = args->flags & 0x000000ff;
	if (exit_signal >= LINUX_NSIG)
		return (EINVAL);

	if (exit_signal <= LINUX_SIGTBLSZ)
		exit_signal = linux_to_bsd_signal[_SIG_IDX(exit_signal)];

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

	rf_args.flags = ff;
	if ((error = rfork(p, &rf_args)) != 0)
		return (error);

	p2 = pfind(p->p_retval[0]);
	if (p2 == 0)
		return (ESRCH);

	p2->p_sigparent = exit_signal;
	p2->p_md.md_regs->tf_esp = (unsigned int)args->stack;

#ifdef DEBUG
	printf ("linux_clone(%ld): successful rfork to %ld\n", (long)p->p_pid,
	    (long)p2->p_pid);
#endif

	return (0);
}

/* XXX move */
struct linux_mmap_argv {
	linux_caddr_t addr;
	int len;
	int prot;
	int flags;
	int fd;
	int pos;
};

#define STACK_SIZE  (2 * 1024 * 1024)
#define GUARD_SIZE  (4 * PAGE_SIZE)

int
linux_mmap(struct proc *p, struct linux_mmap_args *args)
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
	struct linux_mmap_argv linux_args;

	error = copyin(args->ptr, &linux_args, sizeof(linux_args));
	if (error)
		return (error);

#ifdef DEBUG
	printf("Linux-emul(%ld): mmap(%p, %d, %d, 0x%08x, %d, %d)",
	    (long)p->p_pid, (void *)linux_args.addr, linux_args.len,
	    linux_args.prot, linux_args.flags, linux_args.fd, linux_args.pos);
#endif

	bsd_args.flags = 0;
	if (linux_args.flags & LINUX_MAP_SHARED)
		bsd_args.flags |= MAP_SHARED;
	if (linux_args.flags & LINUX_MAP_PRIVATE)
		bsd_args.flags |= MAP_PRIVATE;
	if (linux_args.flags & LINUX_MAP_FIXED)
		bsd_args.flags |= MAP_FIXED;
	if (linux_args.flags & LINUX_MAP_ANON)
		bsd_args.flags |= MAP_ANON;
	else
		bsd_args.flags |= MAP_NOSYNC;
	if (linux_args.flags & LINUX_MAP_GROWSDOWN) {
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
		bsd_args.addr = linux_args.addr + linux_args.len;

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
			p->p_vmspace->vm_maxsaddr = (char *)USRSTACK -
			    p->p_rlimit[RLIMIT_STACK].rlim_cur;
		}

		/* This gives us our maximum stack size */
		if (linux_args.len > STACK_SIZE - GUARD_SIZE)
			bsd_args.len = linux_args.len;
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
		bsd_args.addr = linux_args.addr;
		bsd_args.len  = linux_args.len;
	}

	bsd_args.prot = linux_args.prot | PROT_READ;	/* always required */
	if (linux_args.flags & LINUX_MAP_ANON)
		bsd_args.fd = -1;
	else
		bsd_args.fd = linux_args.fd;
	bsd_args.pos = linux_args.pos;
	bsd_args.pad = 0;

#ifdef DEBUG
	printf("-> (%p, %d, %d, 0x%08x, %d, %d)\n", (void *)bsd_args.addr,
	    bsd_args.len, bsd_args.prot, bsd_args.flags, bsd_args.fd,
	    (int)bsd_args.pos);
#endif

	return (mmap(p, &bsd_args));
}

int
linux_pipe(struct proc *p, struct linux_pipe_args *args)
{
	int error;
	int reg_edx;

#ifdef DEBUG
	printf("Linux-emul(%ld): pipe(*)\n", (long)p->p_pid);
#endif

	reg_edx = p->p_retval[1];
	error = pipe(p, 0);
	if (error) {
		p->p_retval[1] = reg_edx;
		return (error);
	}

	error = copyout(p->p_retval, args->pipefds, 2*sizeof(int));
	if (error) {
		p->p_retval[1] = reg_edx;
		return (error);
	}

	p->p_retval[1] = reg_edx;
	p->p_retval[0] = 0;
	return (0);
}

int
linux_ioperm(struct proc *p, struct linux_ioperm_args *args)
{
	struct sysarch_args sa;
	struct i386_ioperm_args *iia;
	caddr_t sg;

	sg = stackgap_init();
	iia = stackgap_alloc(&sg, sizeof(struct i386_ioperm_args));
	iia->start = args->start;
	iia->length = args->length;
	iia->enable = args->enable;
	sa.op = I386_SET_IOPERM;
	sa.parms = (char *)iia;
	return (sysarch(p, &sa));
}

int
linux_iopl(struct proc *p, struct linux_iopl_args *args)
{
	int error;

	if (args->level < 0 || args->level > 3)
		return (EINVAL);
	if ((error = suser(p)) != 0)
		return (error);
	if (securelevel > 0)
		return (EPERM);
	p->p_md.md_regs->tf_eflags = (p->p_md.md_regs->tf_eflags & ~PSL_IOPL) |
	    (args->level * (PSL_IOPL / 3));
	return (0);
}

int
linux_modify_ldt(p, uap)
	struct proc *p;
	struct linux_modify_ldt_args *uap;
{
	int error;
	caddr_t sg;
	struct sysarch_args args;
	struct i386_ldt_args *ldt;
	struct linux_descriptor ld;
	union descriptor *desc;

	sg = stackgap_init();

	if (uap->ptr == NULL)
		return (EINVAL);

	switch (uap->func) {
	case 0x00: /* read_ldt */
		ldt = stackgap_alloc(&sg, sizeof(*ldt));
		ldt->start = 0;
		ldt->descs = uap->ptr;
		ldt->num = uap->bytecount / sizeof(union descriptor);
		args.op = I386_GET_LDT;
		args.parms = (char*)ldt;
		error = sysarch(p, &args);
		p->p_retval[0] *= sizeof(union descriptor);
		break;
	case 0x01: /* write_ldt */
	case 0x11: /* write_ldt */
		if (uap->bytecount != sizeof(ld))
			return (EINVAL);

		error = copyin(uap->ptr, &ld, sizeof(ld));
		if (error)
			return (error);

		ldt = stackgap_alloc(&sg, sizeof(*ldt));
		desc = stackgap_alloc(&sg, sizeof(*desc));
		ldt->start = ld.entry_number;
		ldt->descs = desc;
		ldt->num = 1;
		desc->sd.sd_lolimit = (ld.limit & 0x0000ffff);
		desc->sd.sd_hilimit = (ld.limit & 0x000f0000) >> 16;
		desc->sd.sd_lobase = (ld.base_addr & 0x00ffffff);
		desc->sd.sd_hibase = (ld.base_addr & 0xff000000) >> 24;
		desc->sd.sd_type = SDT_MEMRO | ((ld.read_exec_only ^ 1) << 1) |
			(ld.contents << 2);
		desc->sd.sd_dpl = 3;
		desc->sd.sd_p = (ld.seg_not_present ^ 1);
		desc->sd.sd_xx = 0;
		desc->sd.sd_def32 = ld.seg_32bit;
		desc->sd.sd_gran = ld.limit_in_pages;
		args.op = I386_SET_LDT;
		args.parms = (char*)ldt;
		error = sysarch(p, &args);
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
linux_sigaction(struct proc *p, struct linux_sigaction_args *args)
{
	linux_osigaction_t osa;
	linux_sigaction_t act, oact;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): sigaction(%d, %p, %p)\n", (long)p->p_pid,
	       args->sig, (void *)args->nsa, (void *)args->osa);
#endif

	if (args->nsa != NULL) {
		error = copyin(args->nsa, &osa, sizeof(linux_osigaction_t));
		if (error)
			return (error);
		act.lsa_handler = osa.lsa_handler;
		act.lsa_flags = osa.lsa_flags;
		act.lsa_restorer = osa.lsa_restorer;
		LINUX_SIGEMPTYSET(act.lsa_mask);
		act.lsa_mask.__bits[0] = osa.lsa_mask;
	}

	error = linux_do_sigaction(p, args->sig, args->nsa ? &act : NULL,
	    args->osa ? &oact : NULL);

	if (args->osa != NULL && !error) {
		osa.lsa_handler = oact.lsa_handler;
		osa.lsa_flags = oact.lsa_flags;
		osa.lsa_restorer = oact.lsa_restorer;
		osa.lsa_mask = oact.lsa_mask.__bits[0];
		error = copyout(&osa, args->osa, sizeof(linux_osigaction_t));
	}

	return (error);
}

/*
 * Linux has two extra args, restart and oldmask.  We dont use these,
 * but it seems that "restart" is actually a context pointer that
 * enables the signal to happen with a different register set.
 */
int
linux_sigsuspend(struct proc *p, struct linux_sigsuspend_args *args)
{
	struct sigsuspend_args bsd;
	sigset_t *sigmask;
	linux_sigset_t mask;
	caddr_t sg = stackgap_init();

#ifdef DEBUG
	printf("Linux-emul(%ld): sigsuspend(%08lx)\n",
	       (long)p->p_pid, (unsigned long)args->mask);
#endif

	sigmask = stackgap_alloc(&sg, sizeof(sigset_t));
	LINUX_SIGEMPTYSET(mask);
	mask.__bits[0] = args->mask;
	linux_to_bsd_sigset(&mask, sigmask);
	bsd.sigmask = sigmask;
	return (sigsuspend(p, &bsd));
}

int
linux_rt_sigsuspend(p, uap)
	struct proc *p;
	struct linux_rt_sigsuspend_args *uap;
{
	linux_sigset_t lmask;
	sigset_t *bmask;
	struct sigsuspend_args bsd;
	caddr_t sg = stackgap_init();
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): rt_sigsuspend(%p, %d)\n", (long)p->p_pid,
	       (void *)uap->newset, uap->sigsetsize);
#endif

	if (uap->sigsetsize != sizeof(linux_sigset_t))
		return (EINVAL);

	error = copyin(uap->newset, &lmask, sizeof(linux_sigset_t));
	if (error)
		return (error);

	bmask = stackgap_alloc(&sg, sizeof(sigset_t));
	linux_to_bsd_sigset(&lmask, bmask);
	bsd.sigmask = bmask;
	return (sigsuspend(p, &bsd));
}

int
linux_pause(struct proc *p, struct linux_pause_args *args)
{
	struct sigsuspend_args bsd;
	sigset_t *sigmask;
	caddr_t sg = stackgap_init();

#ifdef DEBUG
	printf("Linux-emul(%d): pause()\n", p->p_pid);
#endif

	sigmask = stackgap_alloc(&sg, sizeof(sigset_t));
	*sigmask = p->p_sigmask;
	bsd.sigmask = sigmask;
	return (sigsuspend(p, &bsd));
}

int
linux_sigaltstack(p, uap)
	struct proc *p;
	struct linux_sigaltstack_args *uap;
{
	struct sigaltstack_args bsd;
	stack_t *ss, *oss;
	linux_stack_t lss;
	int error;
	caddr_t sg = stackgap_init();

#ifdef DEBUG
	printf("Linux-emul(%ld): sigaltstack(%p, %p)\n",
	    (long)p->p_pid, uap->uss, uap->uoss);
#endif

	if (uap->uss == NULL) {
		ss = NULL;
	} else {
		error = copyin(uap->uss, &lss, sizeof(linux_stack_t));
		if (error)
			return (error);

		ss = stackgap_alloc(&sg, sizeof(stack_t));
		ss->ss_sp = lss.ss_sp;
		ss->ss_size = lss.ss_size;
		ss->ss_flags = linux_to_bsd_sigaltstack(lss.ss_flags);
	}
	oss = (uap->uoss != NULL)
	    ? stackgap_alloc(&sg, sizeof(stack_t))
	    : NULL;

	bsd.ss = ss;
	bsd.oss = oss;
	error = sigaltstack(p, &bsd);

	if (!error && oss != NULL) {
		lss.ss_sp = oss->ss_sp;
		lss.ss_size = oss->ss_size;
		lss.ss_flags = bsd_to_linux_sigaltstack(oss->ss_flags);
		error = copyout(&lss, uap->uoss, sizeof(linux_stack_t));
	}

	return (error);
}
