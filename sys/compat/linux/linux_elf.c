/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2021 Edward Tomasz Napierala <trasz@FreeBSD.org>
 * Copyright (c) 2018 Chuck Tuffli
 * Copyright (c) 2017 Dell EMC
 * Copyright (c) 2000 David O'Brien
 * Copyright (c) 1995-1996 Søren Schmidt
 * Copyright (c) 1996 Peter Wemm
 * All rights reserved.
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory as part of the CHERI for Hypervisors and Operating Systems
 * (CHaOS) project, funded by EPSRC grant EP/V000292/1.
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

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/reg.h>
#include <sys/sbuf.h>
#include <sys/sysent.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/elf.h>

#ifdef COMPAT_LINUX32
#define linux_pt_regset linux_pt_regset32
#define bsd_to_linux_regset bsd_to_linux_regset32
#include <machine/../linux32/linux.h>
#else
#include <machine/../linux/linux.h>
#endif
#include <compat/linux/linux_elf.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>

struct l_elf_siginfo {
	l_int		si_signo;
	l_int		si_code;
	l_int		si_errno;
};

typedef struct linux_pt_regset l_elf_gregset_t;

struct linux_elf_prstatus {
	struct l_elf_siginfo pr_info;
	l_short		pr_cursig;
	l_ulong		pr_sigpend;
	l_ulong		pr_sighold;
	l_pid_t		pr_pid;
	l_pid_t		pr_ppid;
	l_pid_t		pr_pgrp;
	l_pid_t		pr_sid;
	l_timeval	pr_utime;
	l_timeval	pr_stime;
	l_timeval	pr_cutime;
	l_timeval	pr_cstime;
	l_elf_gregset_t	pr_reg;
	l_int		pr_fpvalid;
};

#define	LINUX_NT_AUXV	6

static void __linuxN(note_fpregset)(void *, struct sbuf *, size_t *);
static void __linuxN(note_prpsinfo)(void *, struct sbuf *, size_t *);
static void __linuxN(note_prstatus)(void *, struct sbuf *, size_t *);
static void __linuxN(note_threadmd)(void *, struct sbuf *, size_t *);
static void __linuxN(note_nt_auxv)(void *, struct sbuf *, size_t *);

void
__linuxN(prepare_notes)(struct thread *td, struct note_info_list *list,
    size_t *sizep)
{
	struct proc *p;
	struct thread *thr;
	size_t size;

	p = td->td_proc;
	size = 0;

	/*
	 * To have the debugger select the right thread (LWP) as the initial
	 * thread, we dump the state of the thread passed to us in td first.
	 * This is the thread that causes the core dump and thus likely to
	 * be the right thread one wants to have selected in the debugger.
	 */
	thr = td;
	while (thr != NULL) {
		size += __elfN(register_note)(td, list,
		    NT_PRSTATUS, __linuxN(note_prstatus), thr);
		size += __elfN(register_note)(td, list,
		    NT_PRPSINFO, __linuxN(note_prpsinfo), p);
		size += __elfN(register_note)(td, list,
		    LINUX_NT_AUXV, __linuxN(note_nt_auxv), p);
		size += __elfN(register_note)(td, list,
		    NT_FPREGSET, __linuxN(note_fpregset), thr);
		size += __elfN(register_note)(td, list,
		    -1, __linuxN(note_threadmd), thr);

		thr = thr == td ? TAILQ_FIRST(&p->p_threads) :
		    TAILQ_NEXT(thr, td_plist);
		if (thr == td)
			thr = TAILQ_NEXT(thr, td_plist);
	}

	*sizep = size;
}

typedef struct linux_elf_prstatus linux_elf_prstatus_t;
#ifdef COMPAT_LINUX32
typedef struct prpsinfo32 linux_elf_prpsinfo_t;
typedef struct fpreg32 linux_elf_prfpregset_t;
#else
typedef prpsinfo_t linux_elf_prpsinfo_t;
typedef prfpregset_t linux_elf_prfpregset_t;
#endif

