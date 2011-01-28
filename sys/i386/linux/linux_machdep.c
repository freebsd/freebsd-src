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
#include <sys/sx.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <sys/sched.h>

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
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_emul.h>

#include <i386/include/pcb.h>			/* needed for pcb definition in linux_set_thread_area */

#include "opt_posix.h"

extern struct sysentvec elf32_freebsd_sysvec;	/* defined in i386/i386/elf_machdep.c */

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

static int	linux_mmap_common(struct thread *td, l_uintptr_t addr,
		    l_size_t len, l_int prot, l_int flags, l_int fd,
		    l_loff_t pos);

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
	if (error == 0)
	   	/* linux process can exec fbsd one, dont attempt
		 * to create emuldata for such process using
		 * linux_proc_init, this leads to a panic on KASSERT
		 * because such process has p->p_emuldata == NULL
		 */
		if (SV_PROC_ABI(td->td_proc) == SV_ABI_LINUX)
   			error = linux_proc_init(td, 0, 0);
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

	/* exclude RFPPWAIT */
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
		cv_wait(&p2->p_pwait, &p2->p_mtx);
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
   	   	printf(ARGS(clone, "flags %x, stack %x, parent tid: %x, child tid: %x"),
		    (unsigned int)args->flags, (unsigned int)args->stack, 
		    (unsigned int)args->parent_tidptr, (unsigned int)args->child_tidptr);
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
	 * XXX: in linux sharing of fs info (chroot/cwd/umask)
	 * and open files is independant. in fbsd its in one
	 * structure but in reality it doesn't cause any problems
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
	   	/* XXX: linux mangles pgrp and pptr somehow
		 * I think it might be this but I am not sure.
		 */
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
		error = copyout(&p2->p_pid, args->parent_tidptr, sizeof(p2->p_pid));
		if (error)
			printf(LMSG("copyout failed!"));
	}

	PROC_LOCK(p2);
	p2->p_sigparent = exit_signal;
	PROC_UNLOCK(p2);
	td2 = FIRST_THREAD_IN_PROC(p2);
	/* 
	 * in a case of stack = NULL we are supposed to COW calling process stack
	 * this is what normal fork() does so we just keep the tf_esp arg intact
	 */
	if (args->stack)
   	   	td2->td_frame->tf_esp = (unsigned int)args->stack;

	if (args->flags & LINUX_CLONE_SETTLS) {
   	   	struct l_user_desc info;
   	   	int idx;
	   	int a[2];
		struct segment_descriptor sd;

	   	error = copyin((void *)td->td_frame->tf_esi, &info, sizeof(struct l_user_desc));
		if (error) {
			printf(LMSG("copyin failed!"));
		} else {
		
			idx = info.entry_number;
		
			/* 
			 * looks like we're getting the idx we returned
			 * in the set_thread_area() syscall
			 */
			if (idx != 6 && idx != 3) {
				printf(LMSG("resetting idx!"));
				idx = 3;
			}

			/* this doesnt happen in practice */
			if (idx == 6) {
		   		/* we might copy out the entry_number as 3 */
			   	info.entry_number = 3;
				error = copyout(&info, (void *) td->td_frame->tf_esi, sizeof(struct l_user_desc));
				if (error)
					printf(LMSG("copyout failed!"));
			}

			a[0] = LINUX_LDT_entry_a(&info);
			a[1] = LINUX_LDT_entry_b(&info);

			memcpy(&sd, &a, sizeof(a));
#ifdef DEBUG
		if (ldebug(clone))
		   	printf("Segment created in clone with CLONE_SETTLS: lobase: %x, hibase: %x, lolimit: %x, hilimit: %x, type: %i, dpl: %i, p: %i, xx: %i, def32: %i, gran: %i\n", sd.sd_lobase,
			sd.sd_hibase,
			sd.sd_lolimit,
			sd.sd_hilimit,
			sd.sd_type,
			sd.sd_dpl,
			sd.sd_p,
			sd.sd_xx,
			sd.sd_def32,
			sd.sd_gran);
#endif

			/* set %gs */
			td2->td_pcb->pcb_gsd = sd;
			td2->td_pcb->pcb_gs = GSEL(GUGS_SEL, SEL_UPL);
		}
	} 

#ifdef DEBUG
	if (ldebug(clone))
		printf(LMSG("clone: successful rfork to %ld, stack %p sig = %d"),
		    (long)p2->p_pid, args->stack, exit_signal);
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
			cv_wait(&p2->p_pwait, &p2->p_mtx);
		PROC_UNLOCK(p2);
	}

	return (0);
}

#define STACK_SIZE  (2 * 1024 * 1024)
#define GUARD_SIZE  (4 * PAGE_SIZE)

