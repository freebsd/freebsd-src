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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>

#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_util.h>


/*
 * This returns reference to the thread emuldata entry (if found)
 *
 * Hold PROC_LOCK when referencing emuldata from other threads.
 */
struct linux_emuldata *
em_find(struct thread *td)
{
	struct linux_emuldata *em;

	em = td->td_emuldata;

	return (em);
}

/*
 * This returns reference to the proc pemuldata entry (if found)
 *
 * Hold PROC_LOCK when referencing proc pemuldata from other threads.
 * Hold LINUX_PEM_LOCK wher referencing pemuldata members.
 */
struct linux_pemuldata *
pem_find(struct proc *p)
{
	struct linux_pemuldata *pem;

	pem = p->p_emuldata;

	return (pem);
}

void
linux_proc_init(struct thread *td, struct thread *newtd, int flags)
{
	struct linux_emuldata *em;
	struct linux_pemuldata *pem;
	struct epoll_emuldata *emd;

	if (newtd != NULL) {
		/* non-exec call */
		em = malloc(sizeof(*em), M_TEMP, M_WAITOK | M_ZERO);
		if (flags & LINUX_CLONE_THREAD) {
			LINUX_CTR1(proc_init, "thread newtd(%d)",
			    newtd->td_tid);

			em->em_tid = newtd->td_tid;
		} else {
			LINUX_CTR1(proc_init, "fork newtd(%d)",
			    newtd->td_proc->p_pid);

			em->em_tid = newtd->td_proc->p_pid;

			pem = malloc(sizeof(*pem), M_LINUX, M_WAITOK | M_ZERO);
			sx_init(&pem->pem_sx, "lpemlk");
			newtd->td_proc->p_emuldata = pem;
		}
		newtd->td_emuldata = em;
	} else {
		/* exec */
		LINUX_CTR1(proc_init, "exec newtd(%d)",
		    td->td_proc->p_pid);

		/* lookup the old one */
		em = em_find(td);
		KASSERT(em != NULL, ("proc_init: emuldata not found in exec case.\n"));

		em->em_tid = td->td_proc->p_pid;
		em->flags = 0;
		em->pdeath_signal = 0;
		em->robust_futexes = NULL;
		em->child_clear_tid = NULL;
		em->child_set_tid = NULL;

		 /* epoll should be destroyed in a case of exec. */
		pem = pem_find(td->td_proc);
		KASSERT(pem != NULL, ("proc_exit: proc emuldata not found.\n"));

		if (pem->epoll != NULL) {
			emd = pem->epoll;
			pem->epoll = NULL;
			free(emd, M_EPOLL);
		}
	}

}

void 
linux_proc_exit(void *arg __unused, struct proc *p)
{
	struct linux_pemuldata *pem;
	struct epoll_emuldata *emd;
	struct thread *td = curthread;

	if (__predict_false(SV_CURPROC_ABI() != SV_ABI_LINUX))
		return;

	pem = pem_find(p);
	if (pem == NULL)
		return;	
	(p->p_sysent->sv_thread_detach)(td);

	p->p_emuldata = NULL;

	if (pem->epoll != NULL) {
		emd = pem->epoll;
		pem->epoll = NULL;
		free(emd, M_EPOLL);
	}

	sx_destroy(&pem->pem_sx);
	free(pem, M_LINUX);
}

int 
linux_common_execve(struct thread *td, struct image_args *eargs)
{
	struct linux_pemuldata *pem;
	struct epoll_emuldata *emd;
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
	 * FreeBSD binary we destroy linux emuldata thread & proc entries.
	 */
	if (SV_CURPROC_ABI() != SV_ABI_LINUX) {
		PROC_LOCK(p);
		em = em_find(td);
		KASSERT(em != NULL, ("proc_exec: thread emuldata not found.\n"));
		td->td_emuldata = NULL;

		pem = pem_find(p);
		KASSERT(pem != NULL, ("proc_exec: proc pemuldata not found.\n"));
		p->p_emuldata = NULL;
		PROC_UNLOCK(p);

		if (pem->epoll != NULL) {
			emd = pem->epoll;
			pem->epoll = NULL;
			free(emd, M_EPOLL);
		}

		free(em, M_TEMP);
		free(pem, M_LINUX);
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

		if (SV_PROC_ABI(p) == SV_ABI_LINUX)
			linux_proc_init(td, NULL, 0);
		else
			linux_proc_init(td, td, 0);
	}
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

	p = td->td_proc;

	em = em_find(td);
	KASSERT(em != NULL, ("linux_schedtail: thread emuldata not found.\n"));
	child_set_tid = em->child_set_tid;

	if (child_set_tid != NULL) {
		error = copyout(&em->em_tid, child_set_tid,
		    sizeof(em->em_tid));
		LINUX_CTR4(clone, "schedtail(%d) %p stored %d error %d",
		    td->td_tid, child_set_tid, em->em_tid, error);
	} else
		LINUX_CTR1(clone, "schedtail(%d)", em->em_tid);
}
