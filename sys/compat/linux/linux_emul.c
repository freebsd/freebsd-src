/*-
 * Copyright (c) 2006 Roman Divacky
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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_futex.h>

struct sx	emul_shared_lock;
struct mtx	emul_lock;

/* this returns locked reference to the emuldata entry (if found) */
struct linux_emuldata *
em_find(struct proc *p, int locked)
{
	struct linux_emuldata *em;

	if (locked == EMUL_DOLOCK)
		EMUL_LOCK(&emul_lock);

	em = p->p_emuldata;

	if (em == NULL && locked == EMUL_DOLOCK)
		EMUL_UNLOCK(&emul_lock);

	return (em);
}

int
linux_proc_init(struct thread *td, pid_t child, int flags)
{
	struct linux_emuldata *em, *p_em;
	struct proc *p;

	if (child != 0) {
		/* non-exec call */
		em = malloc(sizeof *em, M_LINUX, M_WAITOK | M_ZERO);
		em->pid = child;
		em->pdeath_signal = 0;
		em->robust_futexes = NULL;
		if (flags & LINUX_CLONE_THREAD) {
			/* handled later in the code */
		} else {
			struct linux_emuldata_shared *s;

			s = malloc(sizeof *s, M_LINUX, M_WAITOK | M_ZERO);
			s->refs = 1;
			s->group_pid = child;

			LIST_INIT(&s->threads);
			em->shared = s;
		}
	} else {
		/* lookup the old one */
		em = em_find(td->td_proc, EMUL_DOLOCK);
		KASSERT(em != NULL, ("proc_init: emuldata not found in exec case.\n"));
	}

	em->child_clear_tid = NULL;
	em->child_set_tid = NULL;

	/*
	 * allocate the shared struct only in clone()/fork cases in the case
	 * of clone() td = calling proc and child = pid of the newly created
	 * proc
	 */
	if (child != 0) {
		if (flags & LINUX_CLONE_THREAD) {
			/* lookup the parent */
			/* 
			 * we dont have to lock the p_em because
			 * its waiting for us in linux_clone so
			 * there is no chance of it changing the
			 * p_em->shared address
			 */
			p_em = em_find(td->td_proc, EMUL_DONTLOCK);
			KASSERT(p_em != NULL, ("proc_init: parent emuldata not found for CLONE_THREAD\n"));
			em->shared = p_em->shared;
			EMUL_SHARED_WLOCK(&emul_shared_lock);
			em->shared->refs++;
			EMUL_SHARED_WUNLOCK(&emul_shared_lock);
		} else {
			/*
			 * handled earlier to avoid malloc(M_WAITOK) with
			 * rwlock held
			 */
		}
	}
	if (child != 0) {
		EMUL_SHARED_WLOCK(&emul_shared_lock);
		LIST_INSERT_HEAD(&em->shared->threads, em, threads);
		EMUL_SHARED_WUNLOCK(&emul_shared_lock);

		p = pfind(child);
		KASSERT(p != NULL, ("process not found in proc_init\n"));
		p->p_emuldata = em;
		PROC_UNLOCK(p);
	} else
		EMUL_UNLOCK(&emul_lock);

	return (0);
}

