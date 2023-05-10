/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1996 Søren Schmidt
 * Copyright (c) 2006 Roman Divacky
 * All rights reserved.
 * Copyright (c) 2013 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>

#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_persona.h>
#include <compat/linux/linux_util.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define SHELLMAGIC	0x2123 /* #! */
#else
#define SHELLMAGIC	0x2321
#endif

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

/*
 * Linux apps generally expect the soft open file limit to be set
 * to 1024, often iterating over all the file descriptors up to that
 * limit instead of using closefrom(2).  Give them what they want,
 * unless there already is a resource limit in place.
 */
static void
linux_set_default_openfiles(struct thread *td, struct proc *p)
{
	struct rlimit rlim;
	int error __diagused;

	if (linux_default_openfiles < 0)
		return;

	PROC_LOCK(p);
	lim_rlimit_proc(p, RLIMIT_NOFILE, &rlim);
	PROC_UNLOCK(p);
	if (rlim.rlim_cur != rlim.rlim_max ||
	    rlim.rlim_cur <= linux_default_openfiles)
		return;
	rlim.rlim_cur = linux_default_openfiles;
	error = kern_proc_setrlimit(td, p, RLIMIT_NOFILE, &rlim);
	KASSERT(error == 0, ("kern_proc_setrlimit failed"));
}

/*
 * The default stack size limit in Linux is 8MB.
 */
static void
linux_set_default_stacksize(struct thread *td, struct proc *p)
{
	struct rlimit rlim;
	int error __diagused;

	if (linux_default_stacksize < 0)
		return;

	PROC_LOCK(p);
	lim_rlimit_proc(p, RLIMIT_STACK, &rlim);
	PROC_UNLOCK(p);
	if (rlim.rlim_cur != rlim.rlim_max ||
	    rlim.rlim_cur <= linux_default_stacksize)
		return;
	rlim.rlim_cur = linux_default_stacksize;
	error = kern_proc_setrlimit(td, p, RLIMIT_STACK, &rlim);
	KASSERT(error == 0, ("kern_proc_setrlimit failed"));
}

void
linux_proc_init(struct thread *td, struct thread *newtd, bool init_thread)
{
	struct linux_emuldata *em;
	struct linux_pemuldata *pem;
	struct proc *p;

	if (newtd != NULL) {
		p = newtd->td_proc;

		/* non-exec call */
		em = malloc(sizeof(*em), M_TEMP, M_WAITOK | M_ZERO);
		if (init_thread) {
			LINUX_CTR1(proc_init, "thread newtd(%d)",
			    newtd->td_tid);

			em->em_tid = newtd->td_tid;
		} else {
			LINUX_CTR1(proc_init, "fork newtd(%d)", p->p_pid);

			em->em_tid = p->p_pid;

			pem = malloc(sizeof(*pem), M_LINUX, M_WAITOK | M_ZERO);
			sx_init(&pem->pem_sx, "lpemlk");
			p->p_emuldata = pem;
		}
		newtd->td_emuldata = em;

		linux_set_default_openfiles(td, p);
		linux_set_default_stacksize(td, p);
	} else {
		p = td->td_proc;

		/* exec */
		LINUX_CTR1(proc_init, "exec newtd(%d)", p->p_pid);

		/* lookup the old one */
		em = em_find(td);
		KASSERT(em != NULL, ("proc_init: thread emuldata not found.\n"));

		em->em_tid = p->p_pid;
		em->flags = 0;
		em->robust_futexes = NULL;
		em->child_clear_tid = NULL;
		em->child_set_tid = NULL;

		pem = pem_find(p);
		KASSERT(pem != NULL, ("proc_init: proc emuldata not found.\n"));
		pem->persona = 0;
		pem->oom_score_adj = 0;
	}
}

void
linux_on_exit(struct proc *p)
{
	struct linux_pemuldata *pem;
	struct thread *td = curthread;

	MPASS(SV_CURPROC_ABI() == SV_ABI_LINUX);

	LINUX_CTR3(proc_exit, "thread(%d) proc(%d) p %p",
	    td->td_tid, p->p_pid, p);

	pem = pem_find(p);
	if (pem == NULL)
		return;
	(p->p_sysent->sv_thread_detach)(td);

	p->p_emuldata = NULL;

	sx_destroy(&pem->pem_sx);
	free(pem, M_LINUX);
}

/*
 * If a Linux binary is exec'ing something, try this image activator
 * first.  We override standard shell script execution in order to
 * be able to modify the interpreter path.  We only do this if a Linux
 * binary is doing the exec, so we do not create an EXEC module for it.
 */
