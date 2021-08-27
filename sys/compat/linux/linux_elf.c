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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/sbuf.h>
#include <sys/sysent.h>
#include <sys/user.h>
#include <sys/vnode.h>

#include <machine/elf.h>

#if __ELF_WORD_SIZE == 32
#define linux_pt_regset linux_pt_regset32
#define bsd_to_linux_regset bsd_to_linux_regset32
#include <machine/../linux32/linux.h>
#else
#include <machine/../linux/linux.h>
#endif
#include <compat/linux/linux_elf.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_misc.h>

/* This adds "linux32_" and "linux64_" prefixes. */
#define	__linuxN(x)	__CONCAT(__CONCAT(__CONCAT(linux,__ELF_WORD_SIZE),_),x)

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
#if __ELF_WORD_SIZE == 32
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
			if (sbuf_finish(&sbarg) == 0)
				len = sbuf_len(&sbarg) - 1;
			else
				len = sizeof(psinfo->pr_psargs) - 1;
			sbuf_delete(&sbarg);
		}
		if (error || len == 0)
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
#if __ELF_WORD_SIZE == 32
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

#if __ELF_WORD_SIZE == 32
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
#if __ELF_WORD_SIZE == 32
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
