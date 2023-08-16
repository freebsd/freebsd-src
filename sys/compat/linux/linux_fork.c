/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <sys/param.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/racct.h>
#include <sys/sched.h>
#include <sys/syscallsubr.h>
#include <sys/sx.h>
#include <sys/umtxvar.h>
#include <sys/unistd.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_futex.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_util.h>

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_fork(struct thread *td, struct linux_fork_args *args)
{
	struct fork_req fr;
	int error;
	struct proc *p2;
	struct thread *td2;

	bzero(&fr, sizeof(fr));
	fr.fr_flags = RFFDG | RFPROC | RFSTOPPED;
	fr.fr_procp = &p2;
	if ((error = fork1(td, &fr)) != 0)
		return (error);

	td2 = FIRST_THREAD_IN_PROC(p2);

	linux_proc_init(td, td2, false);

	td->td_retval[0] = p2->p_pid;

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(td2);
	TD_SET_CAN_RUN(td2);
	sched_add(td2, SRQ_BORING);

	return (0);
}

int
linux_vfork(struct thread *td, struct linux_vfork_args *args)
{
	struct fork_req fr;
	int error;
	struct proc *p2;
	struct thread *td2;

	bzero(&fr, sizeof(fr));
	fr.fr_flags = RFFDG | RFPROC | RFMEM | RFPPWAIT | RFSTOPPED;
	fr.fr_procp = &p2;
	if ((error = fork1(td, &fr)) != 0)
		return (error);

	td2 = FIRST_THREAD_IN_PROC(p2);

	linux_proc_init(td, td2, false);

	td->td_retval[0] = p2->p_pid;

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(td2);
	TD_SET_CAN_RUN(td2);
	sched_add(td2, SRQ_BORING);

	return (0);
}
#endif

static int
linux_clone_proc(struct thread *td, struct l_clone_args *args)
{
	struct fork_req fr;
	int error, ff, f2;
	struct proc *p2;
	struct thread *td2;
	int exit_signal;
	struct linux_emuldata *em;

	f2 = 0;
	ff = RFPROC | RFSTOPPED;
	if (LINUX_SIG_VALID(args->exit_signal)) {
		exit_signal = linux_to_bsd_signal(args->exit_signal);
	} else if (args->exit_signal != 0)
		return (EINVAL);
	else
		exit_signal = 0;

	if (args->flags & LINUX_CLONE_VM)
		ff |= RFMEM;
	if (args->flags & LINUX_CLONE_SIGHAND)
		ff |= RFSIGSHARE;
	if ((args->flags & LINUX_CLONE_CLEAR_SIGHAND) != 0)
		f2 |= FR2_DROPSIG_CAUGHT;
	if (args->flags & LINUX_CLONE_FILES) {
		if (!(args->flags & LINUX_CLONE_FS))
			f2 |= FR2_SHARE_PATHS;
	} else {
		ff |= RFFDG;
		if (args->flags & LINUX_CLONE_FS)
			f2 |= FR2_SHARE_PATHS;
	}

	if (args->flags & LINUX_CLONE_PARENT_SETTID)
		if (args->parent_tid == NULL)
			return (EINVAL);

	if (args->flags & LINUX_CLONE_VFORK)
		ff |= RFPPWAIT;

	bzero(&fr, sizeof(fr));
	fr.fr_flags = ff;
	fr.fr_flags2 = f2;
	fr.fr_procp = &p2;
	error = fork1(td, &fr);
	if (error)
		return (error);

	td2 = FIRST_THREAD_IN_PROC(p2);

	/* create the emuldata */
	linux_proc_init(td, td2, false);

	em = em_find(td2);
	KASSERT(em != NULL, ("clone_proc: emuldata not found.\n"));

	if (args->flags & LINUX_CLONE_CHILD_SETTID)
		em->child_set_tid = args->child_tid;
	else
		em->child_set_tid = NULL;

	if (args->flags & LINUX_CLONE_CHILD_CLEARTID)
		em->child_clear_tid = args->child_tid;
	else
		em->child_clear_tid = NULL;

	if (args->flags & LINUX_CLONE_PARENT_SETTID) {
		error = copyout(&p2->p_pid, args->parent_tid,
		    sizeof(p2->p_pid));
		if (error)
			linux_msg(td, "copyout p_pid failed!");
	}

	PROC_LOCK(p2);
	p2->p_sigparent = exit_signal;
	PROC_UNLOCK(p2);
	/*
	 * In a case of stack = NULL, we are supposed to COW calling process
	 * stack. This is what normal fork() does, so we just keep tf_rsp arg
	 * intact.
	 */
	linux_set_upcall(td2, args->stack);

	if (args->flags & LINUX_CLONE_SETTLS)
		linux_set_cloned_tls(td2, PTRIN(args->tls));

	/*
	 * If CLONE_PARENT is set, then the parent of the new process will be
	 * the same as that of the calling process.
	 */
	if (args->flags & LINUX_CLONE_PARENT) {
		sx_xlock(&proctree_lock);
		PROC_LOCK(p2);
		proc_reparent(p2, td->td_proc->p_pptr, true);
		PROC_UNLOCK(p2);
		sx_xunlock(&proctree_lock);
	}

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(td2);
	TD_SET_CAN_RUN(td2);
	sched_add(td2, SRQ_BORING);

	td->td_retval[0] = p2->p_pid;

	return (0);
}

