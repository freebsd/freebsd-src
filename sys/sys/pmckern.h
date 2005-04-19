/*-
 * Copyright (c) 2003, Joseph Koshy
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
 * $FreeBSD$
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

#define	PMC_FN_PROCESS_EXIT		1
#define	PMC_FN_PROCESS_EXEC		2
#define	PMC_FN_PROCESS_FORK		3
#define	PMC_FN_CSW_IN			4
#define	PMC_FN_CSW_OUT			5

/* hook */
extern int (*pmc_hook)(struct thread *_td, int _function, void *_arg);
extern int (*pmc_intr)(int cpu, uintptr_t pc);

/* SX lock protecting the hook */
extern struct sx pmc_sx;

/* hook invocation; for use within the kernel */
#define	PMC_CALL_HOOK(t, cmd, arg)		\
do {						\
	sx_slock(&pmc_sx);			\
	if (pmc_hook != NULL)			\
		(pmc_hook)((t), (cmd), (arg));	\
	sx_sunlock(&pmc_sx);			\
} while (0)

/* hook invocation that needs an exclusive lock */
#define	PMC_CALL_HOOK_X(t, cmd, arg)		\
do {						\
	sx_xlock(&pmc_sx);			\
	if (pmc_hook != NULL)			\
		(pmc_hook)((t), (cmd), (arg));	\
	sx_xunlock(&pmc_sx);			\
} while (0)

/* context switches cannot take locks */
#define	PMC_SWITCH_CONTEXT(t, cmd)		\
do {						\
	if (pmc_hook != NULL)			\
		(pmc_hook)((t), (cmd), NULL);	\
} while (0)


/*
 * check if a process is using HWPMCs.
 */

#define PMC_PROC_IS_USING_PMCS(p)				\
	(__predict_false(atomic_load_acq_int(&(p)->p_flag) &	\
	    P_HWPMC))

/* helper functions */
int	pmc_cpu_is_disabled(int _cpu);
int	pmc_cpu_is_logical(int _cpu);

#endif /* _SYS_PMCKERN_H_ */
