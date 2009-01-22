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

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <amd64/linux32/linux.h>
#include <amd64/linux32/linux32_proto.h>
#include <compat/linux/linux_ipc.h>
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

/*
 * Custom version of exec_copyin_args() so that we can translate
 * the pointers.
 */
static int
linux_exec_copyin_args(struct image_args *args, char *fname,
    enum uio_seg segflg, char **argv, char **envv)
{
	char *argp, *envp;
	u_int32_t *p32, arg;
	size_t length;
	int error;

	bzero(args, sizeof(*args));
	if (argv == NULL)
		return (EFAULT);

	/*
	 * Allocate temporary demand zeroed space for argument and
	 *	environment strings
	 */
	args->buf = (char *)kmem_alloc_wait(exec_map,
	    PATH_MAX + ARG_MAX + MAXSHELLCMDLEN);
	if (args->buf == NULL)
		return (ENOMEM);
	args->begin_argv = args->buf;
	args->endp = args->begin_argv;
	args->stringspace = ARG_MAX;

	args->fname = args->buf + ARG_MAX;

	/*
	 * Copy the file name.
	 */
	error = (segflg == UIO_SYSSPACE) ?
	    copystr(fname, args->fname, PATH_MAX, &length) :
	    copyinstr(fname, args->fname, PATH_MAX, &length);
	if (error != 0)
		goto err_exit;

	/*
	 * extract arguments first
	 */
	p32 = (u_int32_t *)argv;
	for (;;) {
		error = copyin(p32++, &arg, sizeof(arg));
		if (error)
			goto err_exit;
		if (arg == 0)
			break;
		argp = PTRIN(arg);
		error = copyinstr(argp, args->endp, args->stringspace, &length);
		if (error) {
			if (error == ENAMETOOLONG)
				error = E2BIG;

			goto err_exit;
		}
		args->stringspace -= length;
		args->endp += length;
		args->argc++;
	}

	args->begin_envv = args->endp;

	/*
	 * extract environment strings
	 */
	if (envv) {
		p32 = (u_int32_t *)envv;
		for (;;) {
			error = copyin(p32++, &arg, sizeof(arg));
			if (error)
				goto err_exit;
			if (arg == 0)
				break;
			envp = PTRIN(arg);
			error = copyinstr(envp, args->endp, args->stringspace,
			    &length);
			if (error) {
				if (error == ENAMETOOLONG)
					error = E2BIG;
				goto err_exit;
			}
			args->stringspace -= length;
			args->endp += length;
			args->envc++;
		}
	}

	return (0);

err_exit:
	kmem_free_wakeup(exec_map, (vm_offset_t)args->buf,
	    PATH_MAX + ARG_MAX + MAXSHELLCMDLEN);
	args->buf = NULL;
	return (error);
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

	error = linux_exec_copyin_args(&eargs, path, UIO_SYSSPACE, args->argp,
	    args->envp);
	free(path, M_TEMP);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL);
	if (error == 0)
		/* Linux process can execute FreeBSD one, do not attempt
		 * to create emuldata for such process using
		 * linux_proc_init, this leads to a panic on KASSERT
		 * because such process has p->p_emuldata == NULL.
		 */
	   	if (td->td_proc->p_sysent == &elf_linux_sysvec)
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
linux_fork(struct thread *td, struct linux_fork_args *args)
{
	int error;
	struct proc *p2;
	struct thread *td2;

#ifdef DEBUG
	if (ldebug(fork))
		printf(ARGS(fork, ""));
#endif

	if ((error = fork1(td, RFFDG | RFPROC | RFSTOPPED, 0, &p2)) != 0)
		return (error);

	if (error == 0) {
		td->td_retval[0] = p2->p_pid;
		td->td_retval[1] = 0;
	}

	if (td->td_retval[1] == 1)
		td->td_retval[0] = 0;
	error = linux_proc_init(td, td->td_retval[0], 0);
	if (error)
		return (error);

	td2 = FIRST_THREAD_IN_PROC(p2);

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(td2);
	TD_SET_CAN_RUN(td2);
	sched_add(td2, SRQ_BORING);
	thread_unlock(td2);

	return (0);
}