static int
linux_clone_thread(struct thread *td, struct l_clone_args *args)
{
	struct linux_emuldata *em;
	struct thread *newtd;
	struct proc *p;
	int error;

	LINUX_CTR4(clone_thread, "thread(%d) flags %x ptid %p ctid %p",
	    td->td_tid, (unsigned)args->flags,
	    args->parent_tid, args->child_tid);

	if ((args->flags & LINUX_CLONE_PARENT) != 0)
		return (EINVAL);
	if (args->flags & LINUX_CLONE_PARENT_SETTID)
		if (args->parent_tid == NULL)
			return (EINVAL);

	/* Threads should be created with own stack */
	if (PTRIN(args->stack) == NULL)
		return (EINVAL);

	p = td->td_proc;

#ifdef RACCT
	if (racct_enable) {
		PROC_LOCK(p);
		error = racct_add(p, RACCT_NTHR, 1);
		PROC_UNLOCK(p);
		if (error != 0)
			return (EPROCLIM);
	}
#endif

	/* Initialize our td */
	error = kern_thr_alloc(p, 0, &newtd);
	if (error)
		goto fail;

	bzero(&newtd->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &newtd->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));

	newtd->td_proc = p;
	thread_cow_get(newtd, td);

	cpu_copy_thread(newtd, td);

	/* create the emuldata */
	linux_proc_init(td, newtd, true);

	em = em_find(newtd);
	KASSERT(em != NULL, ("clone_thread: emuldata not found.\n"));

	if (args->flags & LINUX_CLONE_SETTLS)
		linux_set_cloned_tls(newtd, PTRIN(args->tls));

	if (args->flags & LINUX_CLONE_CHILD_SETTID)
		em->child_set_tid = args->child_tid;
	else
		em->child_set_tid = NULL;

	if (args->flags & LINUX_CLONE_CHILD_CLEARTID)
		em->child_clear_tid = args->child_tid;
	else
		em->child_clear_tid = NULL;

	cpu_thread_clean(newtd);

	linux_set_upcall(newtd, args->stack);

	PROC_LOCK(p);
	p->p_flag |= P_HADTHREADS;
	thread_link(newtd, p);
	bcopy(p->p_comm, newtd->td_name, sizeof(newtd->td_name));

	thread_lock(td);
	/* let the scheduler know about these things. */
	sched_fork_thread(td, newtd);
	thread_unlock(td);
	if (P_SHOULDSTOP(p))
		ast_sched(newtd, TDA_SUSPEND);

	if (p->p_ptevents & PTRACE_LWP)
		newtd->td_dbgflags |= TDB_BORN;
	PROC_UNLOCK(p);

	tidhash_add(newtd);

	LINUX_CTR2(clone_thread, "thread(%d) successful clone to %d",
	    td->td_tid, newtd->td_tid);

	if (args->flags & LINUX_CLONE_PARENT_SETTID) {
		error = copyout(&newtd->td_tid, args->parent_tid,
		    sizeof(newtd->td_tid));
		if (error)
			linux_msg(td, "clone_thread: copyout td_tid failed!");
	}

	/*
	 * Make this runnable after we are finished with it.
	 */
	thread_lock(newtd);
	TD_SET_CAN_RUN(newtd);
	sched_add(newtd, SRQ_BORING);

	td->td_retval[0] = newtd->td_tid;

	return (0);