static void
__linuxN(note_prpsinfo)(void *arg, struct sbuf *sb, size_t *sizep)
{
	struct sbuf sbarg;
	size_t len;
	char *cp, *end;
	struct proc *p;
	linux_elf_prpsinfo_t *psinfo;
	int error;

	p = arg;
	if (sb != NULL) {
		KASSERT(*sizep == sizeof(*psinfo), ("invalid size"));
		psinfo = malloc(sizeof(*psinfo), M_TEMP, M_ZERO | M_WAITOK);
		psinfo->pr_version = PRPSINFO_VERSION;
		psinfo->pr_psinfosz = sizeof(linux_elf_prpsinfo_t);
		strlcpy(psinfo->pr_fname, p->p_comm, sizeof(psinfo->pr_fname));
		PROC_LOCK(p);
		if (p->p_args != NULL) {
			len = sizeof(psinfo->pr_psargs) - 1;
			if (len > p->p_args->ar_length)
				len = p->p_args->ar_length;
			memcpy(psinfo->pr_psargs, p->p_args->ar_args, len);
			PROC_UNLOCK(p);
			error = 0;
		} else {
			_PHOLD(p);
			PROC_UNLOCK(p);
			sbuf_new(&sbarg, psinfo->pr_psargs,
			    sizeof(psinfo->pr_psargs), SBUF_FIXEDLEN);
			error = proc_getargv(curthread, p, &sbarg);
			PRELE(p);
			if (sbuf_finish(&sbarg) == 0) {
				len = sbuf_len(&sbarg) - 1;
				if (len > 0)
					len--;
			} else {
				len = sizeof(psinfo->pr_psargs) - 1;
			}
			sbuf_delete(&sbarg);
		}
		if (error != 0 || len == 0 || (ssize_t)len == -1)
			strlcpy(psinfo->pr_psargs, p->p_comm,
			    sizeof(psinfo->pr_psargs));
		else {
			KASSERT(len < sizeof(psinfo->pr_psargs),
			    ("len is too long: %zu vs %zu", len,
			    sizeof(psinfo->pr_psargs)));
			cp = psinfo->pr_psargs;
			end = cp + len - 1;
			for (;;) {
				cp = memchr(cp, '\0', end - cp);
				if (cp == NULL)
					break;
				*cp = ' ';
			}
		}
		psinfo->pr_pid = p->p_pid;
		sbuf_bcat(sb, psinfo, sizeof(*psinfo));
		free(psinfo, M_TEMP);
	}
	*sizep = sizeof(*psinfo);
}

static void
__linuxN(note_prstatus)(void *arg, struct sbuf *sb, size_t *sizep)
{
	struct thread *td;
	linux_elf_prstatus_t *status;
#ifdef COMPAT_LINUX32
	struct reg32 pr_reg;
#else
	struct reg pr_reg;
#endif

	td = arg;
	if (sb != NULL) {
		KASSERT(*sizep == sizeof(*status), ("invalid size"));
		status = malloc(sizeof(*status), M_TEMP, M_ZERO | M_WAITOK);

		/*
		 * XXX: Some fields missing.
		 */
		status->pr_cursig = td->td_proc->p_sig;
		status->pr_pid = td->td_tid;

#ifdef COMPAT_LINUX32
		fill_regs32(td, &pr_reg);
#else
		fill_regs(td, &pr_reg);
#endif
		bsd_to_linux_regset(&pr_reg, &status->pr_reg);
		sbuf_bcat(sb, status, sizeof(*status));
		free(status, M_TEMP);
	}
	*sizep = sizeof(*status);
}

static void
__linuxN(note_fpregset)(void *arg, struct sbuf *sb, size_t *sizep)
{
	struct thread *td;
	linux_elf_prfpregset_t *fpregset;

	td = arg;
	if (sb != NULL) {
		KASSERT(*sizep == sizeof(*fpregset), ("invalid size"));
		fpregset = malloc(sizeof(*fpregset), M_TEMP, M_ZERO | M_WAITOK);
#ifdef COMPAT_LINUX32
		fill_fpregs32(td, fpregset);
#else
		fill_fpregs(td, fpregset);
#endif
		sbuf_bcat(sb, fpregset, sizeof(*fpregset));
		free(fpregset, M_TEMP);
	}
	*sizep = sizeof(*fpregset);
}

/*
 * Allow for MD specific notes, as well as any MD
 * specific preparations for writing MI notes.
 */