int
linux_exec_imgact_try(struct image_params *imgp)
{
	const char *head = (const char *)imgp->image_header;
	char *rpath;
	int error = -1;

	/*
	 * The interpreter for shell scripts run from a Linux binary needs
	 * to be located in /compat/linux if possible in order to recursively
	 * maintain Linux path emulation.
	 */
	if (((const short *)head)[0] == SHELLMAGIC) {
		/*
		 * Run our normal shell image activator.  If it succeeds attempt
		 * to use the alternate path for the interpreter.  If an
		 * alternate path is found, use our stringspace to store it.
		 */
		if ((error = exec_shell_imgact(imgp)) == 0) {
			linux_emul_convpath(imgp->interpreter_name, UIO_SYSSPACE,
			    &rpath, 0, AT_FDCWD);
			if (rpath != NULL)
				imgp->args->fname_buf =
				    imgp->interpreter_name = rpath;
		}
	}
	return (error);
}

int
linux_common_execve(struct thread *td, struct image_args *eargs)
{
	struct linux_pemuldata *pem;
	struct vmspace *oldvmspace;
	struct linux_emuldata *em;
	struct proc *p;
	int error;

	p = td->td_proc;

	error = pre_execve(td, &oldvmspace);
	if (error != 0)
		return (error);

	error = kern_execve(td, eargs, NULL, oldvmspace);
	post_execve(td, error, oldvmspace);
	if (error != EJUSTRETURN)
		return (error);

	/*
	 * In a case of transition from Linux binary execing to
	 * FreeBSD binary we destroy Linux emuldata thread & proc entries.
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

		free(em, M_TEMP);
		free(pem, M_LINUX);
	}
	return (EJUSTRETURN);
}

void
linux_on_exec(struct proc *p, struct image_params *imgp)
{
	struct thread *td;
	struct thread *othertd;
#if defined(__amd64__)
	struct linux_pemuldata *pem;
#endif

	td = curthread;
	MPASS((imgp->sysent->sv_flags & SV_ABI_MASK) == SV_ABI_LINUX);

	/*
	 * When execing to Linux binary, we create Linux emuldata
	 * thread entry.
	 */
	if (SV_PROC_ABI(p) == SV_ABI_LINUX) {
		/*
		 * Process already was under Linuxolator
		 * before exec.  Update emuldata to reflect
		 * single-threaded cleaned state after exec.
		 */
		linux_proc_init(td, NULL, false);
	} else {
		/*
		 * We are switching the process to Linux emulator.
		 */
		linux_proc_init(td, td, false);

		/*
		 * Create a transient td_emuldata for all suspended
		 * threads, so that p->p_sysent->sv_thread_detach() ==
		 * linux_thread_detach() can find expected but unused
		 * emuldata.
		 */
		FOREACH_THREAD_IN_PROC(td->td_proc, othertd) {
			if (othertd == td)
				continue;
			linux_proc_init(td, othertd, true);
		}
	}
#if defined(__amd64__)
	/*
	 * An IA32 executable which has executable stack will have the
	 * READ_IMPLIES_EXEC personality flag set automatically.
	 */
	if (SV_PROC_FLAG(td->td_proc, SV_ILP32) &&
	    imgp->stack_prot & VM_PROT_EXECUTE) {
		pem = pem_find(p);
		pem->persona |= LINUX_READ_IMPLIES_EXEC;
	}
#endif
}

void
linux_thread_dtor(struct thread *td)
{
	struct linux_emuldata *em;

	em = em_find(td);
	if (em == NULL)
		return;
	td->td_emuldata = NULL;

	LINUX_CTR1(thread_dtor, "thread(%d)", em->em_tid);

	free(em, M_TEMP);
}

void
linux_schedtail(struct thread *td)
{
	struct linux_emuldata *em;
#ifdef KTR
	int error;
#else
	int error __unused;
#endif
	int *child_set_tid;

	em = em_find(td);
	KASSERT(em != NULL, ("linux_schedtail: thread emuldata not found.\n"));
	child_set_tid = em->child_set_tid;

	if (child_set_tid != NULL) {
		error = copyout(&em->em_tid, child_set_tid,
		    sizeof(em->em_tid));
		LINUX_CTR4(schedtail, "thread(%d) %p stored %d error %d",
		    td->td_tid, child_set_tid, em->em_tid, error);
	} else
		LINUX_CTR1(schedtail, "thread(%d)", em->em_tid);
}