fail:
#ifdef RACCT
	if (racct_enable) {
		PROC_LOCK(p);
		racct_sub(p, RACCT_NTHR, 1);
		PROC_UNLOCK(p);
	}
#endif
	return (error);
}

int
linux_clone(struct thread *td, struct linux_clone_args *args)
{
	struct l_clone_args ca = {
		.flags = (lower_32_bits(args->flags) & ~LINUX_CSIGNAL),
		.child_tid = args->child_tidptr,
		.parent_tid = args->parent_tidptr,
		.exit_signal = (lower_32_bits(args->flags) & LINUX_CSIGNAL),
		.stack = args->stack,
		.tls = args->tls,
	};

	if (args->flags & LINUX_CLONE_THREAD)
		return (linux_clone_thread(td, &ca));
	else
		return (linux_clone_proc(td, &ca));
}


static int
linux_clone3_args_valid(struct l_user_clone_args *uca)
{

	/* Verify that no unknown flags are passed along. */
	if ((uca->flags & ~(LINUX_CLONE_LEGACY_FLAGS |
	    LINUX_CLONE_CLEAR_SIGHAND | LINUX_CLONE_INTO_CGROUP)) != 0)
		return (EINVAL);
	if ((uca->flags & (LINUX_CLONE_DETACHED | LINUX_CSIGNAL)) != 0)
		return (EINVAL);

	if ((uca->flags & (LINUX_CLONE_SIGHAND | LINUX_CLONE_CLEAR_SIGHAND)) ==
	    (LINUX_CLONE_SIGHAND | LINUX_CLONE_CLEAR_SIGHAND))
		return (EINVAL);
	if ((uca->flags & (LINUX_CLONE_THREAD | LINUX_CLONE_PARENT)) != 0 &&
	    uca->exit_signal != 0)
		return (EINVAL);

	/* We don't support set_tid, only validate input. */
	if (uca->set_tid_size > LINUX_MAX_PID_NS_LEVEL)
		return (EINVAL);
	if (uca->set_tid == 0 && uca->set_tid_size > 0)
		return (EINVAL);
	if (uca->set_tid != 0 && uca->set_tid_size == 0)
		return (EINVAL);

	if (uca->stack == 0 && uca->stack_size > 0)
		return (EINVAL);
	if (uca->stack != 0 && uca->stack_size == 0)
		return (EINVAL);

	/* Verify that higher 32bits of exit_signal are unset. */
	if ((uca->exit_signal & ~(uint64_t)LINUX_CSIGNAL) != 0)
		return (EINVAL);

	/* Verify that no unsupported flags are passed along. */
	if ((uca->flags & LINUX_CLONE_NEWTIME) != 0) {
		LINUX_RATELIMIT_MSG("unsupported clone3 option CLONE_NEWTIME");
		return (ENOSYS);
	}
	if ((uca->flags & LINUX_CLONE_INTO_CGROUP) != 0) {
		LINUX_RATELIMIT_MSG("unsupported clone3 option CLONE_INTO_CGROUP");
		return (ENOSYS);
	}
	if (uca->set_tid != 0 || uca->set_tid_size != 0) {
		LINUX_RATELIMIT_MSG("unsupported clone3 set_tid");
		return (ENOSYS);
	}

	return (0);
}