void
linux_proc_exit(void *arg __unused, struct proc *p)
{
	struct linux_emuldata *em;
	int error;
	struct thread *td = FIRST_THREAD_IN_PROC(p);
	int *child_clear_tid;
	struct proc *q, *nq;

	if (__predict_true(p->p_sysent != &elf_linux_sysvec))
		return;

	release_futexes(p);

	/* find the emuldata */
	em = em_find(p, EMUL_DOLOCK);

	KASSERT(em != NULL, ("proc_exit: emuldata not found.\n"));

	/* reparent all procs that are not a thread leader to initproc */
	if (em->shared->group_pid != p->p_pid) {
		child_clear_tid = em->child_clear_tid;
		EMUL_UNLOCK(&emul_lock);
		sx_xlock(&proctree_lock);
		wakeup(initproc);
		PROC_LOCK(p);
		proc_reparent(p, initproc);
		p->p_sigparent = SIGCHLD;
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
	} else {
		child_clear_tid = em->child_clear_tid;
		EMUL_UNLOCK(&emul_lock);	
	}

	EMUL_SHARED_WLOCK(&emul_shared_lock);
	LIST_REMOVE(em, threads);

	em->shared->refs--;
	if (em->shared->refs == 0) {
		EMUL_SHARED_WUNLOCK(&emul_shared_lock);
		free(em->shared, M_LINUX);
	} else	
		EMUL_SHARED_WUNLOCK(&emul_shared_lock);

	if (child_clear_tid != NULL) {
		struct linux_sys_futex_args cup;
		int null = 0;

		error = copyout(&null, child_clear_tid, sizeof(null));
		if (error) {
			free(em, M_LINUX);
			return;
		}

		/* futexes stuff */
		cup.uaddr = child_clear_tid;
		cup.op = LINUX_FUTEX_WAKE;
		cup.val = 0x7fffffff;	/* Awake everyone */
		cup.timeout = NULL;
		cup.uaddr2 = NULL;
		cup.val3 = 0;
		error = linux_sys_futex(FIRST_THREAD_IN_PROC(p), &cup);
		/*
		 * this cannot happen at the moment and if this happens it
		 * probably means there is a user space bug
		 */
		if (error)
			printf(LMSG("futex stuff in proc_exit failed.\n"));
	}

	/* clean the stuff up */
	free(em, M_LINUX);

	/* this is a little weird but rewritten from exit1() */
	sx_xlock(&proctree_lock);
	q = LIST_FIRST(&p->p_children);
	for (; q != NULL; q = nq) {
		nq = LIST_NEXT(q, p_sibling);
		if (q->p_flag & P_WEXIT)
			continue;
		if (__predict_false(q->p_sysent != &elf_linux_sysvec))
			continue;
		em = em_find(q, EMUL_DOLOCK);
		KASSERT(em != NULL, ("linux_reparent: emuldata not found: %i\n", q->p_pid));
		PROC_LOCK(q);
		if ((q->p_flag & P_WEXIT) == 0 && em->pdeath_signal != 0) {
			psignal(q, em->pdeath_signal);
		}
		PROC_UNLOCK(q);
		EMUL_UNLOCK(&emul_lock);
	}
	sx_xunlock(&proctree_lock);
}

/*
 * This is used in a case of transition from FreeBSD binary execing to linux binary
 * in this case we create linux emuldata proc entry with the pid of the currently running
 * process.
 */
void 
linux_proc_exec(void *arg __unused, struct proc *p, struct image_params *imgp)
{
	if (__predict_false(imgp->sysent == &elf_linux_sysvec
	    && p->p_sysent != &elf_linux_sysvec))
		linux_proc_init(FIRST_THREAD_IN_PROC(p), p->p_pid, 0);
	if (__predict_false(imgp->sysent != &elf_linux_sysvec
	    && p->p_sysent == &elf_linux_sysvec)) {
		struct linux_emuldata *em;

		/* 
		 * XXX:There's a race because here we assign p->p_emuldata NULL
		 * but the process is still counted as linux one for a short
 		 * time so some other process might reference it and try to
 		 * access its p->p_emuldata and panicing on a NULL reference.
		 */
		em = em_find(p, EMUL_DONTLOCK);

		KASSERT(em != NULL, ("proc_exec: emuldata not found.\n"));

		EMUL_SHARED_WLOCK(&emul_shared_lock);
		LIST_REMOVE(em, threads);

		PROC_LOCK(p);
		p->p_emuldata = NULL;
		PROC_UNLOCK(p);

		em->shared->refs--;
		if (em->shared->refs == 0) {
			EMUL_SHARED_WUNLOCK(&emul_shared_lock);
			free(em->shared, M_LINUX);
		} else
			EMUL_SHARED_WUNLOCK(&emul_shared_lock);

		free(em, M_LINUX);
	}
}

void
linux_schedtail(void *arg __unused, struct proc *p)
{
	struct linux_emuldata *em;
	int error = 0;
	int *child_set_tid;

	if (__predict_true(p->p_sysent != &elf_linux_sysvec))
		return;

	/* find the emuldata */
	em = em_find(p, EMUL_DOLOCK);

	KASSERT(em != NULL, ("linux_schedtail: emuldata not found.\n"));
	child_set_tid = em->child_set_tid;
	EMUL_UNLOCK(&emul_lock);

	if (child_set_tid != NULL)
		error = copyout(&p->p_pid, (int *)child_set_tid,
		    sizeof(p->p_pid));

	return;
}

int
linux_set_tid_address(struct thread *td, struct linux_set_tid_address_args *args)
{
	struct linux_emuldata *em;

#ifdef DEBUG
	if (ldebug(set_tid_address))
		printf(ARGS(set_tid_address, "%p"), args->tidptr);
#endif

	/* find the emuldata */
	em = em_find(td->td_proc, EMUL_DOLOCK);

	KASSERT(em != NULL, ("set_tid_address: emuldata not found.\n"));

	em->child_clear_tid = args->tidptr;
	td->td_retval[0] = td->td_proc->p_pid;

	EMUL_UNLOCK(&emul_lock);
	return 0;
}
