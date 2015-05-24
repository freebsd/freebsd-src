/*-
 * Copyright (c) 2006 Roman Divacky
 * Copyright (c) 2013 Dmitry Chagin
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
 *    derived from this software without specific prior written permission
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

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sdt.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_futex.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_util.h>

/**
 * Special DTrace provider for the linuxulator.
 *
 * In this file we define the provider for the entire linuxulator. All
 * modules (= files of the linuxulator) use it.
 *
 * We define a different name depending on the emulated bitsize, see
 * ../../<ARCH>/linux{,32}/linux.h, e.g.:
 *      native bitsize          = linuxulator
 *      amd64, 32bit emulation  = linuxulator32
 */
LIN_SDT_PROVIDER_DEFINE(LINUX_DTRACE);

/**
 * DTrace probes in this module.
 */
LIN_SDT_PROBE_DEFINE1(emul, em_find, entry, "struct thread *");
LIN_SDT_PROBE_DEFINE0(emul, em_find, return);
LIN_SDT_PROBE_DEFINE3(emul, proc_init, entry, "struct thread *",
    "struct thread *", "int");
LIN_SDT_PROBE_DEFINE0(emul, proc_init, create_thread);
LIN_SDT_PROBE_DEFINE0(emul, proc_init, fork);
LIN_SDT_PROBE_DEFINE0(emul, proc_init, exec);
LIN_SDT_PROBE_DEFINE0(emul, proc_init, return);
LIN_SDT_PROBE_DEFINE1(emul, proc_exit, entry, "struct proc *");
LIN_SDT_PROBE_DEFINE1(emul, linux_thread_detach, entry, "struct thread *");
LIN_SDT_PROBE_DEFINE0(emul, linux_thread_detach, futex_failed);
LIN_SDT_PROBE_DEFINE1(emul, linux_thread_detach, child_clear_tid_error, "int");
LIN_SDT_PROBE_DEFINE0(emul, linux_thread_detach, return);
LIN_SDT_PROBE_DEFINE2(emul, proc_exec, entry, "struct proc *",
    "struct image_params *");
LIN_SDT_PROBE_DEFINE0(emul, proc_exec, return);
LIN_SDT_PROBE_DEFINE0(emul, linux_schedtail, entry);
LIN_SDT_PROBE_DEFINE1(emul, linux_schedtail, copyout_error, "int");
LIN_SDT_PROBE_DEFINE0(emul, linux_schedtail, return);
LIN_SDT_PROBE_DEFINE1(emul, linux_set_tid_address, entry, "int *");
LIN_SDT_PROBE_DEFINE0(emul, linux_set_tid_address, return);

/*
 * This returns reference to the emuldata entry (if found)
 *
 * Hold PROC_LOCK when referencing emuldata from other threads.
 */
struct linux_emuldata *
em_find(struct thread *td)
{
	struct linux_emuldata *em;

	LIN_SDT_PROBE1(emul, em_find, entry, td);

	em = td->td_emuldata;

	LIN_SDT_PROBE1(emul, em_find, return, em);

	return (em);
}

void
linux_proc_init(struct thread *td, struct thread *newtd, int flags)
{
	struct linux_emuldata *em;

	LIN_SDT_PROBE3(emul, proc_init, entry, td, newtd, flags);

	if (newtd != NULL) {
		/* non-exec call */
		em = malloc(sizeof(*em), M_TEMP, M_WAITOK | M_ZERO);
		em->pdeath_signal = 0;
		em->flags = 0;
		em->robust_futexes = NULL;
		if (flags & LINUX_CLONE_THREAD) {
			LIN_SDT_PROBE0(emul, proc_init, create_thread);

			em->em_tid = newtd->td_tid;
		} else {
			LIN_SDT_PROBE0(emul, proc_init, fork);

			em->em_tid = newtd->td_proc->p_pid;
		}
		newtd->td_emuldata = em;
	} else {
		/* exec */
		LIN_SDT_PROBE0(emul, proc_init, exec);

		/* lookup the old one */
		em = em_find(td);
		KASSERT(em != NULL, ("proc_init: emuldata not found in exec case.\n"));

		em->em_tid = td->td_proc->p_pid;
	}

	em->child_clear_tid = NULL;
	em->child_set_tid = NULL;

	LIN_SDT_PROBE0(emul, proc_init, return);
}

void 
linux_proc_exit(void *arg __unused, struct proc *p)
{
	struct thread *td = curthread;

	if (__predict_false(SV_CURPROC_ABI() != SV_ABI_LINUX)) {
		LIN_SDT_PROBE1(emul, proc_exit, entry, p);
		(p->p_sysent->sv_thread_detach)(td);
	}
}