int
linux_clone3(struct thread *td, struct linux_clone3_args *args)
{
	struct l_user_clone_args *uca;
	struct l_clone_args *ca;
	size_t size;
	int error;

	if (args->usize > PAGE_SIZE)
		return (E2BIG);
	if (args->usize < LINUX_CLONE_ARGS_SIZE_VER0)
		return (EINVAL);

	/*
	 * usize can be less than size of struct clone_args, to avoid using
	 * of uninitialized data of struct clone_args, allocate at least
	 * sizeof(struct clone_args) storage and zero it.
	 */
	size = max(args->usize, sizeof(*uca));
	uca = malloc(size, M_LINUX, M_WAITOK | M_ZERO);
	error = copyin(args->uargs, uca, args->usize);
	if (error != 0)
		goto out;
	error = linux_clone3_args_valid(uca);
	if (error != 0)
		goto out;
	ca = malloc(sizeof(*ca), M_LINUX, M_WAITOK | M_ZERO);
	ca->flags = uca->flags;
	ca->child_tid = PTRIN(uca->child_tid);
	ca->parent_tid = PTRIN(uca->parent_tid);
	ca->exit_signal = uca->exit_signal;
	ca->stack = uca->stack + uca->stack_size;
	ca->stack_size = uca->stack_size;
	ca->tls = uca->tls;

	if ((ca->flags & LINUX_CLONE_THREAD) != 0)
		error = linux_clone_thread(td, ca);
	else
		error = linux_clone_proc(td, ca);
	free(ca, M_LINUX);
out:
	free(uca, M_LINUX);
	return (error);
}

int
linux_exit(struct thread *td, struct linux_exit_args *args)
{
	struct linux_emuldata *em __diagused;

	em = em_find(td);
	KASSERT(em != NULL, ("exit: emuldata not found.\n"));

	LINUX_CTR2(exit, "thread(%d) (%d)", em->em_tid, args->rval);

	linux_thread_detach(td);

	/*
	 * XXX. When the last two threads of a process
	 * exit via pthread_exit() try thr_exit() first.
	 */
	kern_thr_exit(td);
	exit1(td, args->rval, 0);
		/* NOTREACHED */
}

int
linux_set_tid_address(struct thread *td, struct linux_set_tid_address_args *args)
{
	struct linux_emuldata *em;

	em = em_find(td);
	KASSERT(em != NULL, ("set_tid_address: emuldata not found.\n"));

	em->child_clear_tid = args->tidptr;

	td->td_retval[0] = em->em_tid;

	LINUX_CTR3(set_tid_address, "tidptr(%d) %p, returns %d",
	    em->em_tid, args->tidptr, td->td_retval[0]);

	return (0);
}

void
linux_thread_detach(struct thread *td)
{
	struct linux_emuldata *em;
	int *child_clear_tid;
	int error;

	em = em_find(td);
	KASSERT(em != NULL, ("thread_detach: emuldata not found.\n"));

	LINUX_CTR1(thread_detach, "thread(%d)", em->em_tid);

	release_futexes(td, em);

	child_clear_tid = em->child_clear_tid;

	if (child_clear_tid != NULL) {
		LINUX_CTR2(thread_detach, "thread(%d) %p",
		    em->em_tid, child_clear_tid);

		error = suword32(child_clear_tid, 0);
		if (error != 0)
			return;

		error = futex_wake(td, child_clear_tid, 1, false);
		/*
		 * this cannot happen at the moment and if this happens it
		 * probably means there is a user space bug
		 */
		if (error != 0)
			linux_msg(td, "futex stuff in thread_detach failed.");
	}

	/*
	 * Do not rely on the robust list which is maintained by userspace,
	 * cleanup remaining pi (if any) after release_futexes anyway.
	 */
	umtx_thread_exit(td);
}