int
linux_mmap2(struct thread *td, struct linux_mmap2_args *args)
{

#ifdef DEBUG
	if (ldebug(mmap2))
		printf(ARGS(mmap2, "%p, %d, %d, 0x%08x, %d, %d"),
		    (void *)args->addr, args->len, args->prot,
		    args->flags, args->fd, args->pgoff);
#endif

	return (linux_mmap_common(td, args->addr, args->len, args->prot,
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
		printf(ARGS(mmap, "%p, %d, %d, 0x%08x, %d, %d"),
		    (void *)linux_args.addr, linux_args.len, linux_args.prot,
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

	if (flags & LINUX_MAP_GROWSDOWN) {
		/* 
		 * The Linux MAP_GROWSDOWN option does not limit auto
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

		if ((caddr_t)PTRIN(addr) + len > p->p_vmspace->vm_maxsaddr) {
			/* 
			 * Some linux apps will attempt to mmap
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
linux_ioperm(struct thread *td, struct linux_ioperm_args *args)
{
	int error;
	struct i386_ioperm_args iia;

	iia.start = args->start;
	iia.length = args->length;
	iia.enable = args->enable;
	error = i386_set_ioperm(td, &iia);
	return (error);
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
	int size, written;

	switch (uap->func) {
	case 0x00: /* read_ldt */
		ldt.start = 0;
		ldt.descs = uap->ptr;
		ldt.num = uap->bytecount / sizeof(union descriptor);
		error = i386_get_ldt(td, &ldt);
		td->td_retval[0] *= sizeof(union descriptor);
		break;
	case 0x02: /* read_default_ldt = 0 */
		size = 5*sizeof(struct l_desc_struct);
		if (size > uap->bytecount)
			size = uap->bytecount;
		for (written = error = 0; written < size && error == 0; written++)
			error = subyte((char *)uap->ptr + written, 0);
		td->td_retval[0] = written;
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
		error = i386_set_ldt(td, &ldt, &desc);
		break;
	default:
		error = ENOSYS;
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
	sa.length = args->length;
	return ftruncate(td, &sa);
}

int
linux_set_thread_area(struct thread *td, struct linux_set_thread_area_args *args)
{
	struct l_user_desc info;
	int error;
	int idx;
	int a[2];
	struct segment_descriptor sd;

	error = copyin(args->desc, &info, sizeof(struct l_user_desc));
	if (error)
		return (error);

#ifdef DEBUG
	if (ldebug(set_thread_area))
	   	printf(ARGS(set_thread_area, "%i, %x, %x, %i, %i, %i, %i, %i, %i\n"),
		      info.entry_number,
      		      info.base_addr,
      		      info.limit,
      		      info.seg_32bit,
		      info.contents,
      		      info.read_exec_only,
      		      info.limit_in_pages,
      		      info.seg_not_present,
      		      info.useable);
#endif

	idx = info.entry_number;
	/* 
	 * Semantics of linux version: every thread in the system has array of
	 * 3 tls descriptors. 1st is GLIBC TLS, 2nd is WINE, 3rd unknown. This 
	 * syscall loads one of the selected tls decriptors with a value and
	 * also loads GDT descriptors 6, 7 and 8 with the content of the
	 * per-thread descriptors.
	 *
	 * Semantics of fbsd version: I think we can ignore that linux has 3 
	 * per-thread descriptors and use just the 1st one. The tls_array[]
	 * is used only in set/get-thread_area() syscalls and for loading the
	 * GDT descriptors. In fbsd we use just one GDT descriptor for TLS so
	 * we will load just one. 
	 *
	 * XXX: this doesn't work when a user space process tries to use more
	 * than 1 TLS segment. Comment in the linux sources says wine might do
	 * this.
	 */

	/* 
	 * we support just GLIBC TLS now 
	 * we should let 3 proceed as well because we use this segment so
	 * if code does two subsequent calls it should succeed
	 */
	if (idx != 6 && idx != -1 && idx != 3)
		return (EINVAL);

	/* 
	 * we have to copy out the GDT entry we use
	 * FreeBSD uses GDT entry #3 for storing %gs so load that
	 *
	 * XXX: what if a user space program doesn't check this value and tries
	 * to use 6, 7 or 8? 
	 */
	idx = info.entry_number = 3;
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
	   	printf("Segment created in set_thread_area: lobase: %x, hibase: %x, lolimit: %x, hilimit: %x, type: %i, dpl: %i, p: %i, xx: %i, def32: %i, gran: %i\n", sd.sd_lobase,
			sd.sd_hibase,
			sd.sd_lolimit,
			sd.sd_hilimit,
			sd.sd_type,
			sd.sd_dpl,
			sd.sd_p,
			sd.sd_xx,
			sd.sd_def32,
			sd.sd_gran);
#endif

	/* this is taken from i386 version of cpu_set_user_tls() */
	critical_enter();
	/* set %gs */
	td->td_pcb->pcb_gsd = sd;
	PCPU_GET(fsgs_gdt)[1] = sd;
	load_gs(GSEL(GUGS_SEL, SEL_UPL));
	critical_exit();
   
	return (0);
}

int
linux_get_thread_area(struct thread *td, struct linux_get_thread_area_args *args)
{
   	
	struct l_user_desc info;
	int error;
	int idx;
	struct l_desc_struct desc;
	struct segment_descriptor sd;

#ifdef DEBUG
	if (ldebug(get_thread_area))
		printf(ARGS(get_thread_area, "%p"), args->desc);
#endif

	error = copyin(args->desc, &info, sizeof(struct l_user_desc));
	if (error)
		return (error);

	idx = info.entry_number;
	/* XXX: I am not sure if we want 3 to be allowed too. */
	if (idx != 6 && idx != 3)
		return (EINVAL);

	idx = 3;

	memset(&info, 0, sizeof(info));

	sd = PCPU_GET(fsgs_gdt)[1];

	memcpy(&desc, &sd, sizeof(desc));

	info.entry_number = idx;
	info.base_addr = LINUX_GET_BASE(&desc);
	info.limit = LINUX_GET_LIMIT(&desc);
	info.seg_32bit = LINUX_GET_32BIT(&desc);
	info.contents = LINUX_GET_CONTENTS(&desc);
	info.read_exec_only = !LINUX_GET_WRITABLE(&desc);
	info.limit_in_pages = LINUX_GET_LIMIT_PAGES(&desc);
	info.seg_not_present = !LINUX_GET_PRESENT(&desc);
	info.useable = LINUX_GET_USEABLE(&desc);

	error = copyout(&info, args->desc, sizeof(struct l_user_desc));
	if (error)
	   	return (EFAULT);

	return (0);
}

/* copied from kern/kern_time.c */
int
linux_timer_create(struct thread *td, struct linux_timer_create_args *args)
{
   	return ktimer_create(td, (struct ktimer_create_args *) args);
}

int
linux_timer_settime(struct thread *td, struct linux_timer_settime_args *args)
{
   	return ktimer_settime(td, (struct ktimer_settime_args *) args);
}

int
linux_timer_gettime(struct thread *td, struct linux_timer_gettime_args *args)
{
   	return ktimer_gettime(td, (struct ktimer_gettime_args *) args);
}

int
linux_timer_getoverrun(struct thread *td, struct linux_timer_getoverrun_args *args)
{
   	return ktimer_getoverrun(td, (struct ktimer_getoverrun_args *) args);
}

int
linux_timer_delete(struct thread *td, struct linux_timer_delete_args *args)
{
   	return ktimer_delete(td, (struct ktimer_delete_args *) args);
}

/* XXX: this wont work with module - convert it */
int
linux_mq_open(struct thread *td, struct linux_mq_open_args *args)
{
#ifdef P1003_1B_MQUEUE
   	return kmq_open(td, (struct kmq_open_args *) args);
#else
	return (ENOSYS);
#endif
}

int
linux_mq_unlink(struct thread *td, struct linux_mq_unlink_args *args)
{
#ifdef P1003_1B_MQUEUE
   	return kmq_unlink(td, (struct kmq_unlink_args *) args);
#else
	return (ENOSYS);
#endif
}

int
linux_mq_timedsend(struct thread *td, struct linux_mq_timedsend_args *args)
{
#ifdef P1003_1B_MQUEUE
   	return kmq_timedsend(td, (struct kmq_timedsend_args *) args);
#else
	return (ENOSYS);
#endif
}

int
linux_mq_timedreceive(struct thread *td, struct linux_mq_timedreceive_args *args)
{
#ifdef P1003_1B_MQUEUE
   	return kmq_timedreceive(td, (struct kmq_timedreceive_args *) args);
#else
	return (ENOSYS);
#endif
}

int
linux_mq_notify(struct thread *td, struct linux_mq_notify_args *args)
{
#ifdef P1003_1B_MQUEUE
	return kmq_notify(td, (struct kmq_notify_args *) args);
#else
	return (ENOSYS);
#endif
}

int
linux_mq_getsetattr(struct thread *td, struct linux_mq_getsetattr_args *args)
{
#ifdef P1003_1B_MQUEUE
   	return kmq_setattr(td, (struct kmq_setattr_args *) args);
#else
	return (ENOSYS);
#endif
}

int
linux_wait4(struct thread *td, struct linux_wait4_args *args)
{
	int error, options;
	struct rusage ru, *rup;
	struct proc *p;

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

	p = td->td_proc;
	PROC_LOCK(p);
	sigqueue_delete(&p->p_sigqueue, SIGCHLD);
	PROC_UNLOCK(p);

	if (args->rusage != NULL)
		error = copyout(&ru, args->rusage, sizeof(ru));

	return (error);
}