static void
__linuxN(note_threadmd)(void *arg, struct sbuf *sb, size_t *sizep)
{
	struct thread *td;
	void *buf;
	size_t size;

	td = arg;
	size = *sizep;
	if (size != 0 && sb != NULL)
		buf = malloc(size, M_TEMP, M_ZERO | M_WAITOK);
	else
		buf = NULL;
	size = 0;
	__elfN(dump_thread)(td, buf, &size);
	KASSERT(sb == NULL || *sizep == size, ("invalid size"));
	if (size != 0 && sb != NULL)
		sbuf_bcat(sb, buf, size);
	free(buf, M_TEMP);
	*sizep = size;
}

static void
__linuxN(note_nt_auxv)(void *arg, struct sbuf *sb, size_t *sizep)
{
	struct proc *p;
	size_t size;

	p = arg;
	if (sb == NULL) {
		size = 0;
		sb = sbuf_new(NULL, NULL, LINUX_AT_COUNT * sizeof(Elf_Auxinfo),
		    SBUF_FIXEDLEN);
		sbuf_set_drain(sb, sbuf_count_drain, &size);
		PHOLD(p);
		proc_getauxv(curthread, p, sb);
		PRELE(p);
		sbuf_finish(sb);
		sbuf_delete(sb);
		*sizep = size;
	} else {
		PHOLD(p);
		proc_getauxv(curthread, p, sb);
		PRELE(p);
	}
}

/*
 * Copy strings out to the new process address space, constructing new arg
 * and env vector tables. Return a pointer to the base so that it can be used
 * as the initial stack pointer.
 */
int
__linuxN(copyout_strings)(struct image_params *imgp, uintptr_t *stack_base)
{
	char canary[LINUX_AT_RANDOM_LEN];
	char **vectp;
	char *stringp;
	uintptr_t destp, ustringp;
	struct ps_strings *arginfo;
	struct proc *p;
	size_t execpath_len;
	int argc, envc;
	int error;

	p = imgp->proc;
	destp =	PROC_PS_STRINGS(p);
	arginfo = imgp->ps_strings = (void *)destp;

	/*
	 * Copy the image path for the rtld.
	 */
	if (imgp->execpath != NULL && imgp->auxargs != NULL) {
		execpath_len = strlen(imgp->execpath) + 1;
		destp -= execpath_len;
		destp = rounddown2(destp, sizeof(void *));
		imgp->execpathp = (void *)destp;
		error = copyout(imgp->execpath, imgp->execpathp, execpath_len);
		if (error != 0)
			return (error);
	}

	/*
	 * Prepare the canary for SSP.
	 */
	arc4rand(canary, sizeof(canary), 0);
	destp -= sizeof(canary);
	imgp->canary = (void *)destp;
	error = copyout(canary, imgp->canary, sizeof(canary));
	if (error != 0)
		return (error);
	imgp->canarylen = sizeof(canary);

	/*
	 * Allocate room for the argument and environment strings.
	 */
	destp -= ARG_MAX - imgp->args->stringspace;
	destp = rounddown2(destp, sizeof(void *));
	ustringp = destp;

	if (imgp->auxargs) {
		/*
		 * Allocate room on the stack for the ELF auxargs
		 * array.  It has up to LINUX_AT_COUNT entries.
		 */
		destp -= LINUX_AT_COUNT * sizeof(Elf_Auxinfo);
		destp = rounddown2(destp, sizeof(void *));
	}

	vectp = (char **)destp;

	/*
	 * Allocate room for the argv[] and env vectors including the
	 * terminating NULL pointers.
	 */
	vectp -= imgp->args->argc + 1 + imgp->args->envc + 1;

	/*
	 * Starting with 2.24, glibc depends on a 16-byte stack alignment.
	 */
	vectp = (char **)((((uintptr_t)vectp + 8) & ~0xF) - 8);

	/*
	 * vectp also becomes our initial stack base
	 */
	*stack_base = (uintptr_t)vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	error = copyout(stringp, (void *)ustringp,
	    ARG_MAX - imgp->args->stringspace);
	if (error != 0)
		return (error);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	imgp->argv = vectp;
	if (suword(&arginfo->ps_argvstr, (long)(intptr_t)vectp) != 0 ||
	    suword32(&arginfo->ps_nargvstr, argc) != 0)
		return (EFAULT);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		if (suword(vectp++, ustringp) != 0)
			return (EFAULT);
		while (*stringp++ != 0)
			ustringp++;
		ustringp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	if (suword(vectp++, 0) != 0)
		return (EFAULT);

	imgp->envv = vectp;
	if (suword(&arginfo->ps_envstr, (long)(intptr_t)vectp) != 0 ||
	    suword32(&arginfo->ps_nenvstr, envc) != 0)
		return (EFAULT);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		if (suword(vectp++, ustringp) != 0)
			return (EFAULT);
		while (*stringp++ != 0)
			ustringp++;
		ustringp++;
	}

	/* end of vector table is a null pointer */
	if (suword(vectp, 0) != 0)
		return (EFAULT);

	if (imgp->auxargs) {
		vectp++;
		error = imgp->sysent->sv_copyout_auxargs(imgp,
		    (uintptr_t)vectp);
		if (error != 0)
			return (error);
	}

	return (0);
}