int
linux_vfork(struct thread *td, struct linux_vfork_args *args)
{
	int error;
	struct proc *p2;
	struct thread *td2;

#ifdef DEBUG
	if (ldebug(vfork))
		printf(ARGS(vfork, ""));
#endif

	/* Exclude RFPPWAIT */
	if ((error = fork1(td, RFFDG | RFPROC | RFMEM | RFSTOPPED, 0, &p2)) != 0)
		return (error);
	if (error == 0) {
	   	td->td_retval[0] = p2->p_pid;
		td->td_retval[1] = 0;
	}
	/* Are we the child? */
	if (td->td_retval[1] == 1)
		td->td_retval[0] = 0;
	error = linux_proc_init(td, td->td_retval[0], 0);
	if (error)
		return (error);

	PROC_LOCK(p2);
	p2->p_flag |= P_PPWAIT;
	PROC_UNLOCK(p2);

	td2 = FIRST_THREAD_IN_PROC(p2);

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(td2);
	TD_SET_CAN_RUN(td2);
	sched_add(td2, SRQ_BORING);
	thread_unlock(td2);

	/* wait for the children to exit, ie. emulate vfork */
	PROC_LOCK(p2);
	while (p2->p_flag & P_PPWAIT)
	   	msleep(td->td_proc, &p2->p_mtx, PWAIT, "ppwait", 0);
	PROC_UNLOCK(p2);

	return (0);
}

int
linux_clone(struct thread *td, struct linux_clone_args *args)
{
	int error, ff = RFPROC | RFSTOPPED;
	struct proc *p2;
	struct thread *td2;
	int exit_signal;
	struct linux_emuldata *em;

#ifdef DEBUG
	if (ldebug(clone)) {
		printf(ARGS(clone, "flags %x, stack %p, parent tid: %p, "
		    "child tid: %p"), (unsigned)args->flags,
		    args->stack, args->parent_tidptr, args->child_tidptr);
	}
#endif

	exit_signal = args->flags & 0x000000ff;
	if (LINUX_SIG_VALID(exit_signal)) {
		if (exit_signal <= LINUX_SIGTBLSZ)
			exit_signal =
			    linux_to_bsd_signal[_SIG_IDX(exit_signal)];
	} else if (exit_signal != 0)
		return (EINVAL);

	if (args->flags & LINUX_CLONE_VM)
		ff |= RFMEM;
	if (args->flags & LINUX_CLONE_SIGHAND)
		ff |= RFSIGSHARE;
	/*
	 * XXX: In Linux, sharing of fs info (chroot/cwd/umask)
	 * and open files is independant.  In FreeBSD, its in one
	 * structure but in reality it does not cause any problems
	 * because both of these flags are usually set together.
	 */
	if (!(args->flags & (LINUX_CLONE_FILES | LINUX_CLONE_FS)))
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
	if ((args->flags & 0xffffff00) == LINUX_THREADING_FLAGS)
		ff |= RFTHREAD;

	if (args->flags & LINUX_CLONE_PARENT_SETTID)
		if (args->parent_tidptr == NULL)
			return (EINVAL);

	error = fork1(td, ff, 0, &p2);
	if (error)
		return (error);

	if (args->flags & (LINUX_CLONE_PARENT | LINUX_CLONE_THREAD)) {
	   	sx_xlock(&proctree_lock);
		PROC_LOCK(p2);
		proc_reparent(p2, td->td_proc->p_pptr);
		PROC_UNLOCK(p2);
		sx_xunlock(&proctree_lock);
	}

	/* create the emuldata */
	error = linux_proc_init(td, p2->p_pid, args->flags);
	/* reference it - no need to check this */
	em = em_find(p2, EMUL_DOLOCK);
	KASSERT(em != NULL, ("clone: emuldata not found.\n"));
	/* and adjust it */

	if (args->flags & LINUX_CLONE_THREAD) {
#ifdef notyet
	   	PROC_LOCK(p2);
	   	p2->p_pgrp = td->td_proc->p_pgrp;
	   	PROC_UNLOCK(p2);
#endif
		exit_signal = 0;
	}

	if (args->flags & LINUX_CLONE_CHILD_SETTID)
		em->child_set_tid = args->child_tidptr;
	else
	   	em->child_set_tid = NULL;

	if (args->flags & LINUX_CLONE_CHILD_CLEARTID)
		em->child_clear_tid = args->child_tidptr;
	else
	   	em->child_clear_tid = NULL;

	EMUL_UNLOCK(&emul_lock);

	if (args->flags & LINUX_CLONE_PARENT_SETTID) {
		error = copyout(&p2->p_pid, args->parent_tidptr,
		    sizeof(p2->p_pid));
		if (error)
			printf(LMSG("copyout failed!"));
	}

	PROC_LOCK(p2);
	p2->p_sigparent = exit_signal;
	PROC_UNLOCK(p2);
	td2 = FIRST_THREAD_IN_PROC(p2);
	/*
	 * In a case of stack = NULL, we are supposed to COW calling process
	 * stack. This is what normal fork() does, so we just keep tf_rsp arg
	 * intact.
	 */
	if (args->stack)
		td2->td_frame->tf_rsp = PTROUT(args->stack);

	if (args->flags & LINUX_CLONE_SETTLS) {
		struct user_segment_descriptor sd;
		struct l_user_desc info;
		int a[2];

		error = copyin((void *)td->td_frame->tf_rsi, &info,
		    sizeof(struct l_user_desc));
		if (error) {
			printf(LMSG("copyin failed!"));
		} else {
			/* We might copy out the entry_number as GUGS32_SEL. */
			info.entry_number = GUGS32_SEL;
			error = copyout(&info, (void *)td->td_frame->tf_rsi,
			    sizeof(struct l_user_desc));
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
			td2->td_pcb->pcb_gsbase = (register_t)info.base_addr;
			td2->td_pcb->pcb_gs32sd = sd;
			td2->td_pcb->pcb_gs = GSEL(GUGS32_SEL, SEL_UPL);
			td2->td_pcb->pcb_flags |= PCB_GS32BIT | PCB_32BIT;
		}
	}

#ifdef DEBUG
	if (ldebug(clone))
		printf(LMSG("clone: successful rfork to %d, "
		    "stack %p sig = %d"), (int)p2->p_pid, args->stack,
		    exit_signal);
#endif
	if (args->flags & LINUX_CLONE_VFORK) {
	   	PROC_LOCK(p2);
	   	p2->p_flag |= P_PPWAIT;
	   	PROC_UNLOCK(p2);
	}

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(td2);
	TD_SET_CAN_RUN(td2);
	sched_add(td2, SRQ_BORING);
	thread_unlock(td2);

	td->td_retval[0] = p2->p_pid;
	td->td_retval[1] = 0;

	if (args->flags & LINUX_CLONE_VFORK) {
		/* wait for the children to exit, ie. emulate vfork */
		PROC_LOCK(p2);
		while (p2->p_flag & P_PPWAIT)
			msleep(td->td_proc, &p2->p_mtx, PWAIT, "ppwait", 0);
		PROC_UNLOCK(p2);
	}

	return (0);
}