int 
linux_common_execve(struct thread *td, struct image_args *eargs)
{
	struct linux_emuldata *em;
	struct proc *p;
	int error;

	p = td->td_proc;

	/*
	 * Unlike FreeBSD abort all other threads before
	 * proceeding exec.
	 */
	PROC_LOCK(p);
	/* See exit1() comments. */
	thread_suspend_check(0);
	while (p->p_flag & P_HADTHREADS) {
		if (!thread_single(p, SINGLE_EXIT))
			break;
		thread_suspend_check(0);
	}
	PROC_UNLOCK(p);

	error = kern_execve(td, eargs, NULL);
	if (error != 0)
		return (error);

	/*
	 * In a case of transition from Linux binary execing to
	 * FreeBSD binary we destroy linux emuldata thread entry.
	 */
	if (SV_CURPROC_ABI() != SV_ABI_LINUX) {
		PROC_LOCK(p);
		em = em_find(td);
		KASSERT(em != NULL, ("proc_exec: emuldata not found.\n"));
		td->td_emuldata = NULL;
		PROC_UNLOCK(p);

		free(em, M_TEMP);
	}
	return (0);
}

void 
linux_proc_exec(void *arg __unused, struct proc *p, struct image_params *imgp)
{
	struct thread *td = curthread;

	/*
	 * In a case of execing to linux binary we create linux
	 * emuldata thread entry.
	 */
	if (__predict_false((imgp->sysent->sv_flags & SV_ABI_MASK) ==
	    SV_ABI_LINUX)) {
		LIN_SDT_PROBE2(emul, proc_exec, entry, p, imgp);
		if (SV_PROC_ABI(p) == SV_ABI_LINUX)
			linux_proc_init(td, NULL, 0);
		else
			linux_proc_init(td, td, 0);

		LIN_SDT_PROBE0(emul, proc_exec, return);
	}
}

void
linux_thread_detach(struct thread *td)
{
	struct linux_sys_futex_args cup;
	struct linux_emuldata *em;
	int *child_clear_tid;
	int null = 0;
	int error;

	LIN_SDT_PROBE1(emul, linux_thread_detach, entry, td);

	em = em_find(td);
	KASSERT(em != NULL, ("thread_detach: emuldata not found.\n"));

	LINUX_CTR1(exit, "thread detach(%d)", em->em_tid);

	release_futexes(td, em);

	child_clear_tid = em->child_clear_tid;

	if (child_clear_tid != NULL) {

		LINUX_CTR2(exit, "thread detach(%d) %p",
		    em->em_tid, child_clear_tid);
	
		error = copyout(&null, child_clear_tid, sizeof(null));
		if (error) {
			LIN_SDT_PROBE1(emul, linux_thread_detach,
			    child_clear_tid_error, error);

			LIN_SDT_PROBE0(emul, linux_thread_detach, return);
			return;
		}

		cup.uaddr = child_clear_tid;
		cup.op = LINUX_FUTEX_WAKE;
		cup.val = 1;		/* wake one */
		cup.timeout = NULL;
		cup.uaddr2 = NULL;
		cup.val3 = 0;
		error = linux_sys_futex(td, &cup);
		/*
		 * this cannot happen at the moment and if this happens it
		 * probably means there is a user space bug
		 */
		if (error) {
			LIN_SDT_PROBE0(emul, linux_thread_detach, futex_failed);
			printf(LMSG("futex stuff in thread_detach failed.\n"));
		}
	}

	LIN_SDT_PROBE0(emul, linux_thread_detach, return);
}

void
linux_thread_dtor(void *arg __unused, struct thread *td)
{
	struct linux_emuldata *em;

	em = em_find(td);
	if (em == NULL)
		return;
	td->td_emuldata = NULL;

	LINUX_CTR1(exit, "thread dtor(%d)", em->em_tid);

	free(em, M_TEMP);
}

void
linux_schedtail(struct thread *td)
{
	struct linux_emuldata *em;
	struct proc *p;
	int error = 0;
	int *child_set_tid;

	LIN_SDT_PROBE1(emul, linux_schedtail, entry, td);

	p = td->td_proc;

	em = em_find(td);
	KASSERT(em != NULL, ("linux_schedtail: emuldata not found.\n"));
	child_set_tid = em->child_set_tid;

	if (child_set_tid != NULL) {
		error = copyout(&em->em_tid, (int *)child_set_tid,
		    sizeof(em->em_tid));
		LINUX_CTR4(clone, "schedtail(%d) %p stored %d error %d",
		    td->td_tid, child_set_tid, em->em_tid, error);

		if (error != 0) {
			LIN_SDT_PROBE1(emul, linux_schedtail, copyout_error,
			    error);
		}
	} else
		LINUX_CTR1(clone, "schedtail(%d)", em->em_tid);

	LIN_SDT_PROBE0(emul, linux_schedtail, return);
}

int
linux_set_tid_address(struct thread *td, struct linux_set_tid_address_args *args)
{
	struct linux_emuldata *em;

	LIN_SDT_PROBE1(emul, linux_set_tid_address, entry, args->tidptr);

	em = em_find(td);
	KASSERT(em != NULL, ("set_tid_address: emuldata not found.\n"));

	em->child_clear_tid = args->tidptr;

	td->td_retval[0] = em->em_tid;

	LINUX_CTR3(set_tid_address, "tidptr(%d) %p, returns %d",
	    em->em_tid, args->tidptr, td->td_retval[0]);

	LIN_SDT_PROBE0(emul, linux_set_tid_address, return);
	return (0);
}