bool
linux_trans_osrel(const Elf_Note *note, int32_t *osrel)
{
	const Elf32_Word *desc;
	uintptr_t p;

	p = (uintptr_t)(note + 1);
	p += roundup2(note->n_namesz, sizeof(Elf32_Addr));

	desc = (const Elf32_Word *)p;
	if (desc[0] != GNU_ABI_LINUX)
		return (false);
	/*
	 * For Linux we encode osrel using the Linux convention of
	 * 	(version << 16) | (major << 8) | (minor)
	 * See macro in linux_mib.h
	 */
	*osrel = LINUX_KERNVER(desc[1], desc[2], desc[3]);

	return (true);
}

int
__linuxN(copyout_auxargs)(struct image_params *imgp, uintptr_t base)
{
	struct thread *td = curthread;
	Elf_Auxargs *args;
	Elf_Auxinfo *aarray, *pos;
	struct proc *p;
	int error, issetugid;

	p = imgp->proc;
	issetugid = p->p_flag & P_SUGID ? 1 : 0;
	args = imgp->auxargs;
	aarray = pos = malloc(LINUX_AT_COUNT * sizeof(*pos), M_TEMP,
	    M_WAITOK | M_ZERO);

	__linuxN(arch_copyout_auxargs)(imgp, &pos);
	/*
	 * Do not export AT_CLKTCK when emulating Linux kernel prior to 2.4.0,
	 * as it has appeared in the 2.4.0-rc7 first time.
	 * Being exported, AT_CLKTCK is returned by sysconf(_SC_CLK_TCK),
	 * glibc falls back to the hard-coded CLK_TCK value when aux entry
	 * is not present.
	 * Also see linux_times() implementation.
	 */
	if (linux_kernver(td) >= LINUX_KERNVER(2,4,0))
		AUXARGS_ENTRY(pos, LINUX_AT_CLKTCK, stclohz);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_UID, imgp->proc->p_ucred->cr_ruid);
	AUXARGS_ENTRY(pos, AT_EUID, imgp->proc->p_ucred->cr_svuid);
	AUXARGS_ENTRY(pos, AT_GID, imgp->proc->p_ucred->cr_rgid);
	AUXARGS_ENTRY(pos, AT_EGID, imgp->proc->p_ucred->cr_svgid);
	AUXARGS_ENTRY(pos, LINUX_AT_SECURE, issetugid);
	if (linux_kernver(td) >= LINUX_KERNVER(2,6,30))
		AUXARGS_ENTRY_PTR(pos, LINUX_AT_RANDOM, imgp->canary);
	if (linux_kernver(td) >= LINUX_KERNVER(2,6,26) && imgp->execpathp != 0)
		AUXARGS_ENTRY(pos, LINUX_AT_EXECFN, PTROUT(imgp->execpathp));
	if (args->execfd != -1)
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	if (linux_kernver(td) >= LINUX_KERNVER(5,13,0))
		AUXARGS_ENTRY(pos, LINUX_AT_MINSIGSTKSZ,
		    imgp->sysent->sv_minsigstksz);
	AUXARGS_ENTRY(pos, AT_NULL, 0);

	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;
	KASSERT(pos - aarray <= LINUX_AT_COUNT, ("Too many auxargs"));

	error = copyout(aarray, PTRIN(base), sizeof(*aarray) * LINUX_AT_COUNT);
	free(aarray, M_TEMP);
	return (error);
}