#define STACK_SIZE  (2 * 1024 * 1024)
#define GUARD_SIZE  (4 * PAGE_SIZE)

static int linux_mmap_common(struct thread *, struct l_mmap_argv *);

int
linux_mmap2(struct thread *td, struct linux_mmap2_args *args)
{
	struct l_mmap_argv linux_args;

#ifdef DEBUG
	if (ldebug(mmap2))
		printf(ARGS(mmap2, "0x%08x, %d, %d, 0x%08x, %d, %d"),
		    args->addr, args->len, args->prot,
		    args->flags, args->fd, args->pgoff);
#endif

	linux_args.addr = PTROUT(args->addr);
	linux_args.len = args->len;
	linux_args.prot = args->prot;
	linux_args.flags = args->flags;
	linux_args.fd = args->fd;
	linux_args.pgoff = args->pgoff;

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
		printf(ARGS(mmap, "0x%08x, %d, %d, 0x%08x, %d, %d"),
		    linux_args.addr, linux_args.len, linux_args.prot,
		    linux_args.flags, linux_args.fd, linux_args.pgoff);
#endif
	if ((linux_args.pgoff % PAGE_SIZE) != 0)
		return (EINVAL);
	linux_args.pgoff /= PAGE_SIZE;

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
	if (linux_args->flags & LINUX_MAP_GROWSDOWN)
		bsd_args.flags |= MAP_STACK;

	/*
	 * PROT_READ, PROT_WRITE, or PROT_EXEC implies PROT_READ and PROT_EXEC
	 * on Linux/i386. We do this to ensure maximum compatibility.
	 * Linux/ia64 does the same in i386 emulation mode.
	 */
	bsd_args.prot = linux_args->prot;
	if (bsd_args.prot & (PROT_READ | PROT_WRITE | PROT_EXEC))
		bsd_args.prot |= PROT_READ | PROT_EXEC;

	/* Linux does not check file descriptor when MAP_ANONYMOUS is set. */
	bsd_args.fd = (bsd_args.flags & MAP_ANON) ? -1 : linux_args->fd;
	if (bsd_args.fd != -1) {
		/*
		 * Linux follows Solaris mmap(2) description:
		 * The file descriptor fildes is opened with
		 * read permission, regardless of the
		 * protection options specified.
		 */

		if ((error = fget(td, bsd_args.fd, &fp)) != 0)
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

	if (linux_args->flags & LINUX_MAP_GROWSDOWN) {
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

		if ((caddr_t)PTRIN(linux_args->addr) + linux_args->len >
		    p->p_vmspace->vm_maxsaddr) {
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
		if (linux_args->len > STACK_SIZE - GUARD_SIZE) {
			bsd_args.addr = (caddr_t)PTRIN(linux_args->addr);
			bsd_args.len = linux_args->len;
		} else {
			bsd_args.addr = (caddr_t)PTRIN(linux_args->addr) -
			    (STACK_SIZE - GUARD_SIZE - linux_args->len);
			bsd_args.len = STACK_SIZE - GUARD_SIZE;
		}
	} else {
		bsd_args.addr = (caddr_t)PTRIN(linux_args->addr);
		bsd_args.len  = linux_args->len;
	}
	bsd_args.pos = (off_t)linux_args->pgoff * PAGE_SIZE;

#ifdef DEBUG
	if (ldebug(mmap))
		printf("-> %s(%p, %d, %d, 0x%08x, %d, 0x%x)\n",
		    __func__,
		    (void *)bsd_args.addr, (int)bsd_args.len, bsd_args.prot,
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
linux_mprotect(struct thread *td, struct linux_mprotect_args *uap)
{
	struct mprotect_args bsd_args;

	bsd_args.addr = uap->addr;
	bsd_args.len = uap->len;
	bsd_args.prot = uap->prot;
	if (bsd_args.prot & (PROT_READ | PROT_WRITE | PROT_EXEC))
		bsd_args.prot |= PROT_READ | PROT_EXEC;
	return (mprotect(td, &bsd_args));
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
	return ftruncate(td, &sa);
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
		s32.ru_utime.tv_sec = s.ru_utime.tv_sec;
		s32.ru_utime.tv_usec = s.ru_utime.tv_usec;
		s32.ru_stime.tv_sec = s.ru_stime.tv_sec;
		s32.ru_stime.tv_usec = s.ru_stime.tv_usec;
		s32.ru_maxrss = s.ru_maxrss;
		s32.ru_ixrss = s.ru_ixrss;
		s32.ru_idrss = s.ru_idrss;
		s32.ru_isrss = s.ru_isrss;
		s32.ru_minflt = s.ru_minflt;
		s32.ru_majflt = s.ru_majflt;
		s32.ru_nswap = s.ru_nswap;
		s32.ru_inblock = s.ru_inblock;
		s32.ru_oublock = s.ru_oublock;
		s32.ru_msgsnd = s.ru_msgsnd;
		s32.ru_msgrcv = s.ru_msgrcv;
		s32.ru_nsignals = s.ru_nsignals;
		s32.ru_nvcsw = s.ru_nvcsw;
		s32.ru_nivcsw = s.ru_nivcsw;
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

	critical_enter();
	td->td_pcb->pcb_gsbase = (register_t)info.base_addr;
	td->td_pcb->pcb_gs32sd = *PCPU_GET(gs32p) = sd;
	td->td_pcb->pcb_flags |= PCB_32BIT | PCB_GS32BIT;
	wrmsr(MSR_KGSBASE, td->td_pcb->pcb_gsbase);
	critical_exit();

	return (0);
}
