/*-
 * Copyright (c) 2003-2006, Joseph Koshy
 * All rights reserved.
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
 *
 * $FreeBSD: src/sys/sys/pmckern.h,v 1.6 2006/03/26 12:20:54 jkoshy Exp $
 */

/*
 * PMC interface used by the base kernel.
 */

#ifndef _SYS_PMCKERN_H_
#define _SYS_PMCKERN_H_

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/sx.h>

#define	PMC_FN_PROCESS_EXEC		1
#define	PMC_FN_CSW_IN			2
#define	PMC_FN_CSW_OUT			3
#define	PMC_FN_DO_SAMPLES		4
#define	PMC_FN_KLD_LOAD			5
#define	PMC_FN_KLD_UNLOAD		6
#define	PMC_FN_MMAP			7
#define	PMC_FN_MUNMAP			8

struct pmckern_procexec {
	int		pm_credentialschanged;
	uintfptr_t	pm_entryaddr;
};

struct pmckern_map_in {
	void		*pm_file;	/* filename or vnode pointer */
	uintfptr_t	pm_address;	/* address object is loaded at */
};

struct pmckern_map_out {
	uintfptr_t	pm_address;	/* start address of region */
	size_t		pm_size;	/* size of unmapped region */
};

/* hook */
extern int (*pmc_hook)(struct thread *_td, int _function, void *_arg);
extern int (*pmc_intr)(int _cpu, uintptr_t _pc, int _usermode);

/* SX lock protecting the hook */
extern struct sx pmc_sx;

/* Per-cpu flags indicating availability of sampling data */
extern volatile cpumask_t pmc_cpumask;

/* Count of system-wide sampling PMCs in existence */
extern volatile int pmc_ss_count;

/* kernel version number */
extern const int pmc_kernel_version;

/* Hook invocation; for use within the kernel */
#define	PMC_CALL_HOOK(t, cmd, arg)		\
do {						\
	sx_slock(&pmc_sx);			\
	if (pmc_hook != NULL)			\
		(pmc_hook)((t), (cmd), (arg));	\
	sx_sunlock(&pmc_sx);			\
} while (0)

/* Hook invocation that needs an exclusive lock */
#define	PMC_CALL_HOOK_X(t, cmd, arg)		\
do {						\
	sx_xlock(&pmc_sx);			\
	if (pmc_hook != NULL)			\
		(pmc_hook)((t), (cmd), (arg));	\
	sx_xunlock(&pmc_sx);			\
} while (0)

/*
 * Some hook invocations (e.g., from context switch and clock handling
 * code) need to be lock-free.
 */
#define	PMC_CALL_HOOK_UNLOCKED(t, cmd, arg)	\
do {						\
	if (pmc_hook != NULL)			\
		(pmc_hook)((t), (cmd), (arg));	\
} while (0)

#define	PMC_SWITCH_CONTEXT(t,cmd)	PMC_CALL_HOOK_UNLOCKED(t,cmd,NULL)

/* Check if a process is using HWPMCs.*/
#define PMC_PROC_IS_USING_PMCS(p)				\
	(__predict_false(atomic_load_acq_int(&(p)->p_flag) &	\
	    P_HWPMC))

#define	PMC_SYSTEM_SAMPLING_ACTIVE()		(pmc_ss_count > 0)

/* Check if a CPU has recorded samples. */
#define	PMC_CPU_HAS_SAMPLES(C)	(__predict_false(pmc_cpumask & (1 << (C))))

/* helper functions */
int	pmc_cpu_is_disabled(int _cpu);
int	pmc_cpu_is_logical(int _cpu);

#endif /* _SYS_PMCKERN_H_ */
